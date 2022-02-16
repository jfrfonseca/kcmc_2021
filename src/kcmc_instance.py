"""
KCMC_Instance Object
"""


from typing import List, Set, Tuple, Dict
try:
    import igraph
except Exception as exp:
    igraph = None


class KCMC_Instance(object):

    color_dict = {
        "p": "green",
        "i": "red",          "offline": "white",
        "s": "black",        "vsink": "grey",
        "tree_0": "blue",
        "tree_1": "orange",
        "tree_2": "yellow",
        "tree_3": "magenta",
        "tree_4": "cyan"
    }

    def __repr__(self): return f'<{self.key_str} {self.random_seed} [{len(self.virtual_sinks)}]>'

    def __init__(self, instance_string:str, accept_loose_pois=False, accept_loose_sensors=False, accept_loose_sinks=False):
        self.string = instance_string.upper().strip()

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
        self.poi_sensor = {}
        self.sensor_poi = {}
        self.sensor_sensor = {}
        self.sink_sensor = {}
        self.sensor_sink = {}
        self.edges = {}
        self.virtual_sinks_map = {}

        tag = None
        for i, token in enumerate(instance[4:-1]):
            if token in {'PI', 'II', 'IS'}:
                tag = token
                continue
            elif tag is None:
                raise AssertionError(f'INVALID TAG PARSING AT TOKEN {i+4} - {token}')

            alpha, beta = map(int, token.strip().split(' '))
            if tag == 'PI':
                self._add_to(self.edges, f'p{alpha}', f'i{beta}')
                self._add_to(self.poi_sensor, alpha, beta)
                self._add_to(self.sensor_poi, beta, alpha)
            elif tag == 'II':
                assert alpha != beta,  "SELF-DIRECTED EDGES ARE NOT SUPPORTED"
                self._add_to(self.edges, f'i{alpha}', f'i{beta}')
                self._add_to(self.sensor_sensor, alpha, beta)
                self._add_to(self.sensor_sensor, beta, alpha)
            elif tag == 'IS':
                self._add_to(self.edges, f'i{alpha}', f's{beta}')
                self._add_to(self.sensor_sink, alpha, beta)
                self._add_to(self.sink_sensor, beta, alpha)
            else: raise AssertionError(f'IMPOSSIBLE TAG {tag}')

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

    # BASIC PROPERTIES #################################################################################################

    @property
    def key(self) -> tuple:
        return self.num_pois, self.num_sensors, self.num_sinks, \
               self.area_side, self.sensor_coverage_radius, self.sensor_communication_radius

    @property
    def key_str(self) -> str:
        return ':'.join([f'KCMC_{self.version}'] + list(map(str, self.key)))

    @property
    def is_single_sink(self) -> bool: return self.num_sinks == 1

    @property
    def pois(self) -> List[str]: return [f'p{p}' for p in self.poi_sensor.keys()]

    @property
    def poi_degree(self) -> Dict[str, int]: return {f'p{p}': len(i) for p, i in self.poi_sensor.items()}

    @property
    def poi_edges(self) -> List[Tuple[str, str]]: return [(f'p{p}', f'i{i}') for p, sensors in self.poi_sensor.items() for i in sensors]

    @property
    def sensors(self) -> List[str]: return [f'i{s}' for s in range(0, self.num_sensors) if f'i{s}' not in self.connected_sensors]

    @property
    def sensor_degree(self) -> Dict[str, int]: return {f'i{p}': len(i) for p, i in self.sensor_sensor.items()}

    @property
    def original_sensors(self) -> Set[str]: return set([s for s in self.sensors if s not in self.virtual_sinks])

    @property
    def sensor_edges(self) -> List[Tuple[str, str]]: return [(f'i{i}', f'i{ii}') for i, sensors in self.sensor_sensor.items() for ii in sensors]

    @property
    def sinks(self) -> List[str]: return [f's{k}' for k in self.sink_sensor]

    @property
    def sink_degree(self) -> Dict[str, int]: return {f's{p}': len(i) for p, i in self.sink_sensor.items()}

    @property
    def sink_edges(self) -> List[Tuple[str, str]]: return [(f'i{i}', f's{s}') for i, sinks in self.sensor_sink.items() for s in sinks]

    @property
    def coverage_density(self) -> float: return (self.sensor_coverage_radius*self.num_sensors)/(self.area_side*self.area_side*self.num_pois)

    @property
    def communication_density(self) -> float: return (self.sensor_communication_radius*self.num_sensors*self.num_sinks)/(self.area_side*self.area_side)

    @property
    def virtual_sinks(self) -> Set[str]: return set([f'i{s}' for vsinks in self.virtual_sinks_map.values() for s in vsinks])

    @property
    def original_sinks(self) -> Set[str]: return set([f'i{s}' for s in self.virtual_sinks_map.keys()])

    @property
    def virtual_sinks_dict(self) -> Dict[str, str]:
        inverted_virtual_sinks_map = {}
        for osink, virtual_sinks in self.virtual_sinks_map.items():
            for vsink in virtual_sinks:
                inverted_virtual_sinks_map[f'i{vsink}'] = f's{osink}'
        return inverted_virtual_sinks_map

    @property
    def num_virtual_sinks(self) -> bool: return len(self.virtual_sinks)

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

    @property
    def disposable_sensors(self) -> Set[str]:
        # We consider DISPOSABLE all sensors that fill ALL this criteria:
        # - Do not connects to at least two other sensors
        # - Do not connects to at least one POI
        # - Do not connects to at least sink (direct-bridge sensors)
        if not hasattr(self, '_disposable_sensors'):
            self._disposable_sensors = {f'i{i}' for i in range(self.num_sensors)
                                        if all([len(self.sensor_sensor.get(i, [])) < 2,
                                                i not in self.sensor_poi,
                                                i not in self.sensor_sink])}
        return self._disposable_sensors

    @property
    def connected_sensors(self) -> Set[str]:
        # Sensors that have at least one connection to some other component
        if not hasattr(self, '_connected_sensors'):
            self._connected_sensors = {f'i{i}' for i in range(self.num_sensors)
                                        if all([i not in self.sensor_sensor,
                                                i not in self.sensor_poi,
                                                i not in self.sensor_sink])}
        return self._connected_sensors

    # SERVICES #########################################################################################################

    @staticmethod
    def _add_to(_dict:dict, key, value):
        if key not in _dict:
            _dict[key] = set()
        _dict[key].add(value)

    def to_single_sink(self, MAX_M=10):
        if self.is_single_sink: return self

        # For each sink and corresponding vsink
        vsinks_added = []
        virtual_sinks_map = {}
        new_sensor_sensor = {s: ls.copy() for s, ls in self.sensor_sensor.items()}
        for sink, sensors in self.sink_sensor.items():
            for vsink in range(self.num_sensors+(sink*MAX_M), self.num_sensors+((sink+1)*MAX_M)):
                vsinks_added.append(vsink)
                self._add_to(virtual_sinks_map, sink, vsink)
                for i in sensors:
                    self._add_to(new_sensor_sensor, vsink, i)
                    self._add_to(new_sensor_sensor, i, vsink)

                # Start a new instance with an modified string
        result = KCMC_Instance(
            instance_string=';'.join([
                'KCMC',
                f'{self.num_pois} {self.num_sensors+(MAX_M*self.num_sinks)} 1',
                f'{self.area_side} {self.sensor_coverage_radius} {self.sensor_communication_radius}',
                f'{self.random_seed}'
             ] +['PI']+[f'{p} {i}' for p, sensors in self.poi_sensor.items() for i in sensors]
               +['II']+[f'{p} {i}' for p, sensors in new_sensor_sensor.items() for i in sensors]
               +['IS']+[f'{vs} 0' for vs in vsinks_added]
               +['END']),
            accept_loose_pois=True, accept_loose_sensors=True, accept_loose_sinks=True
        )
        result.virtual_sinks_map = virtual_sinks_map
        return result

    def get_node_label(self, node, installation=None):
        result = 'V'+node if node in self.virtual_sinks else node
        if installation is not None:
            if node.startswith('i'):
                tree = installation.get(node)
                if tree is not None:
                    result = result + f'.{tree}'
        return result

    def get_node_color(self, node, installation=None):
        result = self.color_dict["vsink"] if node in self.virtual_sinks else self.color_dict[node[0]]
        if installation is not None:
            if node.startswith('p'): return self.color_dict['p']
            if node.startswith('s'): return self.color_dict['s']
            tree = installation.get(node)
            if tree is None:
                result = self.color_dict['offline']
            else:
                result = self.color_dict[f'tree_{int(tree) % 5}']
        return result

    def plot(self, labels=False, installation=None, minimal=False):
        assert igraph is not None, 'IGRAPH NOT INSTALLED!'
        g = igraph.Graph()
        if minimal:
            showing = set(self.pois+self.sinks)
            if installation is None:
                showing = showing.union(self.sensors)
            else:
                showing = showing.union(set([i for i, v in installation.items() if v is not None]))
            g.add_vertices(list(showing))
            g.add_edges([(i, j) for i, j in self.linear_edges if ((i in showing) and (j in showing))])
        else:
            g.add_vertices(self.pois+self.sinks+self.sensors)
            g.add_edges(self.linear_edges)
        layout = g.layout("kk")  # Kamada-Kawai Force-Directed algorithm

        # Set the COLOR of every NODE
        g.vs["color"] = [self.get_node_color(node, installation) for node in g.vs["name"]]

        # Set the NAME of the node as its label. Add the TREE it is installed on, if exists
        if labels:
            g.vs["label"] = [self.get_node_label(node, installation) for node in g.vs["name"]]
            g.vs["label_size"] = 6

        # Print as UNIDIRECTED
        g.to_undirected()
        return igraph.plot(g, layout=layout)


# ######################################################################################################################
# RUNTIME


def parse_block(df):

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
    df = df.fillna(False).drop_duplicates(
        subset=(['key', 'random_seed'] + [col for col in df.columns if (col.startswith('K') or col.startswith('M'))])
    ).reset_index(drop=True)

    return df
