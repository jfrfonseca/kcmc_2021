#!/bin/sh

# Generate instances
if [ ! -f /data/instances.csv ]
then

  # Process the data parallely
  echo "GENERATING INSTANCES"
  parallel -a /app/heuristics/instance_classes.csv --colsep ' ' --files /app/instances_generator
  # In case of frequent isomorphism, try to run twice to get spare instances
  #sleep 2
  #parallel -a /app/heuristics/instance_classes.csv --colsep ' ' --files /app/instances_generator

  # Collect the instances and give appropriate permissions
  cat /tmp/*.par > /data/instances.csv
  chmod a+rw /data/instances.csv

  # Test isomorphism. Hopefully we will have enough instances anyway
  echo "TESTING ISOMORPHISM IN GENERATED INSTANCES"
  cat /data/instances.csv | python -u /app/integer_linear_programs/kcmc_instance.py
fi

# Optimize instances parallely using all heuristics, then collect the results
echo "OPTIMIZING INSTANCES HEURISTICALLY"
parallel -a /data/instances.csv --colsep '\t' --files /app/optimizer
cat /tmp/*.par > /tmp/heuristically_optimized_instances.csv
chmod a+rw /tmp/heuristically_optimized_instances.csv

# If we have an S3 TARGET, copy the optimized instances to it. If not, copy to DATA
if [ -n "${RESULTS_TARGET}" ]
then
  echo "EXPORTING TO S3"
  aws s3 cp /tmp/heuristically_optimized_instances.csv "${RESULTS_TARGET}/heuristically_optimized_instances.csv"
else
  echo "LOCAL RUN!"
  cp /tmp/heuristically_optimized_instances.csv "/data/heuristically_optimized_instances.csv"
fi
