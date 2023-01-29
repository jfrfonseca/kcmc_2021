#!/bin/sh

set -e

python /app/integer_linear_programs/gurobi_experiment.py "${@}"

# If we have an S3 TARGET, copy the optimized instances to it. If not, copy to DATA
if [ -n "${RESULTS_TARGET}" ]
then
  echo "EXPORTING TO S3"
  aws s3 cp /tmp/kcmc_gurobi_experiment_*.json "${RESULTS_TARGET}"
else
  echo "LOCAL RUN!"
  cp /tmp/kcmc_gurobi_experiment_*.json "/data"
fi
