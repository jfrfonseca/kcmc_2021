#!/bin/sh

# Generate instances
if [ ! -f /data/instances.csv ]
then

  # Process the data parallely
  parallel -a /app/heuristics/instance_classes.csv --colsep ' ' --files /app/instances_generator
  # In case of frequent isomorphism, try to run twice to get spare instances
  #sleep 2
  #parallel -a /app/heuristics/instance_classes.csv --colsep ' ' --files /app/instances_generator

  # Collect the instances and give appropriate permissions
  cat /tmp/*.par > /data/instances.csv
  chmod a+rw /data/instances.csv
fi

# Test isomorphism. Hopefully we will have enough instances anyway
cat /data/instances.csv | python -u /app/integer_linear_programs/kcmc_instance.py
