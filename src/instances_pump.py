"""
Gurobi Optimizer Driver Script
"""


# STDLib
import argparse

# PIP
import boto3

# Auxiliary packages
from dynamodb_lock import DynamoDBLockClient

# Payload packages
from gurobi_optimizer import INSTANCE_TABLE, LOCK_TABLE, LOCAL_URI


# ######################################################################################################################
# RUNTIME


if __name__ == '__main__':

    print('SETTING UP...')

    # Set the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--instances_file', type=str, help='Instances file')
    parser.add_argument('--release_locks', action='store_true', help='If all locks should be released')
    args, unknown_args = parser.parse_known_args()

    # Read the instances
    with open(args.instances_file, 'r') as fin:
        instances = fin.readlines()

    data = []
    for row in instances:
        serial, kcmc = map(lambda i: i.strip().upper(), row.split('|'))
        k = int(kcmc.split('(K')[-1].split('M')[0])
        m = int(kcmc.split('M')[-1].split(')')[0])
        _, psk, acc, seed, _ = serial.split(';', 4)
        pois, sensors, sinks = map(int, psk.split(' '))
        area, coverage, communication = map(int, psk.split(' '))
        data.append({
            'instance_key': '_'.join(list(map(str, ['KCMC',
                                                    pois, sensors, sinks,
                                                    area, coverage, communication,
                                                    seed]))),
            'K': k, 'M': m, 'seed': seed,
            'pois': pois, 'sensors': sensors, 'sinks': sinks,
            'area': area, 'coverage': coverage, 'communication': communication,
            'serial': serial.strip(),
            'queued': True
        })

    # Put the instances into the tables
    for namespace, dynamo, dynamo_cli in [
        ('LOCAL', boto3.resource('dynamodb', endpoint_url=LOCAL_URI), boto3.client('dynamodb', endpoint_url=LOCAL_URI)),
        ('CLOUD', boto3.resource('dynamodb'), boto3.client('dynamodb'))
    ]:

        # Assert the table
        try:
            dynamo.create_table(
                TableName=INSTANCE_TABLE,
                KeySchema=[{'AttributeName': 'instance_key', 'KeyType': 'HASH'}],
                AttributeDefinitions=[{'AttributeName': 'instance_key', 'AttributeType': 'S'}],
                ProvisionedThroughput={'ReadCapacityUnits': DynamoDBLockClient._DEFAULT_READ_CAPACITY,
                                       'WriteCapacityUnits': DynamoDBLockClient._DEFAULT_WRITE_CAPACITY},
            )
            print(f'CRATED TABLE {INSTANCE_TABLE} IN DYNAMODB {namespace}')
        except Exception as exp:
            if 'preexisting table' in str(exp).lower(): pass
            elif 'the security token included in the request is invalid' in str(exp).lower(): continue
            else: raise exp

        # Insert the data
        print(f'INSERTING {len(data)} ITEMS INTO DYNAMODB {namespace}')
        table = dynamo.Table(INSTANCE_TABLE)
        qtd_inserted = 0
        for item in data:
            try:
                table.put_item(Item=item, ConditionExpression='attribute_not_exists(instance_key)')
                qtd_inserted += 1
            except Exception as exp:
                if 'conditional request failed' in str(exp).lower(): pass
                else: raise exp
        print(f'DONE INSERTING {qtd_inserted} ITEMS IN DYNAMODB {namespace}')

        # Clear the lock table, if exists
        if args.release_locks:
            try:
                dynamo.Table(LOCK_TABLE).delete()
                print(f'ALL LOCKS RELEASED FROM TABLE {LOCK_TABLE} IN DYNAMODB '+namespace)
            except Exception as exp:
                raise exp

        # Assert the existence of the locking table
        lock_client = DynamoDBLockClient(dynamo, table_name=LOCK_TABLE)
        try:
            lock_client.create_dynamodb_table(dynamo_cli, table_name=LOCK_TABLE)
            print('CREATED LOCK TABLE IN DYNAMODB '+namespace)
        except Exception as exp:
            if 'cannot create preexisting table' in str(exp).lower(): pass
            else: raise exp

    exit(0)
