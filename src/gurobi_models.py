"""
GUROBI Single-Flow and Multi-Flow ILP Model Objects Factory
"""

import re
import json
from typing import Any, Union, Dict
from dataclasses import dataclass

import gurobipy as gp
from gurobipy import GRB

import ijson

from kcmc_instance import KCMC_Instance
from gurobi_model_wrapper import GurobiModelWrapper


PRESOLVE_REGEX_1 = re.compile(r'''Optimize a model with (\d+) rows, (\d+) columns and (\d+) nonzeros
Model fingerprint: (\w+)
Variable types: (\d+) continuous, (\d+) integer \((\d+) binary\)''')


# Even if we have more than one "Presolve removed..." lines, we only get the last - the one that precedes the "Presolve time" line
PRESOLVE_REGEX_2 = re.compile(r'''Presolve removed (\d+) rows and (\d+) columns
Presolve time: ([\d.]+)s
Presolved: (\d+) rows, (\d+) columns, (\d+) nonzeros
Variable types: (\d+) continuous, (\d+) integer \((\d+) binary\)''')


# Even if we have more than one "Presolve removed..." lines, we only get the last - the one that precedes the "Presolve time" line
PRESOLVE_REGEX_3 = re.compile(r'''Presolve removed (\d+) rows and (\d+) columns
Presolve time: ([\d.]+)s
Presolved: (\d+) rows, (\d+) columns, (\d+) nonzeros
Found heuristic solution: objective ([\d.]+)
Variable types: (\d+) continuous, (\d+) integer \((\d+) binary\)''')


