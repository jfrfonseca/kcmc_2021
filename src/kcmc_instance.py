"""
KCMC_Instance Object
"""

from queue import SimpleQueue as Queue
from typing import List



class KCMC_Instance(object):

    def __init__(self, instance_string:str, accept_loose_pois=False, accept_loose_sensors=False,accept_loose_sinks=False):
        self.furthest_cache = {'poi': {}, 'sensor': {}, 'sink': {}}
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

    # ADVANCED PROPERTIES ##############################################################################################

    @property
    def poi_degree(self) -> List[int]: return [len(v) for v in self.poi_sensor.values()]

    @property
    def sensor_degree(self) -> List[int]: return [len(v) for v in self.sensor_sensor.values()]

    @property
    def sink_degree(self) -> List[int]: return [len(v) for v in self.sink_sensor.values()]

    @property
    def furthest_distance_poi(self) -> List[int]: return [self.furthest_sensor(v, 'poi') for v in self.poi_sensor.values()]

    @property
    def furthest_distance_sensor(self) -> List[int]: return [self.furthest_sensor(v, 'sensor') for v in self.sensor_sensor.values()]

    @property
    def furthest_distance_sink(self) -> List[int]: return [self.furthest_sensor(v, 'sink') for v in self.sensor_sensor.values()]

    # SERVICES #########################################################################################################

    @staticmethod
    def _add_to(_dict:dict, key:int, value:int):
        if key not in _dict:
            _dict[key] = set()
        _dict[key].add(value)

    def furthest_sensor(self, start_item:int, component_type:str):

        # Select datasource
        if component_type == 'poi':
            source = self.poi_sensor
        elif component_type == 'sensor':
            source = self.sensor_sensor
        elif component_type == 'sink':  # In this case, we have the distance from a sensor to any sink
            if start_item in self.sensor_sink: return 1
            source = self.sensor_sensor
        else: raise TypeError('Unknown component type '+component_type)

        # Check cache
        cache = self.furthest_cache[component_type].get(start_item, None)
        if cache is not None: return cache

        # Prepare buffers
        visited = set()
        layer = source[start_item]
        next_layer = set()
        num_layers = 0
        sink_found = False
        look_for_sink = (component_type == 'sink')

        # BFS loop
        while len(layer) > 0:
            num_layers += 1
            for sensor in layer:
                visited.add(sensor)
                for neighbor in self.sensor_sensor[sensor]:

                    # If we're looking for a sink and found it, to not add new sensors to the next layer
                    if sink_found or (look_for_sink and (neighbor in self.sensor_sink)):
                        sink_found = True
                        continue

                    # If not, add unvisited neighbors out of the current layer to the next layer
                    if neighbor in visited.union(layer): continue
                    next_layer.add(neighbor)

            # If in the last layer already, cache all the results
            if len(next_layer) == 0:
                self.furthest_cache[component_type][start_item] = num_layers
                for sensor in layer:
                    self.furthest_cache[component_type][sensor] = num_layers
            layer = next_layer
        return num_layers


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
