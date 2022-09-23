

import os
import json
import multiprocessing

import pandas as pd

from gurobi_models import KCMC_Result, GurobiModelWrapper

from tqdm.notebook import tqdm
from datetime import timedelta


def parse_file(file):

    # Load the raw data from the file
    with open('/data/results/'+file, 'r') as fin:
        raw_data = json.load(fin)

    # Parse the PRESOLVE data
    ini_rows,  ini_cols,  ini_n0, \
    ini_cont,  ini_int,   ini_bin, \
    end_rows,  end_cols,  end_n0, \
    end_cont,  end_int,   end_bin, \
    prem_rows, prem_cols, pre_t, \
    gurobi_heuristic_objective_value = KCMC_Result.parse_presolve(raw_data['gurobi_logs'])

    assert (ini_rows-prem_rows) == end_rows, 'ROWS'
    assert (ini_cols-prem_cols) == end_cols, 'COLUMNS'

    # Parse some other fields
    pois, sensors, sinks = raw_data['key'].split(';')[1].split(' ')
    area_s, cov_r, com_r = raw_data['key'].split(';')[2].split(' ')
    gurobi_setup_time = sum([raw_data['time']['setup']['model']]
                      + list(raw_data['time']['setup']['constraints'].values())
                      + list(raw_data['time']['setup']['vars'].values()))

    # Parse the Gurobi solution to the KCMC problem
    X = KCMC_Result.parse_variable(raw_data['variables']['x'])
    solution = KCMC_Result.get_solution(X, int(sensors))

    # Emit a KCMC_Result object
    return KCMC_Result(
        source_file=file,

        # Key Values ----------------------
        pois=int(pois),
        sensors=int(sensors),
        sinks=int(sinks),

        area_side=int(area_s),
        coverage_radius=int(cov_r),
        communication_radius=int(com_r),

        random_seed=int(raw_data['random_seed']),

        k=int(raw_data['kcmc_k']),
        m=int(raw_data['kcmc_m']),

        heuristic_name=str(raw_data['preprocessing'].get('method', None)),
        y_binary=bool(raw_data['gurobi_y_binary']),
        gurobi_model_name=str(raw_data['gurobi_model']),

        # Main results --------------------
        heuristic_solution=str(raw_data['preprocessing'].get('solution', '1'*int(sensors))),
        # heuristic_objective_value
        # heuristic_solution_size
        # heuristic_solution_quality
        gurobi_optimal=bool(raw_data['status'] == 'OPTIMAL'),
        gurobi_heuristic_objective_value=gurobi_heuristic_objective_value,
        gurobi_objective_value=float(raw_data['objective_value']) if raw_data['objective_value'] is not None else None,
        gurobi_solution=solution,
        solution=solution,
        # solution_size
        # solution_quality

        # Time ----------------------------
        heuristic_time=float(raw_data['preprocessing'].get('runtime_us', 0)) / 1_000_000,
        gurobi_setup_time=float(gurobi_setup_time/1_000_000_000),
        gurobi_presolve_time=float(pre_t),
        gurobi_run_time=float(raw_data['gurobi_runtime']),
        # total_time: float

        # Other Solver results ------------
        gurobi_mip_gap=float(raw_data['json_solution']['SolutionInfo']['MIPGap']),
        gurobi_bound=float(raw_data['json_solution']['SolutionInfo']['ObjBound']),
        gurobi_bound_c=float(raw_data['json_solution']['SolutionInfo']['ObjBoundC']),
        gurobi_node_count=int(raw_data['json_solution']['SolutionInfo']['NodeCount']),
        gurobi_solutions_count=int(raw_data['json_solution']['SolutionInfo']['SolCount']),
        gurobi_simplex_iterations_count=int(raw_data['json_solution']['SolutionInfo']['IterCount']),
        gurobi_initial_rows_count=int(ini_rows),
        gurobi_initial_columns_count=int(ini_cols),
        gurobi_initial_non_zero_count=int(ini_n0),
        gurobi_initial_continuous_variables_count=int(ini_cont),
        gurobi_initial_integer_variables_count=int(ini_int),
        gurobi_initial_binary_variables_count=int(ini_bin),
        gurobi_presolve_removed_rows=int(prem_rows),
        gurobi_presolve_removed_columns=int(prem_cols),
        gurobi_rows_count=int(end_rows),
        gurobi_columns_count=int(end_cols),
        gurobi_non_zero_count=int(end_n0),
        gurobi_continuous_variables_count=int(end_cont),
        gurobi_integer_variables_count=int(end_int),
        gurobi_binary_variables_count=int(end_bin),

        # Other attributes ----------------
        time_limit=float(raw_data['time_limit']),
        gurobi_logs=str(raw_data['gurobi_logs']),
        gurobi_variable_x_size=len(X),
        gurobi_variable_y_size=KCMC_Result.get_variable_size(raw_data['variables']['y'])
    )


def wrap_parse_file(file):
    try:
        return parse_file(file)
    except Exception as exp:
        print(f'EXCEPTION ON FILE {file}: {exp}')
        return None

pre_df = pd.read_parquet('/home/gurobi/src/results.pq') if os.path.exists('/home/gurobi/src/results.pq') else None
pre_parsed_files = set(pre_df['source_file']) if pre_df is not None else set()
dir_files = os.listdir('/data/results')
json_files = sorted([f for f in dir_files if f.endswith('.json') and f not in pre_parsed_files])

pool = multiprocessing.Pool()
parsed_results = list(
    tqdm(
        pool.imap_unordered(parse_file, json_files),
        total=len(json_files)
    )
)
pool.close()

df = pd.DataFrame([d.to_dict() for d in parsed_results if d is not None])
df = df[sorted(df.columns)].copy()

len(dir_files), len(json_files), len(df), df.columns
if pre_df is not None: df = pre_df.append(df).drop_duplicates().reset_index(drop=True)
df.to_parquet('/home/gurobi/src/results.pq')
