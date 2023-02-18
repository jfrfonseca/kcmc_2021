"""
KCMC_Instance Object
"""


import sys
import subprocess
from typing import List, Set, Tuple, Dict


REGENERATOR_EXECUTABLE = '/app/instance_regenerator'


# PAYLOAD ##############################################################################################################


class KCMC_Instance(object):
    string: str = None
    active_sensors: Set[str] = set()

    def __repr__(self): return f'<{self.key_str}>'
    def __str__(self): return self.__repr__()
    def __hash__(self): return self.__repr__().__hash__()

    def __parse_serialized_instance(self, instance_string:str, active_sensors:Set[str]=None):

        # Store and validate the instance string
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
        self._prep = None
        self._placements = None
        self.poi_sensor = {}
        self.sensor_poi = {}
        self.sensor_sensor = {}
        self.sink_sensor = {}
        self.sensor_sink = {}
        self.edges = {}

        # Get the inactive sensors
        sensors = set([f'i{i}' for i in range(self.num_sensors)])
        if active_sensors is None: active_sensors = []
        if len(active_sensors) == 0: active_sensors = sensors
        self.active_sensors = set(active_sensors)
        assert self.active_sensors.issubset(sensors), f'SOME INACTIVE SENSORS DONT EXIST! {sorted(self.active_sensors - sensors)}'
        self.inactive_sensors = sensors - self.active_sensors

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
                is_expanded = True
                if f'i{beta}' in self.inactive_sensors: continue  # Skip inactive sensors
                self._add_to(self.edges, f'p{alpha}', f'i{beta}')
                self._add_to(self.poi_sensor, alpha, beta)
                self._add_to(self.sensor_poi, beta, alpha)
            elif tag == 'II':
                is_expanded = True
                assert alpha != beta,  "SELF-DIRECTED EDGES ARE NOT SUPPORTED"
                if len({f'i{alpha}', f'i{beta}'}.intersection(self.inactive_sensors)) > 0: continue  # Skip inactive sensors
                self._add_to(self.edges, f'i{alpha}', f'i{beta}')
                self._add_to(self.sensor_sensor, alpha, beta)
                self._add_to(self.sensor_sensor, beta, alpha)
            elif tag == 'IS':
                is_expanded = True
                if f'i{alpha}' in self.inactive_sensors: continue  # Skip inactive sensors
                self._add_to(self.edges, f'i{alpha}', f's{beta}')
                self._add_to(self.sensor_sink, alpha, beta)
                self._add_to(self.sink_sensor, beta, alpha)
            else: raise AssertionError(f'IMPOSSIBLE TAG {tag}')

        # If not expanded, regenerate the instance and re-parse
        if not is_expanded:
            instance_string = subprocess.Popen(
                [REGENERATOR_EXECUTABLE, self.key_str],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT
            )
            stdout,stderr = instance_string.communicate()
            assert (len(stdout) > 10) and (stderr is None), f'ERROR ON THE INSTANCE REGENERATOR.\nSTDOUT:{stdout}\n\nSTDERR:{stderr}'
            instance_string = stdout.decode().strip().splitlines()[0].strip()

            self.__parse_serialized_instance(instance_string, self.active_sensors)

    def __init__(self, instance_string:str, active_sensors:Set[str]=None):

        # Parse the serialized instance
        self.__parse_serialized_instance(instance_string, active_sensors)

        # Reading validations
        assert max(self.poi_sensor.keys()) < self.num_pois, f'INVALID POI IDs! {[p for p in self.poi_sensor.keys() if p > self.num_pois]}'
        assert max(self.sensor_sensor.keys()) < self.num_sensors, f'INVALID SENSOR IDs! {[p for p in self.sensor_sensor.keys() if p > self.num_sensors]}'
        assert max(self.sink_sensor.keys()) < self.num_sinks, f'INVALID SINK IDs! {[p for p in self.sink_sensor.keys() if p > self.num_sinks]}'

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
    def coverage_graph(self) -> Dict[str, Set[str]]: return {f'p{p}': set([f'i{i}' for i in sensors]) for p, sensors in self.poi_sensor.items()}

    # SERVICES ####################################################################################

    @staticmethod
    def _add_to(_dict:dict, key, value):
        if key not in _dict:
            _dict[key] = set()
        _dict[key].add(value)

    def validate(self, kcmc_k:int, kcmc_m:int, *active_sensors) -> (bool, str):
        regenerator = subprocess.Popen(
            [REGENERATOR_EXECUTABLE, self.key_str, str(kcmc_k), str(kcmc_m)] + ['i'+str(int(i)) for i in active_sensors],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        stdout,stderr = regenerator.communicate()
        if stderr is None: return False, f'ERROR ON THE INSTANCE REGENERATOR.\nSTDOUT:{stdout}\n\nSTDERR:{stderr}'
        tikz = stdout.decode()
        return True, tikz


# RUNTIME ##############################################################################################################

if __name__ == '__main__':
    from collections import Counter
    print('Checking for possible isomorphism between different instances')

    def frequency_of_degree(vertice_degrees_dict:dict):
        return set(Counter([degree for vertice, degree in vertice_degrees_dict.items()]).items())

    # Parse the instances
    seeds = set()
    instances = []
    isomorphic_pairs = []
    instance_cohorts = {}
    for line in sys.stdin:
        print(f'Comparing instance {1+len(instances)} with {len(instances)} others (got {len(isomorphic_pairs)} possible isomorphic pairs)')

        # Parse the instance
        instance, k, m = line.strip().split('\t')
        instance = KCMC_Instance(instance.strip())
        seeds.add(instance.random_seed)

        # Get the instance's cohort
        cohort = str(instance).replace(f'{instance.random_seed};', '')+f' K={k} M={m}'
        if cohort not in instance_cohorts: instance_cohorts[cohort] = []
        instance_cohorts[cohort].append(instance)

        # Compare all previous instances with the current
        for other_instance in instances:

            # There definitely is isomorphism if the seeds are equal
            if instance.random_seed == other_instance.random_seed:
                isomorphic_pairs.append((instance, other_instance))
                continue

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
            isomorphic_pairs.append((instance, other_instance))

        # If we got here, no possible isomorfism! Yaaay!
        # Add the current instance in the list for checking against isomorphism with further instances
        instances.append(instance)

    print('COHORTS:')
    for cohort, cohort_instances in sorted(instance_cohorts.items()):
        isomorphic_pairs_in_cohort = [
            i for i, oi in isomorphic_pairs
            if ((i in cohort_instances) or (oi in cohort_instances))
        ]
        print(f'\t{len(cohort_instances)}-{len(isomorphic_pairs_in_cohort)}:\t{cohort}')

    if len(isomorphic_pairs) > 0:
        print(f'GOT {len(isomorphic_pairs)} ISOMORPHIC PAIRS!')
        for instance, other_instance in isomorphic_pairs:
            print(f'\tINSTANCE {instance} AND {other_instance}')
        exit(1)

    print(f'NO POSSIBLE ISOMORPHISM IN {len(instances)} INSTANCES AND {len(seeds)} SEEDS!')
