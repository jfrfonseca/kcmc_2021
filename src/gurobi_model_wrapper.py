"""
GUROBI Single-Flow and Multi-Flow ILP Model Objects Factory
"""


import time
import zlib
import json
import base64

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

    @staticmethod
    def duration(start, end):
        return end-start

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
                'unit': 'nano seconds',
                'setup': {
                    'model': self.duration(self.model_setup_start, self.model_setup_end),
                    'constraints': {key: self.duration(*values) for key, values in self.constraints_setup_time.items()},
                    'vars': {key: self.duration(*values) for key, values in self.vars_setup_time.items()},
                },
                'wall': self.duration(self.optimization_start, self.optimization_end),
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