@dataclass
class KCMC_Result:
    source_file: str

    # Key Attributes ------------------
    pois: int
    sensors: int
    sinks: int

    area_side: int
    coverage_radius: int
    communication_radius: int

    random_seed: int

    k: int
    m: int

    heuristic_name: str
    y_binary: bool
    gurobi_model_name: str

    # Main results --------------------
    heuristic_solution: str
    # heuristic_objective_value
    # heuristic_solution_size
    # heuristic_solution_quality
    gurobi_optimal: bool
    gurobi_objective_value: float
    gurobi_heuristic_objective_value: Union[float, None]
    gurobi_solution: str
    solution: dict
    # solution_size
    # solution_quality

    # Time ----------------------------
    heuristic_time: float
    gurobi_setup_time: float
    gurobi_run_time: float
    # total_time: float

    # Other Solver results ------------
    gurobi_mip_gap: float
    gurobi_bound: float
    gurobi_bound_c: float
    gurobi_node_count: int
    gurobi_solutions_count: int
    gurobi_simplex_iterations_count: int
    gurobi_initial_rows_count: int                  # LOG-Parsed
    gurobi_initial_columns_count: int               # LOG-Parsed
    gurobi_initial_non_zero_count: int              # LOG-Parsed
    gurobi_initial_binary_variables_count: int      # LOG-Parsed
    gurobi_initial_integer_variables_count: int     # LOG-Parsed
    gurobi_initial_continuous_variables_count: int  # LOG-Parsed
    gurobi_presolve_time: float                     # LOG-Parsed
    gurobi_presolve_removed_rows: int               # LOG-Parsed
    gurobi_presolve_removed_columns: int            # LOG-Parsed
    gurobi_rows_count: int                          # LOG-Parsed
    gurobi_columns_count: int                       # LOG-Parsed
    gurobi_non_zero_count: int                      # LOG-Parsed
    gurobi_binary_variables_count: int              # LOG-Parsed
    gurobi_integer_variables_count: int             # LOG-Parsed
    gurobi_continuous_variables_count: int          # LOG-Parsed

    # Other attributes ----------------
    time_limit: float
    gurobi_logs: str
    gurobi_variable_x_size: int
    gurobi_variable_y_size: int

    # Derived attributes ----------------------------------

    @property
    def heuristic_objective_value(self) -> int: return sum(int(i) for i in self.heuristic_solution)
    @property
    def heuristic_solution_size(self) -> float: return float(self.heuristic_objective_value)
    @property
    def heuristic_solution_quality(self) -> float: return (self.sensors-self.heuristic_solution_size)/self.sensors

    @property
    def solution_size(self) -> int: return sum(int(i) for i in self.solution)
    @property
    def solution_quality(self) -> float: return (self.sensors-self.solution_size)/self.sensors

    @property
    def total_time(self) -> float: return sum([self.heuristic_time, self.gurobi_setup_time, self.gurobi_run_time])

    # INSTANCE KEY --------------------

    @property
    def instance_key(self) -> str:
        return 'KCMC;'+';'.join([
            f'{self.pois} {self.sensors} {self.sinks}',
            f'{self.area_side} {self.coverage_radius} {self.communication_radius}',
            str(self.random_seed),
            f'END'
        ])

    @property
    def serial_instance(self) -> str:
        return self.instance_key+f'|(K{self.k}M{self.m})'

    # RESULT KEY ----------------------

    @property
    def result_key(self) -> str:
        return 'KCMC;'+';'.join([
            f'{self.pois} {self.sensors} {self.sinks}',
            f'{self.area_side} {self.coverage_radius} {self.communication_radius}',
            str(self.random_seed),
            f'{self.k} {self.m} {self.heuristic_name} {self.gurobi_model_name}{"*" if self.y_binary else ""}'
        ])
    def __str__(self) -> str: return self.result_key
    def __repr__(self) -> str: return self.result_key
    def __hash__(self): return self.result_key.__hash__()

    # Services --------------------------------------------

    def asdict(self) -> dict:
        data = vars(self)
        data.update({
            'heuristic_objective_value': self.heuristic_objective_value,
            'heuristic_solution_size': self.heuristic_solution_size,
            'heuristic_solution_quality': self.heuristic_solution_quality,
            'solution_size': self.solution_size,
            'solution_quality': self.solution_quality,
            'total_time': self.total_time,
            'instance_key': self.instance_key,
            'serial_instance': self.serial_instance,
            'result_key': self.result_key,
        })
        return data.copy()
    def to_dict(self) -> dict: return self.asdict()

    @staticmethod
    def parse_variable(variable:dict) -> dict:
        if isinstance(variable, str): variable = json.loads(GurobiModelWrapper.decompress(variable))
        return variable

    @staticmethod
    def get_solution(variable_X:dict, num_sensors:int):
        variable_X = KCMC_Result.parse_variable(variable_X)

        # Form the BitTrain solution
        solution = ['0']*num_sensors
        for isid, active in variable_X.items():
            if active:
                if "(" in isid: isid = eval(isid)[0]
                solution[int(isid[1:])] = '1'
        return ''.join(solution)

    @staticmethod
    def get_variable_size(variable:str, decompress=True) -> int:
        variable = GurobiModelWrapper.decompress(variable) if decompress else variable
        i = 0
        for prefix, event, value in ijson.parse(variable):
            if event == 'map_key':
                i += 1
        return i

    @staticmethod
    def parse_presolve(gurobi_log):

        # Initial values
        ini_rows, ini_cols, ini_n0, _, ini_cont, ini_int, ini_bin = PRESOLVE_REGEX_1.search(gurobi_log).groups()

        # Values and presolve
        match_2 = PRESOLVE_REGEX_2.search(gurobi_log)
        if match_2:
            prem_rows, prem_cols, pre_t, \
            end_rows, end_cols, end_n0, \
            end_cont, end_int, end_bin \
                = match_2.groups()
            gurobi_heuristic_objective_value = None
        else:
            match_2 = PRESOLVE_REGEX_3.search(gurobi_log)
            prem_rows, prem_cols, pre_t, \
            end_rows, end_cols, end_n0, \
            gurobi_heuristic_objective_value, \
            end_cont, end_int, end_bin \
                = match_2.groups()

        # Format and return
        return int(ini_rows), int(ini_cols), int(ini_n0), \
               int(ini_cont), int(ini_int), int(ini_bin), \
               int(end_rows), int(end_cols), int(end_n0), \
               int(end_cont), int(end_int), int(end_bin), \
               int(prem_rows), int(prem_cols), float(pre_t), \
               float(gurobi_heuristic_objective_value) if gurobi_heuristic_objective_value else None


"""
# ######################################################################################################################
# MULTI-FLOW
"""


