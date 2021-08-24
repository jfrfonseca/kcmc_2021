
import sys
import csv
import time
import traceback
import asyncio
import aioredis

LOCK_TIME = 10000


async def generate_instances(
        random_seeds:list, k_range: int, m_range: int,
        num_pois:int, num_sensors:int, num_sinks:int, area_side:int, covg_radius:int, comm_radius:int,
        target_instances: int
) -> (int, int):
    redis = await aioredis.from_url("redis://host.docker.internal")  # Get a connection with REDIS

    # Check how many instances we already have
    instance_key = f'INSTANCE:{num_pois}:{num_sensors}:{num_sinks}:{area_side}:{covg_radius}:{comm_radius}'
    evaluation_key = f'EVALUATION:{num_pois}:{num_sensors}:{num_sinks}:{area_side}:{covg_radius}:{comm_radius}'
    existing_instances = int(await redis.hlen(instance_key))
    if existing_instances >= target_instances: return existing_instances, 0

    # If we got here, ~someone~ should create those missing instances. Try to acquire the lock
    async with redis.pipeline(transaction=True) as pipe:
        has_lock, _ = await (pipe
            .setnx('LOCK:'+instance_key, str(time.time_ns()))
            .expire('LOCK:'+instance_key, LOCK_TIME)
            .execute()
        )

    if not has_lock:
        time.sleep(0.05)  # Prevent REDIS from overheating
        return existing_instances, -1
    print('GOT LOCK ::', instance_key)

    # Run the command in a subprocess
    proc = await asyncio.create_subprocess_exec(
        '/app/instance_generator',
        *list(map(str, [k_range, m_range, num_pois, num_sensors, num_sinks, area_side, covg_radius, comm_radius]
                       + random_seeds[existing_instances:target_instances])),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE
    )

    # Read the expected number of lines, each being piped to REDIS
    for num, expected_seed in enumerate(random_seeds[existing_instances:target_instances]):

        # Instance, serialized
        out_instance = await proc.stdout.readline()
        out_instance = out_instance.decode('ascii').strip()

        # Instance evaluation
        out_evaluation = await proc.stdout.readline()
        out_evaluation = out_evaluation.decode('ascii').strip()

        # Set the data on REDIS
        async with redis.pipeline(transaction=True) as pipe:
            await (pipe
               .hset(instance_key,   str(expected_seed), out_instance)
               .hset(evaluation_key, str(expected_seed), out_evaluation)
               .execute()
            )

    # Wait for the subprocess exit.
    await proc.wait()

    # Clear the lock
    await redis.delete('LOCK:'+instance_key)

    # Return the amount of wrok performed
    result = (int(await redis.hlen(instance_key)), target_instances-existing_instances)
    await redis.close()
    return result


if __name__ == '__main__':
    try:
        time.sleep(30)

        # parse the arguments
        configs_file = sys.argv[1]
        target_instances = int(sys.argv[2])
        k_range = int(sys.argv[3])
        m_range = int(sys.argv[4])

        # Extract the complete random seeds
        with open('/data/random_seeds.txt', 'r') as fin:
            random_seeds = fin.read()
        random_seeds = list(set([int(i) for i in random_seeds.replace("\t", " ").replace("\n", " ").split(" ") if len(i) > 0]))

        # Extract the configurations
        configs_to_run = []
        with open(configs_file, 'r') as fin:
            csv_fin = csv.reader(fin, delimiter=',')
            for i,line in enumerate(csv_fin):
                if i == 0: continue
                configs_to_run.append(tuple(list(map(int, map(lambda i: i.strip(), line)))))

        # For each configuration, generate instances
        for num_pois, num_sensors, num_sinks, area_side, covg_radius, comm_radius in configs_to_run:
            try:
                previous_work, current_work = asyncio.run(
                    generate_instances(
                        random_seeds, k_range, m_range,
                        num_pois, num_sensors, num_sinks, area_side, covg_radius, comm_radius,
                        target_instances
                    )
                )
                print(f'>>>> KCMC:{num_pois}:{num_sensors}:{num_sinks}:{area_side}:{covg_radius}:{comm_radius}', previous_work, current_work)
            except Exception as exp:
                if {'connection', 'reset', 'peer'}.issubset(str(exp).lower().strip()):
                    time.sleep(10)
                else: raise exp

    except Exception as exp:
        traceback.print_exc()
