#!/bin/sh

# Generate instances
if [ ! -f /data/instances.csv ]
then
  parallel -a /app/heuristics/instance_classes.csv --colsep ' ' --files /app/builds/instances_generator
  cat /tmp/*.par > /data/instances.csv
  chmod a+rw /data/instances.csv
fi

# Test isomorphism
cat /data/instances.csv | python -u /app/integer_linear_programs/kcmc_instance.py
