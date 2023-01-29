
# STDLIB
import os
import zlib
import json
import base64
import argparse
from typing import List, Set, Union
from dataclasses import dataclass, asdict

# GUROBI
from gurobipy import GRB

# THIS PACKAGE
from kcmc_instance import KCMC_Instance
from gurobi_models import GUROBI_STATUS_TRANSLATE, gurobi_single_flow, gurobi_multi_flow, time


def compress(str_input:str) -> str: return base64.b64encode(zlib.compress(str_input.encode('utf-8'))).decode('ascii')


def decompress(str_input): return zlib.decompress(base64.b64decode(str_input.encode('ascii'))).decode('utf-8')


@dataclass(frozen=True)
class GurobiExperimentResult:

    # Experiment setup
    instance_key: str
    kcmc_k: int
    kcmc_m: int
    heuristic: str
    gurobi_model: str  # Literal["multi-flow", "single-flow"]
    time_limit: int
    threads: int

    # Easy-going Instance parameters
    pois: int
    sensors: int
    sinks: int
    random_seed: int

    # Optimization state
    start_active_sensors: List[str]
    end_active_sensors: List[str]

    # ILP results
    ilp_status: str  = 'none'  # Literal['OPTIMAL', 'INFEASIBLE', 'LIMIT', 'none'] = 'none'
    ilp_objective_value: float = 0.0
    ilp_time: int = 0
    ilp_binary_variables_count: int = 0
    ilp_solutions_count: int = 0
    ilp_node_count: int = 0
    ilp_iterations_count: int = 0

    # Gurobi-Specific results
    gurobi_runtime: int = 0
    gurobi_mip_gap: float = 0.0
    gurobi_status_code: int = 0
    gurobi_model_fingerprint: str = ''
    gurobi_json_solution: str = '{}'

    # ILP Setup
    gurobi_setup_model_time: int = 0
    gurobi_setup_x_variable_time: int = 0
    gurobi_setup_y_variable_time: int = 0
    gurobi_setup_objective_function_time: int = 0
    gurobi_setup_constraints_time: int = 0

    # Compressed variables
    gurobi_logs: str = ''

    def __post_init__(self):
        assert self.gurobi_model in ["multi-flow", "single-flow"], f'UNKNOWN GUROBI_MODEL: {self.gurobi_model}'
        assert self.ilp_status in ['OPTIMAL', 'INFEASIBLE', 'LIMIT', 'none'], f'UNKNOWN ILP_STATUS: {self.ilp_status}'

    @property
    def as_dict(self) -> dict:
        result = asdict(self)
        result['gurobi_logs'] = compress(result['gurobi_logs'])
        return result


def run_gurobi_experiment(
        instance_key:str, kcmc_k:int, kcmc_m:int,
        heuristic:str,
        heuristic_active_sensors:Union[None, Set[str]],
        gurobi_model:str,  # Literal['multi-flow', 'single-flow'],
        time_limit=3600,
        threads=1,
        y_binary=False
    ) -> GurobiExperimentResult:
    if "_" in instance_key: instance_key = instance_key.replace('_', ' ')

    # De-serialize the instance and compute its no-solution
    instance = KCMC_Instance(instance_key, active_sensors=heuristic_active_sensors)
    print('OPTIMIZING', instance.key_str, ' - CURRENTLY AT', f'{len(instance.active_sensors)}/{instance.num_sensors}')

    # Get the model object
    if gurobi_model == 'multi-flow':
        model, X, Y, setup_metadata = gurobi_multi_flow(kcmc_k, kcmc_m, instance, time_limit, threads, y_binary)
    elif gurobi_model == 'single-flow':
        model, X, Y, setup_metadata = gurobi_single_flow(kcmc_k, kcmc_m, instance, time_limit, threads, y_binary)
    else: raise TypeError(f'Unknown model {gurobi_model}')

    # Run the model
    s_opt = time()
    model.optimize()
    e_opt = time()

    # Warn the model that it has ended
    model.setParam(GRB.Param.OutputFlag, 0)

    # Retrieve the LOGs
    with open('/tmp/gurobi.log', 'r') as fin:
        gurobi_logs = '\n'.join([l.replace('\n', '') for l in fin.readlines() if len(l.strip()) > 0])
    os.unlink('/tmp/gurobi.log')

    # Get the state of each sensor at the end
    end_sensor_state = [False]*instance.num_sensors
    for isid, active in X.items():
        if active:
            if isinstance(isid, tuple): isid, tree_id = isid # For the multi-flow, that yields tuples
            end_sensor_state[int(isid[1:])] = True

    # Parse the results
    json_solution = model.getJSONSolution()
    return GurobiExperimentResult(

        # Experiment setup
        instance_key=instance_key, kcmc_k=kcmc_k, kcmc_m=kcmc_m, heuristic=heuristic, gurobi_model=gurobi_model,
        time_limit=time_limit, threads=threads,

        # Easy-going Instance parameters
        pois=instance.num_pois, sensors=instance.num_sensors, sinks=instance.num_sinks, random_seed=instance.random_seed,

        # Optimization State
        start_active_sensors=sorted(instance.active_sensors),
        end_active_sensors=[f'i{i}' for i,s in enumerate(end_sensor_state) if s],  # Only ACTIVE sensors

        # ILP results
        ilp_status=GUROBI_STATUS_TRANSLATE[model.Status],
        ilp_objective_value=json.loads(json_solution)['SolutionInfo'].get('ObjVal', instance.num_sensors),
        ilp_time=int(e_opt-s_opt),
        ilp_binary_variables_count=int(model.NumBinVars),
        ilp_solutions_count=int(model.SolCount),
        ilp_node_count=int(model.NodeCount),
        ilp_iterations_count=int(model.IterCount),

        # Gurobi-Specific results
        gurobi_runtime=int(model.Runtime),
        gurobi_mip_gap=float(model.MIPGap),
        gurobi_status_code=int(model.Status),
        gurobi_model_fingerprint=str(model.Fingerprint),
        gurobi_json_solution=json_solution,

        # ILP Setup
        gurobi_setup_model_time=setup_metadata.model_setup,
        gurobi_setup_x_variable_time=setup_metadata.x_variable_setup,
        gurobi_setup_y_variable_time=setup_metadata.y_variable_setup,
        gurobi_setup_objective_function_time=setup_metadata.objective_function_setup,
        gurobi_setup_constraints_time=setup_metadata.constraints_setup,

        # Compressed variables
        gurobi_logs=gurobi_logs
    )


