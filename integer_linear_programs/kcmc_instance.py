"""
KCMC_Instance Object
"""


import sys
import subprocess
from typing import List, Set, Tuple, Dict


# PAYLOAD ##############################################################################################################


# Get the regenerated instance from its key using the C++ interface
def get_serialized_instance(pois, sensors, sinks, area_side, sensor_coverage_radius, sensor_communication_radius, random_seed,
                            executable='/app/instance_regenerator'):
    out = subprocess.Popen(
        list(map(str, [
            executable,
            f'KCMC;{pois} {sensors} {sinks}; {area_side} {sensor_coverage_radius} {sensor_communication_radius}; {random_seed}; END'
        ])),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )
    stdout,stderr = out.communicate()
    assert (len(stdout) > 10) and (stderr is None), f'ERROR ON THE INSTANCE REGENERATOR.\nSTDOUT:{stdout}\n\nSTDERR:{stderr}'
    return stdout.decode().strip().splitlines()[0].strip()


# Get the regenerated instance from its key using the C++ interface
def get_preprocessing(pois, sensors, sinks, area_side, sensor_coverage_radius, sensor_communication_radius, random_seed,
                      kcmc_k, kcmc_m, executable='/app/optimizer'):

    # Run the C++ package
    instance_key = f'KCMC;{pois} {sensors} {sinks}; {area_side} {sensor_coverage_radius} {sensor_communication_radius};{random_seed};END'
    out = subprocess.Popen(
        list(map(str, [
            executable,
            instance_key,
            kcmc_k, kcmc_m
        ])),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )

    # Parse the results
    stdout,stderr = out.communicate()
    assert (len(stdout) > 10) and (stderr is None), f'ERROR ON THE INSTANCE REGENERATOR.\nSTDOUT:{stdout}\n\nSTDERR:{stderr}'
    result = {}
    for line in stdout.decode().strip().splitlines():
        content = line.lower().strip().split('\t')[3:]
        assert len(content) == 6, f'INVALID LINE.\nSTDOUT:{stdout.decode()}\n\nSTDERR:{stderr}'
        item = dict(zip(['method', 'runtime_us', 'valid_result', 'num_used_sensors', 'compression_rate', 'solution'], content))

        # Normalize the item method
        if item['method'][-1].isdigit():
            item['method'], num_paths = item['method'].rsplit('_', 1)
            item['num_paths'] = int(num_paths)

        # Normalize the values and store
        item['valid_result'] = str(item['valid_result']).upper() == 'OK'
        item['runtime_us'] = int(item['runtime_us'])
        item['num_used_sensors'] = int(item['num_used_sensors'])
        item['compression_rate'] = float(item['compression_rate'])
        result[item['method']] = item.copy()
    return result


