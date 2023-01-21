
# STDLIB
import os
import zlib
import json
import base64
from time import time
from typing import Literal
from dataclasses import dataclass, asdict

# GUROBI
from gurobipy import GRB

# THIS PACKAGE
from kcmc_instance import KCMC_Instance
from gurobi_models import GUROBI_STATUS_TRANSLATE, gurobi_single_flow, gurobi_multi_flow


def compress(str_input:str) -> str: return base64.b64encode(zlib.compress(str_input.encode('utf-8'))).decode('ascii')


def decompress(str_input): return zlib.decompress(base64.b64decode(str_input.encode('ascii'))).decode('utf-8')


@dataclass(frozen=True)
class ExperimentResult:

    # Experiment setup
    instance_key: str
    kcmc_k: int
    kcmc_m: int
    heuristic: Literal["kcov-dinic", "reuse-dinic", "flood-dinic", "best-dinic", "none"]
    gurobi_model: Literal["multi-flow", "single-flow", "none"]
    time_limit: int
    threads: int

    # Easy-going Instance parameters
    pois: int
    sensors: int
    sinks: int
    random_seed: int

    # Heuristic results
    heuristic_objective_value: float
    heuristic_solution: str
    heuristic_time: int

    # ILP results
    ilp_status: Literal['OPTIMAL', 'INFEASIBLE', 'LIMIT', 'none'] = 'none'
    ilp_objective_value: float = 0.0
    ilp_solution: str = ''
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
    gurobi_variable_X: str = ''
    gurobi_variable_Y: str = ''

    @property
    def to_json(self): return json.dumps(asdict(self))


def run_experiment(
        instance_key:str, kcmc_k:int, kcmc_m:int,
        heuristic:Literal['kcov-dinic', 'reuse-dinic', 'flood-dinic', 'best-dinic', 'none'],
        gurobi_model:Literal['multi-flow', 'single-flow', 'none'],
        time_limit=3600, threads=1, y_binary=False
    ) -> ExperimentResult:

    # De-serialize the instance and compute its no-solution
    original_instance = KCMC_Instance(instance_key)
    no_solution = '1'*original_instance.num_sensors

    # Run the heuristic in the instance
    if heuristic == 'none':
        instance = original_instance
        heuristic_objective_value = original_instance.num_sensors
        heuristic_solution = no_solution
        heuristic_time = 0
    else:
        instance, preprocessing_result = original_instance.preprocess(kcmc_k, kcmc_m, heuristic)
        heuristic_objective_value = instance.num_sensors
        heuristic_solution = preprocessing_result.heuristic_solution
        heuristic_time = preprocessing_result.heuristic_time

    # Run the ILP Model in the instance
    if gurobi_model == 'none':
        return ExperimentResult(
            # Experiment setup
            instance_key=instance_key, kcmc_k=kcmc_k, kcmc_m=kcmc_m, heuristic=heuristic, gurobi_model='none',
            time_limit=time_limit, threads=threads,

            # Easy-going Instance parameters
            pois=original_instance.num_pois, sensors=original_instance.num_sensors, sinks=original_instance.num_sinks,
            random_seed=original_instance.random_seed,

            # Heuristic results
            heuristic_objective_value=heuristic_objective_value,
            heuristic_solution=heuristic_solution,
            heuristic_time=heuristic_time,

            # ILP results
            ilp_status='none',
            ilp_solution=no_solution,
            gurobi_variable_X=no_solution
        )
    elif gurobi_model == 'multi-flow':
        model, X, Y, setup = gurobi_multi_flow(kcmc_k, kcmc_m, instance, time_limit, threads, y_binary)
    elif gurobi_model == 'multi-flow':
        model, X, Y, setup = gurobi_single_flow(kcmc_k, kcmc_m, instance, time_limit, threads, y_binary)
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

    # Form the BitTrain solution
    solution = ['0']*instance.num_sensors
    for isid, active in X.items():
        if active:
            if "(" in isid:
                isid = eval(isid)[0]
            solution[int(isid[1:])] = '1'
    solution = ''.join(solution)

    # Parse the results
    var_Y = {
        str(k): (v.X if model.SolCount > 0 else (v.Xn[0] if hasattr(v, 'Xn') else None))
        for k,v in Y.items()
    }
    json_solution = model.getJSONSolution()
    return ExperimentResult(
        # Experiment setup
        instance_key=instance_key, kcmc_k=kcmc_k, kcmc_m=kcmc_m, heuristic=heuristic, gurobi_model='none',
        time_limit=time_limit, threads=threads,

        # Easy-going Instance parameters
        pois=original_instance.num_pois, sensors=original_instance.num_sensors, sinks=original_instance.num_sinks,
        random_seed=original_instance.random_seed,

        # Heuristic results
        heuristic_objective_value=heuristic_objective_value,
        heuristic_solution=heuristic_solution,
        heuristic_time=heuristic_time,

        # ILP results
        ilp_status=GUROBI_STATUS_TRANSLATE[model.Status],
        ilp_objective_value=json.loads(json_solution)['SolutionInfo'].get('ObjVal', instance.num_sensors),
        ilp_solution=solution,
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
        gurobi_setup_model_time=setup.model_setup,
        gurobi_setup_x_variable_time=setup.x_variable_setup,
        gurobi_setup_y_variable_time=setup.y_variable_setup,
        gurobi_setup_objective_function_time=setup.objective_function_setup,
        gurobi_setup_constraints_time=setup.constraints_setup,

        # Compressed variables
        gurobi_logs=compress(gurobi_logs),
        gurobi_variable_X=solution,
        gurobi_variable_Y=compress(json.dumps(var_Y))
    )
