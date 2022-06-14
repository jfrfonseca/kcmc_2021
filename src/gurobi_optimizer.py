"""
Gurobi Optimizer Driver Script
"""


# STDLib
import os
import time
import json
import argparse
from datetime import timedelta

# PIP
import boto3

# Auxiliary packages
from python_dynamodb_lock.python_dynamodb_lock import DynamoDBLockClient, DynamoDBLockError

# Payload packages
from kcmc_instance import KCMC_Instance
from gurobi_models import gurobi_multi_flow, gurobi_single_flow


PROCESS_QUEUE_EXPIRE_TIME = 3000
MODELS = {
    'gurobi_y_binary_single_flow': (gurobi_single_flow, True, None),
    'gurobi_y_binary_multi_flow': (gurobi_multi_flow, True, None),
    'gurobi_single_flow': (gurobi_single_flow, False, None),
    'gurobi_multi_flow': (gurobi_multi_flow, False, None),
    # 'dinic__gurobi_single_flow',
    # 'dinic__gurobi_multi_flow',
    # 'minimal_flood__gurobi_single_flow',
    # 'minimal_flood__gurobi_multi_flow',
    # 'full_flood__gurobi_single_flow',
    # 'full_flood__gurobi_multi_flow',
    # 'reuse__gurobi_single_flow',
    # 'reuse__gurobi_multi_flow',
}


class MockLockObject(object):
    def __init__(self, lock_string):
        self.lock_string = lock_string
    def release(self): return True


def acquire_lock(lock_client:DynamoDBLockClient, lock_string:str) -> MockLockObject:
    if lock_client is None: return MockLockObject(lock_string)
    try:
        lock = lock_client.acquire_lock(
            lock_string,
            retry_timeout=timedelta(seconds=0.25),
            retry_period=timedelta(seconds=0.20)
        )
    except DynamoDBLockError as lkerr:
        if 'timed out' in str(lkerr).lower():
            return None
        else: raise lkerr
    return lock


def reset_process_queue(instances_file:str, models_list:list, s3_client, expire_time:int):
    process_queue = []

    # For each instance in the file
    with open(instances_file, 'r') as fin:
        for line in fin:

            # Load the key, K and M of the instance
            key = (';'.join(line.split(';', 4)[:-1]) + ';END').upper()
            kcmc_k = int(line.split('|')[-1].split('K')[-1].split('M')[0])
            kcmc_m = int(int(line.split('|')[-1].split('M')[-1].split(')')[0]))

            # For each model, add a line to the process queue
            for model in models_list:
                process_queue.append((key, kcmc_k, kcmc_m, model))

    # Find the already-processed results in the results store
    existing_results = []
    if s3_client is None:  # LOCAL FOLDER
        for result in os.listdir(results_store):
            if not result.endswith('.json'): continue
            key, kcmc_k, kcmc_m, model_name, _ = result.lower().split('.')
            kcmc_k, kcmc_m = map(int, [kcmc_k, kcmc_m])
            key = key.split('_')
            key = f'KCMC;{key[1]} {key[2]} {key[3]};{key[4]} {key[5]} {key[6]};{key[7]};END'
            existing_results.append((key, kcmc_k, kcmc_m, model_name))
    else:
        bucket = boto3.resource('s3').Bucket(bucket_name)
        all_items = s3_client.list_objects_v2(
            Bucket=bucket_name,
            Prefix=s3_path
        )
        assert len(all_items.get('Contents', [])) < 1000, 'MORE THAN 1000 ITEMS! SOME MIGHT HAVE BEEN LOST!'
        for item in all_items.get('Contents', []):
            key, kcmc_k, kcmc_m, model_name, _ = item['Key'].lower().rsplit('/', 1)[-1].split('.')
            kcmc_k, kcmc_m = map(int, [kcmc_k, kcmc_m])
            key = key.split('_')
            key = f'KCMC;{key[1]} {key[2]} {key[3]};{key[4]} {key[5]} {key[6]};{key[7]};END'
            existing_results.append((key, kcmc_k, kcmc_m, model_name))

    # Find the resulting queue
    process_queue = sorted(list(set(process_queue) - set(existing_results)))
    print(f'PROCESS QUEUE HAS {len(process_queue)} ({len(existing_results)} already done)')
    return process_queue, int(time.time())+expire_time


# ######################################################################################################################
# RUNTIME


