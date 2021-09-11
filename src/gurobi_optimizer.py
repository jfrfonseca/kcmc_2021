"""
Gurobi Optimizer Driver Script
"""


# STDLib
import json
import time
import os.path
import logging
import argparse
import traceback
from datetime import datetime

# GUROBI
import gurobipy as gp
from gurobipy import GRB

# This package
from kcmc_instance import KCMC_Instance


# TRANSLATE GUROBI STATUS CODES
GUROBI_STATUS_TRANSLATE = {
    GRB.OPTIMAL: f'OPTIMAL ({GRB.OPTIMAL})',
    GRB.INFEASIBLE: f'INFEASIBLE ({GRB.INFEASIBLE})',
    GRB.INF_OR_UNBD: f'INFEASIBLE ({GRB.INF_OR_UNBD})',
    GRB.UNBOUNDED: f'INFEASIBLE ({GRB.UNBOUNDED})',

    GRB.ITERATION_LIMIT: f'LIMIT ({GRB.ITERATION_LIMIT})',
    GRB.NODE_LIMIT: f'LIMIT ({GRB.NODE_LIMIT})',
    GRB.TIME_LIMIT: f'LIMIT ({GRB.TIME_LIMIT})',
    GRB.SOLUTION_LIMIT: f'LIMIT ({GRB.SOLUTION_LIMIT})',
    GRB.USER_OBJ_LIMIT: f'LIMIT ({GRB.USER_OBJ_LIMIT})'
}


def run_gurobi_optimizer(serialized_instance:str,
                         kcmc_k:int, kcmc_m:int,
                         time_limit:float, processes:int,
                         log:callable=print, LOGFILE:str=None) -> dict:

    # Prepare the results object
    results = {'kcmc': {'k': kcmc_k, 'm': kcmc_m, 'raw': {}, 'single_sink': {}},
               'optimization': {'X': [], 'installation': {}},
               'limits': {'time': time_limit, 'processes': processes}}

    # De-Serialize the instance as a KCMC_Instance object.
    # It MIGHT be multisink and thus incompatible with the ILP formulation
    maybe_multisink_instance = KCMC_Instance(serialized_instance, False, True, True)
    log(f'Parsed raw instance of key {maybe_multisink_instance.key_str}')

    # Add metadata to the result object
    results['kcmc']['raw'].update({
        'key': maybe_multisink_instance.key_str,
        'instance': serialized_instance,
        'pois': maybe_multisink_instance.num_pois,
        'sensors': maybe_multisink_instance.sensors,
        'sinks': maybe_multisink_instance.num_sinks,
        'area_side': maybe_multisink_instance.area_side,
        'coverage_radius': maybe_multisink_instance.sensor_coverage_radius,
        'communication_radius': maybe_multisink_instance.sensor_communication_radius,
        'coverage_density': maybe_multisink_instance.coverage_density,
        'communication_density': maybe_multisink_instance.communication_density
    })

    # Convert the instance to a single-sink version.
    # Use M as the MAX_M parameter for minimal impact on performance
    instance = maybe_multisink_instance.to_single_sink(MAX_M=kcmc_m)
    log('Ensured single-sink instance')

    # Add metadata to the result object
    results['kcmc']['single_sink'].update({
        'key': instance.key_str,
        'instance': instance.string,
        'pois': instance.num_pois,
        'sensors': instance.sensors,
        'sinks': instance.num_sinks,
        'area_side': instance.area_side,
        'coverage_radius': instance.sensor_coverage_radius,
        'communication_radius': instance.sensor_communication_radius,
        'coverage_density': instance.coverage_density,
        'communication_density': instance.communication_density,
        'virtual_sinks': {k: list(v) for k, v in instance.virtual_sinks_map.items()}
    })

    # Extract the component sets
    iC = instance.inverse_coverage_graph
    L = [str(m) for m in range(kcmc_m)]
    P = instance.pois
    I = instance.sensors
    S = instance.sinks
    s = list(S)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!
    A_c = instance.poi_edges
    A_s = instance.sink_edges
    A_g = instance.sensor_edges
    A = A_c + A_g + A_s
    log('Extracted constants')

    # MODEL SETUP ======================================================================================================

    # Set the GUROBI MODEL
    model = gp.Model('KCMC')

    # Set the Time Limit and the Thread Count
    model.setParam(GRB.Param.TimeLimit, time_limit)
    model.setParam(GRB.Param.Threads, processes)

    # Set the LOG configuration
    if LOGFILE is not None: model.setParam(GRB.Param.LogFile, LOGFILE)
    model.setParam(GRB.Param.OutputFlag, 1)
    model.setParam(GRB.Param.LogToConsole, 0)

    # Set the variables
    X = model.addVars(I, L, vtype=GRB.BINARY, name="x")
    Y = model.addVars(A, P, L, vtype=GRB.BINARY, name='y')

    # Set the objective function
    model.setObjective(X.sum('*', '*'), GRB.MINIMIZE)

    # Set the CONSTRAINTS -------------------------------------------------------------------------

    # Disjunction -----------------------------------------
    disjunction = model.addConstrs(
        (X.sum(i, '*') <= 1
         for i in I),
        name="ilp_multi_disjunction"
    )

    # Flow ------------------------------------------------
    ilp_multi_flow_p = model.addConstrs(
        ((  gp.quicksum(Y.select(p, '*', p, l))
          - gp.quicksum(Y.select('*', p, p, l))) == 1
         for p in P
         for l in L),
        name="ilp_multi_flow_p"
    )

    ilp_multi_flow_s = model.addConstrs(
        ((  gp.quicksum(Y.select(s, '*', p, l))
          - gp.quicksum(Y.select('*', s, p, l))) == -1
         for p in P
         for l in L),
        name="ilp_multi_flow_s"
    )

    ilp_multi_flow_i = model.addConstrs(
        ((  gp.quicksum(Y.select(i, '*', p, l))
          - gp.quicksum(Y.select('*', i, p, l))) == 0
         for i in I
         for p in P
         for l in L),
        name="ilp_multi_flow_i"
    )

    # Projection ------------------------------------------
    ilp_multi_projection = model.addConstrs(
        (Y.sum(i, '*', p, l) <= X.sum(i, l)
         for i in I
         for p in P
         for l in L),
        name="ilp_multi_projection"
    )

    # K-Coverage ------------------------------------------
    ilp_multi_k_coverage = model.addConstrs(
        (gp.quicksum(X.select(iC[p], '*')) >= kcmc_k
         for p in P),
        name="ilp_multi_k_coverage"
    )

    # Save the model --------------------------------------
    # model.write(f'/home/gurobi/results/{KEY}.lp')
    # model.write(f'/home/gurobi/results/{KEY}.mps')
    log('GUROBI Model set up')

    # MODEL EXECUTION ==================================================================================================

    # Run the execution
    log('STARTING OPTIMIZATION ' + ('*'*38))
    results['optimization']['start'] = time.time_ns()
    model.optimize()
    results['optimization']['finish'] = time.time_ns()
    results['optimization']['time'] = results['optimization']['finish'] - results['optimization']['start']
    log('OPTIMIZATION DONE ' + ('*'*42))

    # Warn the model that it has ended
    model.setParam(GRB.Param.OutputFlag, 0)

    # Store some metadata
    STATUS = GUROBI_STATUS_TRANSLATE.get(model.Status, f'ERROR ({model.Status})')
    results['optimization'].update({
        'status_code': model.Status,
        'status': STATUS.split(' ')[0],
        'solutions_count': model.SolCount
    })
    log(f'OPTIMIZATION STATUS: {STATUS}')
    log(f'QUANTITY OF SOLUTIONS FOUND: {model.SolCount}')

    # Store the values of X
    if 'OPTIMAL' in STATUS:
        for x in X:
            installation_spot_id, steiner_tree_id = x
            sensor_installed = bool(X[x].X)
            results['optimization']['X'].append((installation_spot_id, steiner_tree_id, sensor_installed))
            if sensor_installed:
                results['single_sink']['installation'][installation_spot_id] = steiner_tree_id

    # Parse the used spots
    results['single_sink']['used_spots'] = [s for s, i in results['single_sink']['installation'].items()]
    results['raw']['installation'] = {instance.virtual_sinks_dict.get(s, s): t for s, t in results['single_sink']['installation'].items()}
    results['raw']['used_spots'] = [s for s, i in results['raw']['installation'].items()]

    log('DONE '+('*'*55))
    return results