class KCMC_Instance(object):

    def __repr__(self): return f'<{self.key_str} {self.random_seed}>'

    def parse_serialized_instance(self, instance_string:str, inactive_sensors=None):

        self.string = instance_string.upper().strip()
        if inactive_sensors is None: inactive_sensors = set()

        assert self.string.startswith('KCMC;'), 'ALL INSTANCES MUST START WITH THE TAG <KCMC;>'
        assert self.string.endswith(';END'), 'ALL INSTANCES MUST END WITH THE TAG <;END>'

        # IDENTIFY AND UPGRADE STRING VERSION
        self.version = 0.1 if ';SK;' in self.string else 1.0
        if self.version == 0.1: self.string = self.string.replace('PS', 'PI').replace('SS', 'II').replace('SK', 'IS')

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
        self.inactive_sensors = inactive_sensors
        self._prep = None
        self._placements = None
        self.poi_sensor = {}
        self.sensor_poi = {}
        self.sensor_sensor = {}
        self.sink_sensor = {}
        self.sensor_sink = {}
        self.edges = {}

        tag = None
        is_expanded = False
        for i, token in enumerate(instance[4:-1]):
            if token in {'PI', 'II', 'IS'}:
                tag = token
                continue
            elif tag is None:
                raise AssertionError(f'INVALID TAG PARSING AT TOKEN {i+4} - {token}')

            alpha, beta = map(int, token.strip().split(' '))
            if tag == 'PI':
                if f'i{beta}' in inactive_sensors: continue  # Skip inactive sensors
                is_expanded = True
                self._add_to(self.edges, f'p{alpha}', f'i{beta}')
                self._add_to(self.poi_sensor, alpha, beta)
                self._add_to(self.sensor_poi, beta, alpha)
            elif tag == 'II':
                assert alpha != beta,  "SELF-DIRECTED EDGES ARE NOT SUPPORTED"
                if len({f'i{alpha}', f'i{beta}'}.intersection(inactive_sensors)) > 0: continue  # Skip inactive sensors
                is_expanded = True
                self._add_to(self.edges, f'i{alpha}', f'i{beta}')
                self._add_to(self.sensor_sensor, alpha, beta)
                self._add_to(self.sensor_sensor, beta, alpha)
            elif tag == 'IS':
                if f'i{alpha}' in inactive_sensors: continue  # Skip inactive sensors
                is_expanded = True
                self._add_to(self.edges, f'i{alpha}', f's{beta}')
                self._add_to(self.sensor_sink, alpha, beta)
                self._add_to(self.sink_sensor, beta, alpha)
            else: raise AssertionError(f'IMPOSSIBLE TAG {tag}')

        # If not expanded, regenerate the instance and re-parse
        if not is_expanded:
            self.parse_serialized_instance(get_serialized_instance(
                self.num_pois, self.num_sensors, self.num_sinks,
                self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius,
                self.random_seed
            ), inactive_sensors)

    def __init__(self, instance_string:str,
                 accept_loose_pois=False,
                 accept_loose_sensors=False,
                 accept_loose_sinks=False,
                 inactive_sensors=None):
        self.acceptance = (accept_loose_pois, accept_loose_sensors, accept_loose_sinks)

        # Parse the serialized instance
        self.parse_serialized_instance(instance_string, inactive_sensors)

        # Reading validations
        assert max(self.poi_sensor.keys()) < self.num_pois, f'INVALID POI IDs! {[p for p in self.poi_sensor.keys() if p > self.num_pois]}'
        assert max(self.sensor_sensor.keys()) < self.num_sensors, f'INVALID SENSOR IDs! {[p for p in self.sensor_sensor.keys() if p > self.num_sensors]}'
        assert max(self.sink_sensor.keys()) < self.num_sinks, f'INVALID SINK IDs! {[p for p in self.sink_sensor.keys() if p > self.num_sinks]}'

        # Optional validations
        assert ((len(self.poi_sensor) == self.num_pois) or accept_loose_pois), \
            f'INVALID NUMBER OF POIS ({self.num_pois} {len(self.poi_sensor)})'
        assert ((len(self.sink_sensor) == self.num_sinks) or accept_loose_sinks), \
            f'INVALID NUMBER OF SINKS ({self.num_sinks} {len(self.sink_sensor)})'
        assert ((len(self.sensor_sensor) == self.num_sensors) or accept_loose_sensors), \
            f'INVALID NUMBER OF SENSORS ({self.num_sensors} {len(self.sensor_sensor)} {[i for i in range(self.num_sensors) if i not in self.sensor_sensor]})'

    # BASIC PROPERTIES ############################################################################

    @property
    def key(self) -> tuple:
        return self.num_pois, self.num_sensors, self.num_sinks, \
               self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius

    @property
    def key_str(self) -> str:
        p,i,s, a,cv,cm, = self.key
        return f'KCMC;{p} {i} {s};{a} {cv} {cm};{self.random_seed};END'

    @property
    def is_single_sink(self) -> bool: return self.num_sinks == 1

    @property
    def pois(self) -> List[str]: return [f'p{p}' for p in self.poi_sensor.keys()]

    @property
    def poi_degree(self) -> Dict[str, int]: return {f'p{p}': len(i) for p, i in self.poi_sensor.items()}

    @property
    def poi_edges(self) -> List[Tuple[str, str]]: return [(f'p{p}', f'i{i}') for p, sensors in self.poi_sensor.items() for i in sensors]

    @property
    def sensors(self) -> List[str]: return sorted(list(self.sensor_degree.keys()))

    @property
    def sensor_degree(self) -> Dict[str, int]: return {f'i{p}': len(i) for p, i in self.sensor_sensor.items()}

    @property
    def sensor_edges(self) -> List[Tuple[str, str]]: return [(f'i{i}', f'i{ii}') for i, sensors in self.sensor_sensor.items() for ii in sensors]

    @property
    def sinks(self) -> List[str]: return [f's{k}' for k in self.sink_sensor]

    @property
    def sink_degree(self) -> Dict[str, int]: return {f's{p}': len(i) for p, i in self.sink_sensor.items()}

    @property
    def sink_edges(self) -> List[Tuple[str, str]]: return [(f'i{i}', f's{s}') for i, sinks in self.sensor_sink.items() for s in sinks]

    @property
    def dual_edges(self):
        return sorted(list(set(
            [(a, b) for a, l in self.edges.items() for b in l if (a != b)]
          + [(b, a) for a, l in self.edges.items() for b in l if (a != b)]
        )))

    @property
    def linear_edges(self):
        edges = set()
        for a, b in self.dual_edges:
            if (a, b) not in edges:
                if (b, a) not in edges:
                    edges.add((a, b))
        return sorted(list(edges))

    @property
    def coverage_graph(self) -> Dict[str, Set[str]]: return {f'i{i}': set([f'p{p}' for p in pois]) for i, pois in self.sensor_poi.items()}

    @property
    def inverse_coverage_graph(self) -> Dict[str, Set[str]]: return {f'p{p}': set([f'i{i}' for i in sensors]) for p, sensors in self.poi_sensor.items()}

    @property
    def communication_graph(self) -> Dict[str, Set[str]]: return {f'i{i}': set([f'i{p}' for p in sensors]) for i, sensors in self.sensor_sensor.items()}

    # SERVICES ####################################################################################

    def preprocess(self, k, m, prep_method:str):

        # Memoize the preprocessing
        if self._prep is None:
            self._prep = get_preprocessing(
                self.num_pois, self.num_sensors, self.num_sinks,
                self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius,
                self.random_seed, k, m
            )

        # If we have a valid preprocessing:
        if (len(self._prep) > 0) and self._prep.get(prep_method, {}).get('valid_result', False):

            # Parse the raw result as a new instance
            inactive_sensors = {f'i{j}' for j,i in enumerate(self._prep[prep_method]['solution']) if i == '0'}
            return KCMC_Instance(self.key_str, *self.acceptance, inactive_sensors=inactive_sensors)

        # If invalid preprocessing
        else:
            raise KeyError(f'Invalid preprocessing method on instance {self.key_str}: {prep_method}')

    @staticmethod
    def _add_to(_dict:dict, key, value):
        if key not in _dict:
            _dict[key] = set()
        _dict[key].add(value)