if __name__ == '__main__':

    # Set the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-m', '--models', nargs='*', help='List of zero or more models to run. If none is specified, all models and preprocessors will be run')
    parser.add_argument('--instances_file', default='/data/instances.csv', help='Local source of instances. Default: /data/instances.csv')
    parser.add_argument('--lock', default='', help='Optional name of a DynamoDB LOCK TABLE to synchronize multi-node deployments')
    parser.add_argument('-l', '--limit', type=int, help='Time limit to run GUROBI, in seconds. Default: 3600', default=3600)
    parser.add_argument('-r', '--results', default='/results', help='Results store. May be an AWS S3 prefix. Defaults to /results')
    args, unknown_args = parser.parse_known_args()

    # Parse the simplest arguments
    threads = 1
    time_limit = float(str(args.limit))
    assert time_limit > 1, 'INVALID TIME LIMIT: '+str(time_limit)
    instances_file = args.instances_file.strip()
    assert os.path.isfile(instances_file), 'INVALID INSTANCES FILE: '+instances_file
    lock_table = None if len(str(args.lock).strip()) == 0 else str(args.lock).strip()

    # if we have a lock table, create a DynamoDB lock object on it
    if lock_table is None: lock_client = None
    else:
        # Create a Boto3 client; create the table if it does not exists and create a lock client
        dynamo_cli = boto3.client('dynamodb')
        dynamo = boto3.resource('dynamodb')
        try:
            response = dynamo_cli.describe_table(TableName=lock_table)
        except Exception as exp:
            if 'resource not found' in str(exp):
                DynamoDBLockClient.create_dynamodb_table(dynamo_cli, table_name=lock_table)
            else: raise exp
        lock_client = DynamoDBLockClient(dynamo, table_name=lock_table)  # TTL by default: 1 hour

    # Parse the models
    if args.models is None: models = list(MODELS.keys())
    elif len(args.models) == 0: models = list(MODELS.keys())
    else: models = [m.strip().lower() for m in args.models]
    unknown_models = set(models).difference(set(MODELS.keys()))
    assert len(unknown_models) == 0, 'UNKNOWN MODELS: '+', '.join(sorted(list(unknown_models)))

    # Parse the results store
    results_store = args.results.strip()

    # If the results store is an AWS S3 prefix, start an AWS S3 client
    if results_store.startswith('s3://'):
        s3_client = boto3.client('s3')
        _, _, bucket_name, s3_path = results_store.split('/', 3)
    else:
        s3_client = None

    # Prepare the process queue
    process_queue, process_queue_expiration = reset_process_queue(instances_file, models, s3_client, PROCESS_QUEUE_EXPIRE_TIME)

    # RUNTIME ----------------------------------------------------------------------------------------------------------

    # For each object to process
    for key, kcmc_k, kcmc_m, model_name in process_queue:

        # Acquire the lock to the object. If failure to acquire the lock, skip it
        results_file = f'{key}.{kcmc_k}.{kcmc_m}.{model_name}'.replace(';', '_').replace(' ', '_')
        lock = acquire_lock(lock_client, results_file)
        if lock is None: continue

        # Notify the processing of the instance
        print(f'PROCESSING {key} | {kcmc_k} | {kcmc_m} | {model_name}')

        # Load the instance as a python object
        instance = KCMC_Instance(key, False, True, False)

        # Get the arguments of the model
        model_factory, y_binary, prep_stage = MODELS[model_name]

        # If the model has a preprocessing stage, preprocess it
        if prep_stage:
            new_instance = instance.preprocess(kcmc_k, kcmc_m, prep_stage, raw=False)
            preprocessing = instance._prep.copy()

            # Notify the PRE processing of the instance
            print(f'\tPREPROCESSING {prep_stage}'
                  f' | {len(new_instance.sensors)}'
                  f' | {round(len(new_instance.inactive_sensors)*100.0 / len(instance.sensors), 3)}%')

            # Update the referred instance
            instance = new_instance

        else:
            prep_stage = ''
            preprocessing = {}

        if model_factory:

            # Build the model and its variables
            model, X, Y = model_factory(kcmc_k, kcmc_m, instance,
                                        time_limit=time_limit, threads=threads,
                                        LOGFILE='/tmp/gurobi.log', y_binary=y_binary)

            # Run the model
            results = model.optimize(compress_variables=True)

            # Get the LOGs
            with open('/tmp/gurobi.log', 'r') as fin:
                gurobi_logs = '\n'.join([l.replace('\n', '') for l in fin.readlines() if len(l.strip()) > 0])
            os.unlink('/tmp/gurobi.log')

            # Update the results with some more metadata
            results.update({
                'gurobi_y_binary': y_binary,
                'gurobi_model': model_name,
                'gurobi_logs': gurobi_logs
            })

        # If no GUROBI stage:
        else:
            results = {'gurobi_model': None, 'gurobi_logs': None, 'gurobi_y_binary': None}

        # Enrich the results
        results.update({
            'key': key, 'kcmc_k': kcmc_k, 'kcmc_m': kcmc_m,
            'pois': len(instance.pois), 'sensors': len(instance.sensors), 'sinks': len(instance.sinks),
            'coverage_radius': instance.sensor_coverage_radius,
            'communication_radius': instance.sensor_communication_radius,
            'random_seed': instance.random_seed,
            'model': model_name, 'prep_stage': prep_stage,
            'time_limit': time_limit, 'threads': threads,
            'coverage_density': instance.coverage_density,
            'communication_density': instance.communication_density,
            'preprocessing': preprocessing
        })

        # Store the results
        if s3_client is None:
            results_file = os.path.join(results_store, f'{results_file}.json')
            with open(results_file, 'w') as fout:
                json.dump(results, fout)
        else:
            results_file = results_file+'.json'
            with open('/tmp/'+results_file, 'w') as fout:
                json.dump(results, fout)
            s3_client.upload_file('/tmp/'+results_file, bucket_name, os.path.join(s3_path, results_file))

        # Release the lock
        lock.release()
        print(f'DONE {results["objective_value"]}')

        # Reset the process queue if expired
        if time.time() > process_queue_expiration:
            process_queue, process_queue_expiration = reset_process_queue(instances_file, models, s3_client, PROCESS_QUEUE_EXPIRE_TIME)

    print('DONE WITH A RUN OF THE QUEUE!')
