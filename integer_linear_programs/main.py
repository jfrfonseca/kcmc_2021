
# STDLIB
import os
import argparse

# This package
from experiment import run_experiment


if __name__ == '__main__':

    # Parse the arguments
    parser = argparse.ArgumentParser(prog='KCMC Solver', description='Solver for the KCMC problem')
    parser.add_argument('-i', '--instance')
    parser.add_argument('-k', '--kcmc_k', type=int)
    parser.add_argument('-m', '--kcmc_m', type=int)
    parser.add_argument('-h', '--heuristic')
    parser.add_argument('-g', '--gurobi_model')
    parser.add_argument('-t', '--time_limit', type=int, default=3600)
    parser.add_argument('-p', '--processes', type=int, default=1)
    parser.add_argument('-y', '--y_binary', action='store_true')
    parser.add_argument('-l', '--local', action='store_true')
    args = parser.parse_args()

    # Get the arguments
    instance_key=args.instance
    kcmc_k = args.kcmc_k
    kcmc_m = args.kcmc_m
    heuristic = args.heuristic
    gurobi_model = args.gurobi_model
    time_limit = args.time_limit
    processes = args.processes
    y_binary = args.y_binary
    local = args.local

    # Parse some values
    _, pois, _ = instance_key.split(';', 2)
    pois, sensors, _ = map(int, pois.split(' '))
    random_seed = instance_key.rsplit(';', 1)[-1]

    # TODO Check if we already have the file. If so, stop
    filename = f'/data/{pois}.{sensors}.{random_seed}.{heuristic}.{gurobi_model}.json'

    # Prepare the GUROBI file
    with open('/opt/gurobi/gurobi.lic', 'w') as fout:
        fout.write(f'WLSACCESSID={os.environ["WLSACCESSID"]}\n')
        fout.write(f'WLSSECRET={os.environ["WLSSECRET"]}\n')
        fout.write(f'LICENSEID={os.environ["LICENSEID"]}')

    # Run the experiment
    result = run_experiment(instance_key, kcmc_k, kcmc_m, heuristic, gurobi_model,
                            time_limit=time_limit, threads=processes, y_binary=y_binary)

    # Persist the result
    with open(filename, 'w') as fout:
        fout.write(result.to_json())