def gurobi_multi_flow(kcmc_k:int, kcmc_m:int, kcmc_instance:KCMC_Instance, time_limit=60, threads=1, LOGFILE=None,
                      y_binary=True) -> (GurobiModelWrapper, Any, Any):

    # Prepare the model object, using the wrapper
    model_mf = GurobiModelWrapper('KCMC MULTI-FLOW', kcmc_k, kcmc_m, kcmc_instance, time_limit, threads, LOGFILE)

    # Extract the component sets
    P = kcmc_instance.pois
    I = kcmc_instance.sensors
    s = list(kcmc_instance.sinks)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!

    A_c = kcmc_instance.poi_edges
    A_s = kcmc_instance.sink_edges
    A_g = kcmc_instance.sensor_edges
    A = A_c + A_g + A_s

    iC = kcmc_instance.inverse_coverage_graph
    L = [str(m) for m in range(kcmc_m)]

    # Set the variables
    X = model_mf.add_vars(I, L, name='x', vtype=GRB.BINARY)
    if y_binary:
        Y = model_mf.add_vars(A, P, L, name='y', vtype=GRB.BINARY)
    else:
        Y = model_mf.add_vars(A, P, L, name='y')

    # Set the objective function
    model_mf.set_objective(X.sum('*', '*'), GRB.MINIMIZE)

    # Set the CONSTRAINTS -------------------------------------------------------------------------

    # Disjunction -----------------------------------------
    disjunction = model_mf.add_constraints(
        (X.sum(i, '*') <= 1
         for i in I),
        name="disjunction"
    )

    # Flow ------------------------------------------------
    flow_p = model_mf.add_constraints(
        ((  gp.quicksum(Y.select(p, '*', p, l))
          - gp.quicksum(Y.select('*', p, p, l))) == 1
         for p in P
         for l in L),
        name="flow_p"
    )

    flow_i = model_mf.add_constraints(
        ((  gp.quicksum(Y.select(i, '*', p, l))
          - gp.quicksum(Y.select('*', i, p, l))) == 0
         for i in I
         for p in P
         for l in L),
        name="flow_i"
    )

    flow_s = model_mf.add_constraints(
        ((  gp.quicksum(Y.select(s, '*', p, l))
          - gp.quicksum(Y.select('*', s, p, l))) == -1
         for p in P
         for l in L),
        name="flow_s"
    )

    # Projection ------------------------------------------
    projection = model_mf.add_constraints(
        (Y.sum(i, '*', p, l) <= X.sum(i, l)
         for i in I
         for p in P
         for l in L),
        name="projection"
    )

    # K-Coverage ------------------------------------------
    k_coverage = model_mf.add_constraints(
        (gp.quicksum(X.select(iC[p], '*')) >= kcmc_k
         for p in P),
        name="k_coverage"
    )

    # Return the model
    return model_mf, X, Y


"""
# ######################################################################################################################
# SINGLE-FLOW
"""


def gurobi_single_flow(kcmc_k:int, kcmc_m:int, kcmc_instance:KCMC_Instance, time_limit=60, threads=1, LOGFILE=None,
                       y_binary=True) -> (GurobiModelWrapper, Any, Any):

    # Prepare the model object, using the wrapper
    model_sf = GurobiModelWrapper('KCMC SINGLE-FLOW', kcmc_k, kcmc_m, kcmc_instance, time_limit, threads, LOGFILE)

    # Extract the component sets
    P = kcmc_instance.pois
    I = kcmc_instance.sensors
    s = list(kcmc_instance.sinks)[0]  # HARD-CODED ASSUMPTION OF SINGLE-SINK!

    A_c = kcmc_instance.poi_edges
    A_s = kcmc_instance.sink_edges
    A_g = kcmc_instance.sensor_edges
    A = A_c + A_g + A_s

    iC = kcmc_instance.inverse_coverage_graph
    # L = [str(m) for m in range(kcmc_m)] not used in this formulation

    # Set the variables
    X = model_sf.add_vars(I, name='x', vtype=GRB.BINARY)
    if y_binary:
        Y = model_sf.add_vars(A, P, name='y', vtype=GRB.BINARY)
    else:
        Y = model_sf.add_vars(A, P, name='y')

    # Set the objective function
    model_sf.set_objective(X.sum('*'), GRB.MINIMIZE)

    # Set the CONSTRAINTS -------------------------------------------------------------------------

    # Flow ------------------------------------------------
    flow_p = model_sf.add_constraints(
        ((  gp.quicksum(Y.select(p, '*', p))
          - gp.quicksum(Y.select('*', p, p))) == kcmc_m
         for p in P),
        name="flow_p"
    )

    flow_s = model_sf.add_constraints(
        ((  gp.quicksum(Y.select(s, '*', p))
          - gp.quicksum(Y.select('*', s, p))) == -1 * kcmc_m
         for p in P),
        name="flow_s"
    )

    flow_i = model_sf.add_constraints(
        ((  gp.quicksum(Y.select(i, '*', p))
          - gp.quicksum(Y.select('*', i, p))) == 0
         for i in I
         for p in P),
        name="flow_i"
    )

    # Projection ------------------------------------------
    projection = model_sf.add_constraints(
        (Y.sum(i, '*', p) <= X.sum(i)
         for i in I
         for p in P),
        name="projection"
    )

    # K-Coverage ------------------------------------------
    k_coverage = model_sf.add_constraints(
        (gp.quicksum(X.select(iC[p], '*')) >= kcmc_k
         for p in P),
        name="k_coverage"
    )

    # Return the model
    return model_sf, X, Y
