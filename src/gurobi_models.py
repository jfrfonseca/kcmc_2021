"""
GUROBI Single-Flow and Multi-Flow ILP Model Objects Factory
"""


import time
import zlib
import json
import base64
from typing import Any

import gurobipy as gp
from gurobipy import GRB

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


def get_installation(variable_X):
    tuplelist = []
    installation = {}
    for x in variable_X:
        if isinstance(x, str):
            steiner_tree_id = '0'
            installation_spot_id = str(x)
        else:
            installation_spot_id, steiner_tree_id = x
        sensor_installed = bool(variable_X[x].X)
        tuplelist.append((installation_spot_id, steiner_tree_id, sensor_installed))
        if sensor_installed:
            installation[installation_spot_id] = steiner_tree_id
    return tuplelist, installation


class GurobiModelWrapper(object):

    def __init__(self, name:str, kcmc_k:int, kcmc_m:int, kcmc_instance:KCMC_Instance,
                 time_limit:int, threads:int, LOGFILE=None):

        # Store the params
        self.name = name
        self.kcmc_k = kcmc_k
        self.kcmc_m = kcmc_m
        self.time_limit = time_limit
        self.threads = threads
        self.kcmc_instance = kcmc_instance

        # Prepare the base params of the model
        self.model_setup_start = time.time_ns()
        model = gp.Model(self.name)

        # Set the Time Limit and the Thread Count
        model.setParam(GRB.Param.TimeLimit, time_limit)
        model.setParam(GRB.Param.Threads, threads)

        # Set the LOG configuration
        if LOGFILE is not None: model.setParam(GRB.Param.LogFile, LOGFILE)
        model.setParam(GRB.Param.OutputFlag, 1)
        model.setParam(GRB.Param.LogToConsole, 0)
        self.model_setup_end = time.time_ns()

        # Copy the model locally
        self.model = model
        self.results = None
        self.constraints = {}
        self.solution_variables = {}
        self.vars_setup_time = {}
        self.constraints_setup_time = {}

    # Configuration Service Wrapers
    def add_vars(self, *args, **kwargs):
        start = time.time_ns()
        var = self.model.addVars(*args, **kwargs)
        self.vars_setup_time[kwargs['name']] = (start, time.time_ns())
        self.solution_variables[kwargs['name']] = var
        return var
    def set_objective(self, *args, **kwargs): return self.model.setObjective(*args, **kwargs)
    def add_constraints(self, *args, name:str, **kwargs):
        start = time.time_ns()
        constr = self.model.addConstrs(*args, name=name, **kwargs)
        self.constraints_setup_time[name] = (start, time.time_ns())
        self.constraints[name] = constr
        return constr

    @staticmethod
    def compress(str_input:str) -> str:
        return base64.b64encode(zlib.compress(str_input.encode('utf-8'))).decode('ascii')

    @staticmethod
    def decompress(str_input):
        return zlib.decompress(base64.b64decode(str_input.encode('ascii'))).decode('utf-8')

    # Results Wrapper
    def optimize(self, compress_variables=False):
        if self.results is not None: return self.results.copy()

        # Run the execution
        self.optimization_start = time.time_ns()
        self.model.optimize()
        self.optimization_end = time.time_ns()

        # Warn the model that it has ended
        self.model.setParam(GRB.Param.OutputFlag, 0)

        # Store some metadata
        STATUS = GUROBI_STATUS_TRANSLATE.get(self.model.Status, f'ERROR ({self.model.Status})')
        self.results = {
            'time': {
                'setup': {
                    'model': (self.model_setup_start, self.model_setup_end),
                    'constraints': self.constraints_setup_time,
                    'vars': self.vars_setup_time,
                },
                'wall': (self.optimization_start, self.optimization_end),
            },
            'gurobi_runtime': self.model.Runtime,
            'status_code': self.model.Status,
            'status': STATUS.split(' ')[0],
            'mip_gap': self.model.MIPGap,
            'gurobi_model_fingerprint': str(self.model.Fingerprint),
            'binary_variables': self.model.NumBinVars,
            'solutions_count': self.model.SolCount,
            'node_count': self.model.NodeCount,
            'simplex_iterations_count': self.model.IterCount,
            'json_solution': json.loads(self.model.getJSONSolution()),
            'variables': {
                name: {str(k): (v.X if self.model.SolCount > 0
                                    else (v.Xn[0] if hasattr(v, 'Xn') else None))
                       for k,v in var.items()}
                for name, var in self.solution_variables.items()
            }
        }
        self.results['objective_value'] = self.results['json_solution']['SolutionInfo'].get('ObjVal', None)
        if compress_variables:
            self.results['variables'] = {name: self.compress(json.dumps(val))
                                         for name, val in self.results['variables'].items()}

        return self.results.copy()


def gurobi_multi_flow(kcmc_k:int, kcmc_m:int, kcmc_instance:KCMC_Instance, time_limit=60, threads=1, LOGFILE=None
                      ) -> (GurobiModelWrapper, Any, Any):

    # Prepare the model object, using the wrapper
    model = GurobiModelWrapper('KCMC MULTI-FLOW', kcmc_k, kcmc_m, kcmc_instance, time_limit, threads, LOGFILE)

    # Extract the component sets
    iC = kcmc_instance.inverse_coverage_graph
    L = [str(m) for m in range(kcmc_m)]
    P = kcmc_instance.pois
    I = kcmc_instance.sensors
    S = kcmc_instance.sinks
    s = list(S)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!
    A_c = kcmc_instance.poi_edges
    A_s = kcmc_instance.sink_edges
    A_g = kcmc_instance.sensor_edges
    A = A_c + A_g + A_s

    # Set the variables
    X = model.add_vars(I, L, name='x', vtype=GRB.BINARY)
    Y = model.add_vars(A, P, L, name='y')  # , vtype=GRB.BINARY)

    # Set the objective function
    model.set_objective(X.sum('*', '*'), GRB.MINIMIZE)

    # Set the CONSTRAINTS -------------------------------------------------------------------------

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

    # Return the model
    return model, X, Y


def gurobi_single_flow(kcmc_k:int, kcmc_m:int, kcmc_instance:KCMC_Instance, time_limit=60, threads=1, LOGFILE=None
                       ) -> (GurobiModelWrapper, Any, Any):

    # Prepare the model object, using the wrapper
    model = GurobiModelWrapper('KCMC SINGLE-FLOW', kcmc_k, kcmc_m, kcmc_instance, time_limit, threads, LOGFILE)

    # Extract the component sets
    iC = kcmc_instance.inverse_coverage_graph
    P = kcmc_instance.pois
    I = kcmc_instance.sensors
    S = kcmc_instance.sinks
    s = list(S)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!
    A_c = kcmc_instance.poi_edges
    A_s = kcmc_instance.sink_edges
    A_g = kcmc_instance.sensor_edges
    A = A_c + A_g + A_s

    # Set the variables
    X = model.add_vars(I, name='x', vtype=GRB.BINARY)
    Y = model.add_vars(A, P, name='y')  # , vtype=GRB.BINARY)

    # Set the objective function
    model.set_objective(X.sum('*'), GRB.MINIMIZE)

    # Set the CONSTRAINTS -------------------------------------------------------------------------

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

    # Return the model
    return model, X, Y
