"""
Gurobi Optimizer Driver Script
"""


# STDLib
import os
import argparse
from typing import Any
from os import unlink, makedirs
from datetime import timedelta

# PIP
import boto3
import ijson
import simplejson as json
from boto3.dynamodb.conditions import Key, Attr
try:
    import tqdm
except Exception as exp:
    def tqdm(iterator, **kwargs):
        for item in iterator:
            yield item

# Auxiliary packages
from dynamodb_lock import DynamoDBLockClient, DynamoDBLockError

# Payload packages
from kcmc_instance import KCMC_Instance
from gurobi_models import gurobi_multi_flow, gurobi_single_flow, GurobiModelWrapper
from instances_pump import parse_instance_row, INSTANCE_TABLE, LOCK_TABLE, LOCAL_URI


def run_gurobi_optimizer(serialized_instance:str,
                         kcmc_k:int, kcmc_m:int,
                         time_limit:float, threads:int,
                         LOGFILE:str=None,
                         model_factory=gurobi_multi_flow) -> dict:

    # De-Serialize the instance as a KCMC_Instance object.
    # It MIGHT be multisink and thus incompatible with the ILP formulation
    instance = KCMC_Instance(serialized_instance, False, True, True)
    print(f'\tParsed raw instance of key {instance.key_str} (seed {instance.random_seed})')
    # WE ASSUME SINGLE-SINK INSTANCES

    # MODEL SETUP ======================================================================================================

    # Build the model and its variables
    model, X, Y = model_factory(kcmc_k, kcmc_m, instance, time_limit, threads, LOGFILE)
    print(f'\tGUROBI Model {model.name} set up')

    # MODEL EXECUTION ==================================================================================================

    # Run the execution
    print('\tSTARTING OPTIMIZATION ' + ('*'*38))
    results = model.optimize()
    print('\tOPTIMIZATION DONE ' + ('*'*42))

    # Store some metadata
    results.update({
        'time_limit': time_limit, 'threads': threads,
        'coverage_density': instance.coverage_density,
        'communication_density': instance.communication_density
    })
    print(f'\tOPTIMIZATION STATUS: {results["status"]}')
    print(f'\tQUANTITY OF SOLUTIONS FOUND: {results["solutions_count"]}')

    print('\tDONE '+('*'*55))
    return results


# ######################################################################################################################
# RUNTIME


def acquire_instance(table, lock_client:DynamoDBLockClient, instances_queue:list) -> (Any, dict, set):

    # Check if the queue is done
    if len(instances_queue) == 0:
        print('DONE WITH QUEUE')
        return None, None, []

    # Get the first instance from the list
    instance_key = instances_queue[0]
    response = table.query(KeyConditionExpression=Key('instance_key').eq(instance_key), Limit=1)
    instance = response['Items'][0]
    if not instance['queued']:
        print('(UNQUEUED) SKIPPING INSTANCE '+instance['instance_key'])
        return acquire_instance(table, lock_client, instances_queue[1:])
    print('TRYING INSTANCE '+instance['instance_key'])

    # Get a lock on the returned instance
    try:
        lock = lock_client.acquire_lock(instance['instance_key'],
                                        retry_timeout=timedelta(seconds=0.15),
                                        retry_period=timedelta(seconds=0.1))

    # If failed to acquire lock, it means another instance has the lock. Skip the instance and try again
    except DynamoDBLockError as lkerr:
        if 'timed out' in str(lkerr).lower():

            # CYCLE LOGIC
            # print('DELAYING INSTANCE '+instance['instance_key'])
            # return acquire_instance(table, lock_client, instance_keys_list[1:]+[instance_keys_list[0]])

            # SKIP LOGIC
            print('(LOCKED) SKIPPING INSTANCE '+instance['instance_key'])
            return acquire_instance(table, lock_client, instances_queue[1:])

        else: raise lkerr

    # Return the lock and the instance
    print('GOT LOCK ON INSTANCE '+instance['instance_key'])
    return lock, instance, instances_queue[1:]


def get_update_params(body, stringfy=True):
    update_expression = ["set "]
    update_values = {}
    for key, val in body.items():
        update_expression.append(f" {key} = :{key},")
        update_values[f":{key}"] = str(val) if stringfy else val
    return "".join(update_expression)[:-1], update_values