# RUNTIME ##############################################################################################################


if __name__ == '__main__':

    # Parse the arguments
    parser = argparse.ArgumentParser(prog='KCMC Gurobi Optimizer', description='Optimize an instance of the KCMC problem using GUROBI')
    parser.add_argument('kcmc_instance')
    parser.add_argument('kcmc_k', type=int)
    parser.add_argument('kcmc_m', type=int)
    parser.add_argument('gurobi_model', choices=['multi-flow', 'single-flow'])
    parser.add_argument('-e', '--heuristic', default='none')
    parser.add_argument('-a', '--heuristic_active_sensors', default='')
    parser.add_argument('-t', '--time_limit', type=int, default=3600)
    parser.add_argument('-p', '--processes', type=int, default=1)
    parser.add_argument('--y_binary', action='store_true', default=False)
    args = parser.parse_args()

    # Validate some arguments
    heuristic = args.heuristic.lower().strip()
    heuristic_active_sensors = args.heuristic_active_sensors
    if heuristic == 'none':
        if len(heuristic_active_sensors) > 0:
            raise TypeError('If no heuristic, there can be no limited set of active sensors!')
    else:
        try:
            heuristic_active_sensors = heuristic_active_sensors.split('i')
            empty, heuristic_active_sensors = heuristic_active_sensors[0], heuristic_active_sensors[1:]
            assert empty == '', 'invalid format! it must be a string like i0i1...iX without spaces!'
            heuristic_active_sensors = [f'i{i}' for i in map(int, heuristic_active_sensors)]
            assert len(heuristic_active_sensors) > 0, 'IF THERE IS AN HEURISTIC, THERE SHOULD BE AT LEAST ONE ACTIVE SENSOR!'
        except Exception as exp:
            raise Exception(f'Invalid listing of limited active sensors! {args.heuristic_active_sensors} - {exp}')

    # Run an experiment
    gex_result = run_gurobi_experiment(
        instance_key=args.kcmc_instance,
        kcmc_k=args.kcmc_k,
        kcmc_m=args.kcmc_m,
        heuristic=heuristic,
        heuristic_active_sensors=heuristic_active_sensors,
        gurobi_model=args.gurobi_model,
        time_limit=args.time_limit,
        threads=args.processes,
        y_binary=args.y_binary
    )

    # Store the experiment as a JSON file with a predictable name
    outfile = f'/tmp/kcmc_gurobi_experiment_{gex_result.random_seed}_{args.heuristic}_{args.gurobi_model}_{args.y_binary}.json'
    with open(outfile, 'w') as out:
        out.write(json.dumps(gex_result.as_dict))

    print(f'ALL DONE! {gex_result.ilp_status} {gex_result.ilp_objective_value}/{len(gex_result.start_active_sensors)} {gex_result.ilp_time}\t{outfile}')