# RUNTIME ##############################################################################################################

if __name__ == '__main__':
    from collections import Counter
    print('Checking for possible isomorphism between different instances')

    def frequency_of_degree(vertice_degrees_dict:dict):
        return set(Counter([degree for vertice, degree in vertice_degrees_dict.items()]).items())

    # Parse the instances
    instances = []
    for line in sys.stdin:
        print(f'Comparing instance {1+len(instances)} with {len(instances)} others')

        # Parse the instance
        instance = KCMC_Instance(line.strip().split('|')[0].strip(), True, True, True)

        # Compare all previous instances with the current
        for other_instance in instances:

            # No possible isomorphism if different numbers of elements
            if other_instance.num_sinks != instance.num_sinks: continue
            if other_instance.num_pois != instance.num_pois: continue
            if other_instance.num_sensors != instance.num_sensors: continue

            # No possible isomorphism if different ranks of SINKs
            ranks_o = frequency_of_degree(other_instance.sink_degree)
            ranks_i = frequency_of_degree(instance.sink_degree)
            diff_oi = ranks_o - ranks_i
            if len(diff_oi) != 0: continue
            diff_io = ranks_i - ranks_o
            if len(diff_io) != 0: continue

            # No possible isomorphism if different ranks of POIs
            ranks_o = frequency_of_degree(other_instance.poi_degree)
            ranks_i = frequency_of_degree(instance.poi_degree)
            diff_oi = ranks_o - ranks_i
            if len(diff_oi) != 0: continue
            diff_io = ranks_i - ranks_o
            if len(diff_io) != 0: continue

            # No possible isomorphism if different ranks of Sensors
            ranks_o = frequency_of_degree(other_instance.sensor_degree)
            ranks_i = frequency_of_degree(instance.sensor_degree)
            diff_oi = ranks_o - ranks_i
            if len(diff_oi) != 0: continue
            diff_io = ranks_i - ranks_o
            if len(diff_io) != 0: continue

            # There might be isomorphism of both instances
            raise Exception(f'Possible isomorphism:\n\t{other_instance}\n\t{instance}')

        # If we got here, no possible isomorfism! Yaaay!
        # Add the current instance in the list for checking against isomorphism with further instances
        instances.append(instance)

    print(f'NO POSSIBLE ISOMORPHISM IN {len(instances)} INSTANCES!')
