"""
Gurobi Optimizer Driver Script
"""


# STDLib
import logging
import argparse

# GUROBI
import gurobipy as gp
from gurobipy import GRB

# This package
from kcmc_instance import KCMC_Instance


if __name__ == '__main__':

    # Set the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--instance", required=True, help="KCMC Instance serialized as a STRING")
    parser.add_argument("-k", "--kcmc_k", required=True, help="KCMC K value to evaluate")
    parser.add_argument("-m", "--kcmc_m", required=True, help="KCMC M value to evaluate")
    parser.add_argument('-t', '--time_limit', help='Time limit', default=60)
    parser.add_argument('-p', '--processes', help='Number of processes to use', default=1)
    parser.add_argument('-v', '--verbose', help='Log to CONSOLE and to file', default=False)
    args, unknown_args = parser.parse_known_args()

    # Parse the arguments
    serialized_instance = str(args.instance.strip().upper())
    time_limit = float(str(args.time_limit))
    processes = int(float(str(args.processes)))
    kcmc_k = int(float(str(args.kcmc_k)))
    kcmc_m = int(float(str(args.kcmc_m)))
    verbose = bool(str(args.verbose))

    # PREPROCESSING ====================================================================================================

    # De-Serialize the instance as a KCMC_Instance object.
    # It MIGHT be multisink and thus incompatible with the ILP formulation
    maybe_multisink_instance = KCMC_Instance(serialized_instance, False, True, True)
    KEY = maybe_multisink_instance.key_str.replace(':', '_')

    # Start the logger
    logging.basicConfig(filename=f'/home/gurobi/results/{KEY}.out', level=logging.DEBUG)
    logging.info('Parsed raw instance')

    # Convert the instance to a single-sink version.
    # Use M as the MAX_M parameter for minimal impact on performance
    instance = maybe_multisink_instance.to_single_sink(MAX_M=kcmc_m)
    logging.info('Ensured single-sink instance')

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
    logging.info('Extracted constants')

    # MODEL SETUP ======================================================================================================

    # Set the GUROBI MODEL
    model = gp.Model('KCMC')

    # Set the Time Limit and the Thread Count
    model.setParam(GRB.Param.TimeLimit, 60.0)
    model.setParam(GRB.Param.Threads, 1)

    # Set the LOG configuration
    model.setParam(GRB.Param.LogFile, f'/home/gurobi/results/{KEY}.model.log')
    model.setParam(GRB.Param.OutputFlag, 1)
    model.setParam(GRB.Param.LogToConsole, 1 if verbose else 0)

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
    model.write(f'/home/gurobi/results/{KEY}.lp')
    logging.info('GUROBI Model set up')

    # MODEL EXECUTION ==================================================================================================

    # Run the execution
    logging.info('STARTING OPTIMIZATION')
    model.optimize()
    logging.info('OPTIMIZATION DONE')

    # Warn the model that it has ended
    model.setParam(GRB.Param.OutputFlag, 0)

    # Store some metadata
    logging.info(f'OPTIMIZATION STATUS: {model.Status}')
    logging.info(f'QUANTITY OF SOLUTIONS FOUND: {model.SolCount}')

    # Store the values of X
    if model.Status != GRB.OPTIMAL:
        logging.info('X VALUES ===============================')
        for x in X:
            installation_spot_id, steiner_tree_id = x
            sensor_installed = bool(X[x].X)
            logging.info(f'IS: {installation_spot_id} \t STree: {steiner_tree_id} \t INSTALLED: {sensor_installed}')
