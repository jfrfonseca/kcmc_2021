

# STDLIB
from time import time
from typing import Any, Literal
from dataclasses import dataclass

# GUROBI
import gurobipy as gp
from gurobipy import GRB

# THIS PACKAGE
from kcmc_instance import KCMC_Instance


@dataclass(frozen=True)
class ModelSetupTime:
    model_setup: int
    x_variable_setup: int
    y_variable_setup: int
    objective_function_setup: int
    constraints_setup: int


GUROBI_STATUS_TRANSLATE = {
    GRB.OPTIMAL: f'OPTIMAL',
    GRB.INFEASIBLE: f'INFEASIBLE',
    GRB.INF_OR_UNBD: f'INFEASIBLE',
    GRB.UNBOUNDED: f'INFEASIBLE',

    GRB.ITERATION_LIMIT: f'LIMIT ',
    GRB.NODE_LIMIT: f'LIMIT',
    GRB.TIME_LIMIT: f'LIMIT',
    GRB.SOLUTION_LIMIT: f'LIMIT',
    GRB.USER_OBJ_LIMIT: f'LIMIT'
}


def gurobi_multi_flow(
        kcmc_k:int, kcmc_m:int,
        kcmc_instance:KCMC_Instance,
        time_limit=3600,
        threads=1,
        y_binary=False
    ) -> (gp.Model, Any, Any, ModelSetupTime):

    # Prepare the model object --------------------------------------------------------------------
    s_model = time()
    model = gp.Model('multi-flow')

    # Set the Time Limit and the Thread Count
    model.setParam(GRB.Param.TimeLimit, time_limit)
    model.setParam(GRB.Param.Threads, threads)

    # Set the LOG configuration
    model.setParam(GRB.Param.LogFile, '/tmp/gurobi.log')
    model.setParam(GRB.Param.OutputFlag, 1)
    model.setParam(GRB.Param.LogToConsole, 0)

    # Extract and apply the components
    P = kcmc_instance.pois
    I = kcmc_instance.sensors
    s = list(kcmc_instance.sinks)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!

    A_c = kcmc_instance.poi_edges
    A_s = kcmc_instance.sink_edges
    A_g = kcmc_instance.sensor_edges
    A = A_c + A_g + A_s

    iC = kcmc_instance.inverse_coverage_graph
    L = [str(m) for m in range(kcmc_m)]

    e_model = time()

    # Set the variables ---------------------------------------------------------------------------
    s_x = time()
    X = model.add_vars(I, L, name='x', vtype=GRB.BINARY)
    e_x = time()
    s_y = time()
    if y_binary:
        Y = model.add_vars(A, P, L, name='y', vtype=GRB.BINARY)
    else:
        Y = model.add_vars(A, P, L, name='y')
    e_y = time()

    # Set the objective function ------------------------------------------------------------------
    s_obj = time()
    model.set_objective(X.sum('*', '*'), GRB.MINIMIZE)
    e_obj = time()

    # Set the CONSTRAINTS -------------------------------------------------------------------------
    s_constr = time()

    # Disjunction -----------------------------------------
    disjunction = model.add_constraints(
        (X.sum(i, '*') <= 1
         for i in I),
        name="disjunction"
    )

    # Flow ------------------------------------------------
    flow_p = model.add_constraints(
        ((  gp.quicksum(Y.select(p, '*', p, l))
          - gp.quicksum(Y.select('*', p, p, l))) == 1
         for p in P
         for l in L),
        name="flow_p"
    )

    flow_i = model.add_constraints(
        ((  gp.quicksum(Y.select(i, '*', p, l))
          - gp.quicksum(Y.select('*', i, p, l))) == 0
         for i in I
         for p in P
         for l in L),
        name="flow_i"
    )

    flow_s = model.add_constraints(
        ((  gp.quicksum(Y.select(s, '*', p, l))
          - gp.quicksum(Y.select('*', s, p, l))) == -1
         for p in P
         for l in L),
        name="flow_s"
    )

    # Projection ------------------------------------------
    projection = model.add_constraints(
        (Y.sum(i, '*', p, l) <= X.sum(i, l)
         for i in I
         for p in P
         for l in L),
        name="projection"
    )

    # K-Coverage ------------------------------------------
    k_coverage = model.add_constraints(
        (gp.quicksum(X.select(iC[p], '*')) >= kcmc_k
         for p in P),
        name="k_coverage"
    )

    e_constr = time()

    # Return the model
    return model, X, Y, ModelSetupTime(model_setup=int(e_model-s_model),
                                       x_variable_setup=int(e_x-s_x),
                                       y_variable_setup=int(e_y-s_y),
                                       objective_function_setup=int(e_obj-s_obj),
                                       constraints_setup=int(e_constr-s_constr))