# ######################################################################################################################
# RUNTIME


if __name__ == '__main__':

    # Set the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--instance", required=True, help="KCMC Instance serialized as a STRING")
    parser.add_argument("-k", "--kcmc_k", required=True, help="KCMC K value to evaluate")
    parser.add_argument("-m", "--kcmc_m", required=True, help="KCMC M value to evaluate")
    parser.add_argument('-t', '--time_limit', help='Time limit', default=60)
    parser.add_argument('-p', '--processes', help='Number of processes to use', default=1)
    args, unknown_args = parser.parse_known_args()

    # Parse the arguments
    serialized_instance = str(args.instance.strip().upper())
    time_limit = float(str(args.time_limit))
    processes = int(float(str(args.processes)))
    kcmc_k = int(float(str(args.kcmc_k)))
    kcmc_m = int(float(str(args.kcmc_m)))

    # Parse the KEY of the instance
    KEY = '_'.join(serialized_instance.split(';', 4)[:4])

    # Check if the LOG file already exists. If so, skip.
    RESULTS_KEY = f'/home/gurobi/results/{KEY}'
    if os.path.exists(RESULTS_KEY+'.json'): exit(0)

    # Start the logger
    logging.basicConfig(filename=RESULTS_KEY+'.log', level=logging.DEBUG)
    def log(logstring): logging.info('APP:::'+logstring)

    # Run the main execution, logging possible errors
    try:
        results = run_gurobi_optimizer(serialized_instance, kcmc_k, kcmc_m, time_limit, processes, log, LOGFILE=RESULTS_KEY+'.log')

        # Save the results on disk and exit with success
        with open(RESULTS_KEY+'.json', 'w') as fout:
            json.dump(results, fout, indent=2)
        exit(0)
    except Exception as exp:
        with open(RESULTS_KEY+'.err', 'a') as fout:
            fout.write(f'KEY {KEY} AT {datetime.now()}\n{traceback.format_tb(exp)}\n\n')
        exit(2)
