"""
KCMC_Instance Object
"""


from typing import List, Tuple



class KCMC_Instance(object):

    def __repr__(self): return f'<{self.key_str} {self.random_seed} [{len(self.virtual_sinks)}]>'

    def __init__(self, instance_string:str, accept_loose_pois=False, accept_loose_sensors=False, accept_loose_sinks=False):
        self.string = instance_string.upper().strip()

        assert self.string.startswith('KCMC;'), 'ALL INSTANCES MUST START WITH THE TAG <KCMC;>'
        assert self.string.endswith(';END'), 'ALL INSTANCES MUST END WITH THE TAG <;END>'

        # Base-parse the string
        instance = self.string.split(';')

        # Parse the constants
        try:
            self.num_pois, self.num_sensors, self.num_sinks = instance[1].split(' ')
            self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius = instance[2].split(' ')
            self.random_seed = instance[3]
            self.num_pois, self.num_sensors, self.num_sinks, self.random_seed \
                = map(int, [self.num_pois, self.num_sensors, self.num_sinks, self.random_seed])
            self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius \
                = map(int, [self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius])
        except Exception as exp: raise AssertionError('INVALID INSTANCE PREAMBLE!')

        # Prepare the buffers
        self.poi_sensor = {}
        self.sensor_poi = {}
        self.sensor_sensor = {}
        self.sink_sensor = {}
        self.sensor_sink = {}
        self.virtual_sinks = []

        tag = None
        for i, token in enumerate(instance[4:-1]):
            if token in {'PS', 'SS', 'SK'}:
                tag = token
                continue
            elif tag is None:
                raise AssertionError(f'INVALID TAG PARSING AT TOKEN {i+4}')

            alpha, beta = map(int, token.strip().split(' '))
            if tag == 'PS':
                self._add_to(self.poi_sensor, alpha, beta)
                self._add_to(self.sensor_poi, beta, alpha)
            elif tag == 'SK':
                self._add_to(self.sensor_sink, alpha, beta)
                self._add_to(self.sink_sensor, beta, alpha)
            elif tag == 'SS':
                self._add_to(self.sensor_sensor, alpha, beta)
                self._add_to(self.sensor_sensor, beta, alpha)
            else: raise AssertionError(f'IMPOSSIBLE TAG {tag}')

        # Reading validations
        assert ((len(self.poi_sensor) == self.num_pois) or accept_loose_pois), \
            f'INVALID NUMBER OF POIS ({self.num_pois} {len(self.poi_sensor)})'
        assert ((len(self.sink_sensor) == self.num_sinks) or accept_loose_sinks), \
            f'INVALID NUMBER OF SINKS ({self.num_sinks} {len(self.sink_sensor)})'
        assert ((len(self.sensor_sensor) == self.num_sensors) or accept_loose_sensors), \
            f'INVALID NUMBER OF SENSORS ({self.num_sensors} {len(self.sensor_sensor)} {[i for i in range(self.num_sensors) if i not in self.sensor_sensor]})'

    # BASIC PROPERTIES #################################################################################################

    @property
    def key(self) -> tuple:
        return self.num_pois, self.num_sensors, self.num_sinks, \
               self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius

    @property
    def key_str(self) -> str:
        return ':'.join(['KCMC'] + list(map(str, self.key)))

    @property
    def is_single_sink(self) -> bool: return self.num_sinks == 1

    @property
    def pois(self) -> List[str]: return list(map(str, sorted(self.poi_sensor.keys())))

    @property
    def sensors(self) -> List[str]: return list(map(str, sorted(self.sensor_sensor.keys())))

    @property
    def sinks(self) -> List[str]: return list(map(str, sorted(self.sink_sensor.keys())))

    @property
    def edges(self) -> List[Tuple[str, str]]:
        result = []
        for r in ([(f'p{p}', f's{s}') for p, sens in sorted(self.poi_sensor.items()) for s in sens]
                + [(f's{s}', f's{s}') for s, sens in sorted(self.sensor_sensor.items()) for s in sens]
                + [(f's{s}', f'k{k}') for s, sinks in sorted(self.sensor_sink.items()) for k in sinks]):
            if ((r[1], r[0]) not in result) and (r[0] != r[1]):
                result.append(r)
        return result

    # SERVICES #########################################################################################################

    @staticmethod
    def _add_to(_dict:dict, key:int, value:int):
        if key not in _dict:
            _dict[key] = set()
        _dict[key].add(value)

    def to_single_sink(self, MAX_M=10) -> object:
        if self.is_single_sink: return self

        # Start a new instance without any content
        result = KCMC_Instance(
            instance_string=';'.join([
                'KCMC',
                f'{self.num_pois} {self.num_sensors} 1',
                f'{self.area_side} {self.sensor_coverage_radius} {self.sensor_communication_radius}',
                f'{self.random_seed}',
                'END'
            ]),
            accept_loose_pois=True, accept_loose_sensors=True, accept_loose_sinks=True
        )

        # Set the present instance's serialized string and some of the buffers
        result.string = self.string
        result.poi_sensor = self.poi_sensor.copy()
        result.sensor_poi = self.sensor_poi.copy()
        result.sensor_sensor = self.sensor_sensor.copy()

        # Set the virtual sinks
        for sink, targets in self.sink_sensor.items():
            for i in range(MAX_M):
                virtual_sink = max(result.sensor_sensor.keys()) + 1
                result.virtual_sinks.append(virtual_sink)
                for sensor in targets:
                    result._add_to(result.sensor_sensor, virtual_sink, sensor)
                    result._add_to(result.sensor_sensor, sensor, virtual_sink)
                    result._add_to(result.sensor_sink, virtual_sink, 0)
                    result._add_to(result.sink_sensor, 0, virtual_sink)
        # result.num_sensors = result.num_sensors + len(result.virtual_sinks)

        # Return the new instance
        return result


