"""
Gurobi Optimizer Driver Script
"""


# STDLib
import json
import os.path
import logging
import argparse
import traceback
from datetime import datetime

# This package
from kcmc_instance import KCMC_Instance
from gurobi_models import get_installation, gurobi_multi_flow
from filelock import FileLock, FileLockException


def run_gurobi_optimizer(serialized_instance:str,
                         kcmc_k:int, kcmc_m:int,
                         time_limit:float, threads:int,
                         log:callable=print, LOGFILE:str=None,
                         model_factory=gurobi_multi_flow) -> dict:

    # Prepare the results metadata object
    metadata = {'kcmc_k': kcmc_k, 'kcmc_m': kcmc_m, 'time_limit': time_limit, 'threads': threads}

    # De-Serialize the instance as a KCMC_Instance object.
    # It MIGHT be multisink and thus incompatible with the ILP formulation
    instance = KCMC_Instance(serialized_instance, False, True, True)
    log(f'Parsed raw instance of key {instance.key_str}')

    # Add metadata to the result object
    metadata.update({
        'key': instance.key_str,
        'instance': serialized_instance,
        'pois': instance.num_pois,
        'sensors': instance.sensors,
        'sinks': instance.num_sinks,
        'area_side': instance.area_side,
        'coverage_radius': instance.sensor_coverage_radius,
        'communication_radius': instance.sensor_communication_radius,
        'coverage_density': instance.coverage_density,
        'communication_density': instance.communication_density
    })

    # WE ASSUME SINGLE-SINK INSTANCES
    # # Convert the instance to a single-sink version.
    # # Use M as the MAX_M parameter for minimal impact on performance
    # instance = maybe_multisink_instance.to_single_sink(MAX_M=kcmc_m)
    # log('Ensured single-sink instance')

    # MODEL SETUP ======================================================================================================

    # Build the model and its variables
    model, X, Y = model_factory(kcmc_k, kcmc_m, instance, time_limit, threads, LOGFILE)
    log('GUROBI Model set up')

    # MODEL EXECUTION ==================================================================================================

    # Run the execution
    log('STARTING OPTIMIZATION ' + ('*'*38))
    results = model.optimize()
    log('OPTIMIZATION DONE ' + ('*'*42))

    # Store some metadata
    results.update(metadata)
    log(f'OPTIMIZATION STATUS: {results["status"]}')
    log(f'QUANTITY OF SOLUTIONS FOUND: {results["solutions_count"]}')

    # Store the values of X
    if 'OPTIMAL' in results["status"]:
        tuplelist, installation = get_installation(X)
        results['X'] = tuplelist
        results['installation'] = installation

        # Parse the used spots
        results['used_spots'] = [s for s, i in results['installation'].items()]
        # results['raw']['installation'] = {instance.virtual_sinks_dict.get(s, s): t for s, t in results['single_sink']['installation'].items()}  # ASSUME SINGLE SINK
        # results['raw']['used_spots'] = [s for s, i in results['raw']['installation'].items()]  # ASSUME SINGLE SINK

    log('DONE '+('*'*55))
    return results


def process_instance(kcmc_k:int, kcmc_m:int, time_limit:int, threads:int, serialized_instance:str) -> (str, str):

    # Parse the KEY of the instance
    KEY = '_'.join(serialized_instance.split(';', 4)[:4])

    # Check if the LOG file already exists. If so, skip.
    RESULTS_KEY = f'/home/gurobi/results/{KEY}'
    if os.path.exists(RESULTS_KEY+'.json'): exit(0)

    # Start the logger
    logging.basicConfig(filename=RESULTS_KEY+'.log', level=logging.DEBUG)
    def log(logstring): logging.info(KEY+':::'+logstring)

    # Run the main execution, logging possible errors
    try:
        results = run_gurobi_optimizer(serialized_instance, kcmc_k, kcmc_m, time_limit, threads, log, LOGFILE=RESULTS_KEY+'.log')

        # Save the results on disk and exit with success
        with open(RESULTS_KEY+'.json', 'w') as fout:
            json.dump(results, fout, indent=2)
        return KEY, results['status']
    except Exception as exp:
        with open(RESULTS_KEY+'.err', 'a') as fout:
            fout.write(f'KEY {KEY} AT {datetime.now()}\n{traceback.format_tb(exp)}\n\n')
        return KEY, 'ERROR: '+str(exp)


