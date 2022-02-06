"""
Gurobi Optimizer Driver Script
"""


# STDLib
import json
import os.path
import logging
import argparse
import traceback
from random import randint
from datetime import datetime

# This package
from kcmc_instance import KCMC_Instance
from gurobi_models import get_installation, gurobi_multi_flow, gurobi_single_flow
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


def process_instance(kcmc_k:int, kcmc_m:int, time_limit:int, threads:int, serialized_instance:str, model_factory='multi') -> (str, str):

    if isinstance(model_factory, str):
        model_factory = {
            'multi': gurobi_multi_flow, 'multi_flow': gurobi_multi_flow, 'multiflow': gurobi_multi_flow,
            'single': gurobi_single_flow, 'single_flow': gurobi_single_flow, 'singleflow': gurobi_single_flow
        }[model_factory]

    # Parse the KEY of the instance
    KEY = '_'.join(serialized_instance.split(';', 4)[:4])

    # Check if the LOG file already exists. If so, skip.
    RESULTS_KEY = f'/home/gurobi/results/{KEY}'+('.multi' if model_factory == gurobi_multi_flow else '.single')
    if os.path.exists(RESULTS_KEY+'.json'): exit(0)

    # Start the logger
    logging.basicConfig(filename=RESULTS_KEY+'.log', level=logging.DEBUG)
    def log(logstring): logging.info(KEY+':::'+logstring)

    # Run the main execution, logging possible errors
    try:
        results = run_gurobi_optimizer(serialized_instance, kcmc_k, kcmc_m, time_limit, threads, log,
                                       LOGFILE=RESULTS_KEY+'.log', model_factory=model_factory)

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
    parser.add_argument("input_csv_file", help="CSV file full of KCMC Instances, each with its values to K and M")
    parser.add_argument('-l', '--limit', type=int, help='Time limit', default=3600)
    parser.add_argument('-t', '--threads', type=int, help='Number of threads to use in gurobi process', default=1)
    parser.add_argument('--skip_multi', action='store_true', help='If the multiflow variations should be skipped')
    args, unknown_args = parser.parse_known_args()

    # Get the name of the STATE file. Added random number to improve thread-safety!
    STATEFILE = f'/home/gurobi/results/STATE.{randint(10000, 99999)}.log'

    # Parse the arguments
    input_csv_file = str(args.input_csv_file.strip())
    time_limit = float(str(args.limit))
    threads = int(float(str(args.threads)))
    assert os.path.exists(input_csv_file), f'INPUT CSV FILE {input_csv_file} DOES NOT EXISTS!'

    # For each line in the input file (assuming there is no header)
    with open(input_csv_file, 'r') as input_file:
        for line_no, serialized_instance in enumerate(input_file):

            # Get the instance and the values for K and M
            serialized_instance, kcmc = map(lambda l: l.strip().upper(), serialized_instance.strip().split('|'))
            if not serialized_instance.startswith('KCMC'): continue  # Only valid lines. Skip the rest
            kcmc_k = int(kcmc.split('K')[-1].split('M')[0])
            kcmc_m = int(kcmc.split('M')[-1].split(')')[0])
            assert kcmc_k >= kcmc_m, 'KCMC K MUST BE NO SMALLER THAN KCMC M!'

            # Parse the KEY of the instance
            KEY = '_'.join(serialized_instance.split(';', 4)[:4])

            # Since we have two variations of the gurobi optimizer formulation, we test with both
            for MODEL_TYPE, factory in [('.single', gurobi_single_flow), ('.multi', gurobi_multi_flow)]:
                if args.skip_multi and (MODEL_TYPE == '.multi'): continue

                # Check if the RESULTS file already exists. If so, skip.
                RESULTS_KEY = f'/home/gurobi/results/{KEY}{MODEL_TYPE}'
                if os.path.exists(RESULTS_KEY+'.json'): continue

                # If we do manage to acquire the LOCK to the results key:
                try:
                    with FileLock(RESULTS_KEY+'.log', timeout=None, delay=None):

                        # Start the logger
                        logging.basicConfig(filename=RESULTS_KEY+'.log', level=logging.DEBUG)
                        def log(logstring): logging.info(KEY+MODEL_TYPE+':::'+logstring)

                        # Run the main execution, logging possible errors
                        try:
                            results = run_gurobi_optimizer(serialized_instance, kcmc_k, kcmc_m, time_limit,
                                                           threads, log, LOGFILE=RESULTS_KEY+'.log',
                                                           model_factory=factory)
                            results['gurobi_model_type'] = MODEL_TYPE[1:]+'_flow'

                            # Save the results on disk and exit with success
                            with open(RESULTS_KEY+'.json', 'w') as fout:
                                json.dump(results, fout, indent=2)

                            # Printout the result status
                            result = '{}\t{} {} {}'.format(line_no, KEY+MODEL_TYPE, results['status'], results['mip_gap'])
                        except Exception as exp:
                            with open(RESULTS_KEY+'.err', 'a') as fout:
                                fout.write(f'KEY {KEY+MODEL_TYPE} AT {datetime.now()}\nERROR - {exp}\n{traceback.format_exc()}\n\n')

                            # Printout the error
                            result = '{}\t{} {}'.format(line_no, KEY+MODEL_TYPE, 'ERROR: '+str(exp))

                        with open(STATEFILE, 'a') as fout:
                            fout.write(f'{datetime.now()} {result}\n')

                # Locking exception
                except FileLockException: continue  # If we did not got the lock, skip this key.