# ######################################################################################################################
# RUNTIME


def parse_evaluation(evaluation):
    result = []
    for k_m, msg_k, msg_m in [i.strip().split(' | ')
                              for i in evaluation.strip().split(';') if len(i) > 1]:
        k, m = k_m.strip().split(' ')
        k_success = (msg_k.strip().upper() == 'SUCCESS')
        m_success = (msg_m.strip().upper() == 'SUCCESS')
        result.append({
            'K=' + k: k_success,
            'M=' + m: m_success,
        })
    return result


def parse_block(df):

    # Parse each evaluation as a list of dicts
    df.loc[:, 'evaluation'] = df['raw_evaluation'].apply(parse_evaluation)

    # Parse the instance as a KCMC_Instance object
    df.loc[:, 'obj_instance'] = df['instance'].apply(
        lambda instance: KCMC_Instance(instance,
                                       accept_loose_pois=True,
                                       accept_loose_sensors=True,
                                       accept_loose_sinks=True)
    )

    # Extract basic attributes of the instance
    df.loc[:, 'key'] = df['obj_instance'].apply(lambda i: i.key_str)
    df.loc[:, 'random_seed'] = df['obj_instance'].apply(lambda i: i.random_seed)
    df.loc[:, 'pois'] = df['obj_instance'].apply(lambda i: i.num_pois)
    df.loc[:, 'sensors'] = df['obj_instance'].apply(lambda i: i.num_sensors)
    df.loc[:, 'sinks'] = df['obj_instance'].apply(lambda i: i.num_sinks)
    df.loc[:, 'area_side'] = df['obj_instance'].apply(lambda i: i.area_side)
    df.loc[:, 'coverage_r'] = df['obj_instance'].apply(lambda i: i.sensor_coverage_radius)
    df.loc[:, 'communication_r'] = df['obj_instance'].apply(lambda i: i.sensor_communication_radius)

    # Extract attributes of the instance that cannot be calculated from other attributes

    # Reformat the dataframe
    df = df.explode('evaluation').reset_index(drop=True).copy()
    df = df.merge(pd.DataFrame(df['evaluation'].tolist(), index=df.index),
                  left_index=True, right_index=True)
    df = df.fillna(False).drop_duplicates(
        subset=(['key', 'random_seed'] + [col for col in df.columns if (col.startswith('K') or col.startswith('M'))])
    ).reset_index(drop=True)

    return df


def parse_key(instance_key):
    key = instance_key.replace('INSTANCE', 'KCMC').replace(':', '_')
    if os.path.exists(sys.argv[1] + '/' + key + '.pq'):
        return key, -1

    # If we have to reprocess, start our own redis connection and extract the data
    df = []
    redis = StrictRedis(sys.argv[3], decode_responses=True)
    evaluation_key = instance_key.replace('INSTANCE', 'EVALUATION')
    for random_seed, instance in redis.hscan_iter(instance_key):
        df.append({
            'instance': instance,
            'raw_evaluation': redis.hget(evaluation_key, random_seed)
        })
    redis.close()

    # With the connection closed, parse and save the data
    df = parse_block(pd.DataFrame(df))
    df.drop(columns=['obj_instance', 'evaluation']).copy().to_parquet(sys.argv[1] + f'/{key}.pq')
    return key, len(df)


if __name__ == "__main__":

    import sys, os, multiprocessing
    import pandas as pd
    from redis import StrictRedis

    redis = StrictRedis(sys.argv[3], decode_responses=True)
    list_keys = list(redis.scan_iter('INSTANCE:*'))
    redis.close()

    # Parse the REDIS data as a DataFrame
    pool = multiprocessing.Pool(int(sys.argv[2]))
    for num, pair in enumerate(pool.imap_unordered(parse_key, list_keys)):
        key, qtd = pair
        print(round(num / len(list_keys), 3), '\t', qtd, '\t', key)
    pool.close()