# ######################################################################################################################
# RUNTIME


if __name__ == '__main__':

    # Set the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv_file", help="CSV file full of KCMC Instances")
    parser.add_argument("kcmc_k", type=int, help="KCMC K value to evaluate")
    parser.add_argument("kcmc_m", type=int, help="KCMC M value to evaluate")
    parser.add_argument('-l', '--limit', type=int, help='Time limit', default=60)
    parser.add_argument('-t', '--threads', type=int, help='Number of threads to use in the current gurobi process', default=1)
    args, unknown_args = parser.parse_known_args()

    # Parse the arguments
    input_csv_file = str(args.input_csv_file.strip())
    time_limit = float(str(args.limit))
    threads = int(float(str(args.threads)))
    kcmc_k = int(float(str(args.kcmc_k)))
    kcmc_m = int(float(str(args.kcmc_m)))
    assert kcmc_k >= kcmc_m, 'KCMC K MUST BE NO SMALLER THAN KCMC M!'
    assert os.path.exists(input_csv_file), f'INPUT CSV FILE {input_csv_file} DOES NOT EXISTS!'
    processed_instances = set()

    # For each line in the input file (assuming there is no header)
    with open(input_csv_file, 'r') as input_file:
        for line_no, serialized_instance in enumerate(input_file):

            # Get the serialized instance
            s_line = serialized_instance.strip().split(',')[0].strip()
            serialized_instance = s_line.split('\t')[0].strip().upper()
            if not serialized_instance.startswith('KCMC'): continue  # Only valid lines. Skip the rest

            # Check if the instance is pre-certified
            precert = s_line.split('\t')[-1].strip().upper() if (('\t' in s_line) and (s_line.endswith(')'))) else None
            if precert is not None:
                if f'(K{kcmc_k}M{kcmc_m})' != precert:
                    continue

            # Parse the KEY of the instance
            KEY = '_'.join(serialized_instance.split(';', 4)[:4])

            # Check if the RESULTS file already exists. If so, skip.
            RESULTS_KEY = f'/home/gurobi/results/{KEY}'
            if os.path.exists(RESULTS_KEY+'.json'): continue

            # If we do manage to acquire the LOCK to the results key:
            try:
                if serialized_instance in processed_instances: continue
                processed_instances.add(serialized_instance)
                with FileLock(RESULTS_KEY+'.log', timeout=None, delay=None):

                    # Start the logger
                    logging.basicConfig(filename=RESULTS_KEY+'.log', level=logging.DEBUG)
                    def log(logstring): logging.info(KEY+':::'+logstring)


                    # Run the main execution, logging possible errors
                    try:
                        results = run_gurobi_optimizer(serialized_instance, kcmc_k, kcmc_m, time_limit,
                                                       threads, log, LOGFILE=RESULTS_KEY+'.log')

                        # Save the results on disk and exit with success
                        with open(RESULTS_KEY+'.json', 'w') as fout:
                            json.dump(results, fout, indent=2)

                        # Printout the result status
                        result = '{}\t{} {}'.format(line_no, KEY, results['status'])
                    except Exception as exp:
                        with open(RESULTS_KEY+'.err', 'a') as fout:
                            fout.write(f'KEY {KEY} AT {datetime.now()}\nERROR - {exp}\n{traceback.format_exc()}\n\n')

                        # Printout the error
                        result = '{}\t{} {}'.format(line_no, KEY, 'ERROR: '+str(exp))

                    with open('/home/gurobi/results/STATE.log', 'a') as fout:
                        fout.write(f'{datetime.now()} {result}\n')

            # Locking exception
            except FileLockException: continue  # If we did not got the lock, skip this key.