def gurobi_single_flow(
        kcmc_k:int, kcmc_m:int,
        kcmc_instance:KCMC_Instance,
        time_limit=3600,
        threads=1,
        y_binary=False
    ) -> (gp.Model, Any, Any, ModelSetupTime):

    # Prepare the model object --------------------------------------------------------------------
    s_model = time()
    model = gp.Model('single-flow')

    # Set the Time Limit and the Thread Count
    model.setParam(GRB.Param.TimeLimit, time_limit)
    model.setParam(GRB.Param.Threads, threads)

    # Set the LOG configuration
    model.setParam(GRB.Param.LogFile, '/tmp/gurobi.log')
    model.setParam(GRB.Param.OutputFlag, 1)
    model.setParam(GRB.Param.LogToConsole, 0)

    # Extract and apply the components
    P = kcmc_instance.pois
    I = kcmc_instance.sensors
    s = list(kcmc_instance.sinks)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!

    A_c = kcmc_instance.poi_edges
    A_s = kcmc_instance.sink_edges
    A_g = kcmc_instance.sensor_edges
    A = A_c + A_g + A_s

    iC = kcmc_instance.inverse_coverage_graph
    # L = [str(m) for m in range(kcmc_m)] not used in this formulation

    e_model = time()

    # Set the variables ---------------------------------------------------------------------------
    s_x = time()
    X = model.add_vars(I, name='x', vtype=GRB.BINARY)
    X.start = [1]*len(I)  # Start the first solution as using ALL SENSORS
    e_x = time()
    s_y = time()
    if y_binary:
        Y = model.add_vars(A, P, name='y', vtype=GRB.BINARY)
    else:
        Y = model.add_vars(A, P, name='y')
    e_y = time()

    # Set the objective function ------------------------------------------------------------------
    s_obj = time()
    model.set_objective(X.sum('*', '*'), GRB.MINIMIZE)
    e_obj = time()

    # Set the CONSTRAINTS -------------------------------------------------------------------------
    s_constr = time()

    # Flow ------------------------------------------------
    flow_p = model.add_constraints(
        ((  gp.quicksum(Y.select(p, '*', p))
          - gp.quicksum(Y.select('*', p, p))) == kcmc_m
         for p in P),
        name="flow_p"
    )

    flow_s = model.add_constraints(
        ((  gp.quicksum(Y.select(s, '*', p))
          - gp.quicksum(Y.select('*', s, p))) == -1 * kcmc_m
         for p in P),
        name="flow_s"
    )

    flow_i = model.add_constraints(
        ((  gp.quicksum(Y.select(i, '*', p))
          - gp.quicksum(Y.select('*', i, p))) == 0
         for i in I
         for p in P),
        name="flow_i"
    )

    # Projection ------------------------------------------
    projection = model.add_constraints(
        (Y.sum(i, '*', p) <= X.sum(i)
         for i in I
         for p in P),
        name="projection"
    )

    # K-Coverage ------------------------------------------
    k_coverage = model.add_constraints(
        (gp.quicksum(X.select(iC[p], '*')) >= kcmc_k
         for p in P),
        name="k_coverage"
    )

    e_constr = time()

    # Return the model
    return model, X, Y, ModelSetupTime(model_setup=int(e_model-s_model),
                                       x_variable_setup=int(e_x-s_x),
                                       y_variable_setup=int(e_y-s_y),
                                       objective_function_setup=int(e_obj-s_obj),
                                       constraints_setup=int(e_constr-s_constr))