def process_instance(time_limit:float, threads:int, MODEL_TYPE:str, instance:dict, factory) -> dict:

    # De-compress the instance
    serialized_instance = GurobiModelWrapper.decompress(instance['serial'])
    kcmc_k = int(instance['K'])
    kcmc_m = int(instance['M'])
    assert kcmc_k >= kcmc_m, 'KCMC K MUST BE NO SMALLER THAN KCMC M!'

    # Run the main execution, logging possible errors
    results = run_gurobi_optimizer(serialized_instance, kcmc_k, kcmc_m,
                                   time_limit, threads,
                                   LOGFILE='/tmp/dynamodb_objects/gurobi.log', model_factory=factory)
    results.update(instance)
    results['gurobi_model_type'] = MODEL_TYPE[1:]+'_flow'

    # Get the logs, clearing the files
    try:
        with open('/tmp/dynamodb_objects/gurobi.log', 'r') as fin:
            results['gurobi_logs'] = fin.readlines()
        unlink('/tmp/dynamodb_objects/gurobi.log')
    except Exception:
        results['gurobi_logs'] = None

    # Return the processed data
    return results


if __name__ == '__main__':

    print('SETTING UP...')
    try:
        makedirs('/tmp/dynamodb_objects')
    except: pass

    # Set the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--limit', type=int, help='Time limit, in seconds. Default: 3600', default=3600)
    parser.add_argument('-t', '--threads', type=int, help='Number of threads to use in gurobi process. Default: 1', default=1)
    parser.add_argument('--skip_multi', action='store_true', help='If the multiflow formulation should be skipped. Default: False')
    parser.add_argument('--local_dynamodb', action='store_true', help='If we should use the local dynamodb. Default: False')
    parser.add_argument('--bucket', default='kcmc-heuristic', help='S3 Bucket to store large results. Default: kcmc-heuristic')
    parser.add_argument('--instances_file', default='/data/instances.csv', help='Use a local instances file instead of DynamoDB. Default: /data/instances.csv')
    parser.add_argument('-i', '--instance_test', nargs='*', help='Number of the row of the local instances file to test. If provided, the DynamoDB instances will be ignored.')
    parser.add_argument('-o', '--output', default='/results', help='If test instances were provided, then this argument is the directory where the results will be placed.')
    args, unknown_args = parser.parse_known_args()

    # Parse the model arguments
    time_limit = float(str(args.limit))
    threads = int(float(str(args.threads)))
    model_list = [('_single', gurobi_single_flow), ('_multi', gurobi_multi_flow)]
    if args.skip_multi: model_list = [model_list[0]]

    # If we do have a test file and a test item, we perform only local small-scale tests -------------------------------
    test_cases = []
    if args.instance_test is not None:
        test_cases = set([int(row) for row in args.instance_test])
    if len(test_cases) > 0:
        output_path = os.path.abspath(args.output)
        assert os.path.exists(args.instances_file), 'TEST INSTANCES FILE DO NOT EXISTS! '+args.instances_file
        assert os.path.exists(output_path), 'OUTPUT DIRECTORY DO NOT EXISTS! '+output_path

        # Reads the instances
        instances_list_raw = []
        with open(args.instances_file, 'r') as fin:
            for i, row in enumerate(fin):
                if i in test_cases:
                    instances_list_raw.append(row)
        print(f'GOT {len(instances_list_raw)} DIFFERENT INSTANCES TO PROCESS')

        # Process the instances locally
        for instance_raw in tqdm(instances_list_raw):
            instance = parse_instance_row(instance_raw)
            for MODEL_TYPE, factory in model_list:
                model_key = 'results'+MODEL_TYPE+'_flow'
                results_file = os.path.join(output_path, f'{instance["instance_key"]}_{model_key}.json')

                # If there is a previously-processed instance, quick-parse its details
                if os.path.exists(results_file):
                    details = {}
                    with open(results_file, 'r') as f:
                        for obj in ijson.items(f, 'json_solution'):
                            details = json.loads(obj).get('SolutionInfo')
                else:
                    results = process_instance(time_limit, threads, MODEL_TYPE, instance, factory)
                    with open(results_file, 'w') as fout:
                        json.dump(results, fout)
                    details = json.loads(results.get('json_solution', '{}')).get('SolutionInfo')

                # Nerd-out the results
                model_name = {'_multi': 'MULTI-FLOW', '_single': 'SINGLE-FLOW'}[MODEL_TYPE]
                for key in sorted(details.keys()):
                    print(f'\t0u0 ({instance["instance_key"]} {instance["seed"]} {model_name}) {key}: {details[key]}')

        print('DONE WITH ALL LOCAL TEST INSTANCES!')
        exit(0)

    # If we are in a production environment ----------------------------------------------------------------------------
    # Get a connection to the DynamoDB Stateful Broker
    if args.local_dynamodb:
        dynamo = boto3.resource('dynamodb', endpoint_url=LOCAL_URI)
        print('USING LOCAL DYNAMODB')
    else:
        dynamo = boto3.resource('dynamodb')
        print('CONNECTED TO CLOUD DYNAMODB')

    # Get the table and lock client - ASSUME THE LOCK TABLE EXISTS
    table = dynamo.Table(INSTANCE_TABLE)
    lock_client = DynamoDBLockClient(dynamo, table_name=LOCK_TABLE)

    # List all instances in DynamoDB
    # List all queued instances in DynamoDB
    scan_kwargs = {'FilterExpression': Attr('queued').eq(True), 'ProjectionExpression': "instance_key"}
    instances_queue = []
    done = False
    start_key = None
    while not done:
        if start_key:
            scan_kwargs['ExclusiveStartKey'] = start_key
        response = table.scan(**scan_kwargs)
        instances_queue += [i['instance_key'] for i in response['Items']]
        start_key = response.get('LastEvaluatedKey', None)
        done = start_key is None
        print(f'QUEUE SIZE: {len(instances_queue)} (MAY HAVE DUPLICATES)')
    instances_queue = sorted(set(instances_queue))
    print(f'GOT {len(instances_queue)} DIFFERENT INSTANCES TO PROCESS')

    # Start processing loop --------------------------------------------------------------------------------------------
    print('ALL SET UP!')

    # While there are instances to acquire
    lock, instance, instances_queue = acquire_instance(table, lock_client, instances_queue)
    while lock is not None:

        # Since we have two variations of the gurobi optimizer formulation, we test with both
        for MODEL_TYPE, factory in model_list:
            model_key = 'results'+MODEL_TYPE+'_flow'
            if model_key in instance:
                print(f'(DONE) SKIPPING MODEL {MODEL_TYPE} ON INSTANCE {instance["instance_key"]}')
                continue

            # Process the local results
            results = process_instance(time_limit, threads, MODEL_TYPE, instance, factory)

            # Get the larger variables and store in a local file
            large_values = {key: results.pop(key) for key in ['gurobi_logs', 'variables', 'json_solution', 'serial']}
            large_values.update(results)
            local_file = f'/tmp/dynamodb_objects/{instance["instance_key"]}_{model_key}.json'
            with open(local_file, 'w') as fout:
                json.dump(large_values, fout)

            # Send the value to S3
            try:
                s3_target = "dynamodb_objects/"+(local_file.split('/')[-1])
                s3_client = boto3.client('s3')
                response = s3_client.upload_file(local_file, args.bucket, s3_target)
                results['results'] = f"s3://{args.bucket}/{s3_target}"
            except Exception as exp:
                if 'the aws access key id you provided does not exist in our records' in str(exp).lower(): pass
                else: raise exp

            # Update the dynamodb register, unqueueing it
            results['results'] = results.get('results', local_file)
            expression, values = get_update_params({model_key: results})
            table.update_item(
                Key={'instance_key': instance['instance_key']},
                UpdateExpression=expression,
                ExpressionAttributeValues=values
            )

        # De-queue the instance key
        expression, values = get_update_params({'queued': False}, stringfy=True)
        table.update_item(
            Key={'instance_key': instance['instance_key']},
            UpdateExpression=expression,
            ExpressionAttributeValues=values
        )

        # Release the lock and get another instance
        lock.release()
        lock, instance, instances_queue = acquire_instance(table, lock_client, instances_queue)
        print(f'QUEUE SIZE: {len(instances_queue)}')
    print('DONE WITH ALL INSTANCES!')
