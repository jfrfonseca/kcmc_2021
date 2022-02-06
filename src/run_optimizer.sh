#!/bin/sh

# PARAMETERS:
# 1) Timeout (in seconds)
# 2) Instance Number (line num in file, starting in 1)
# 3) Weight of Coverage violation penalties
# 4) Weight of Connectivity violation penalties
# 5) Population Size
# 6) Selection Group Size
# 7) Mutation Rate
# 8) Bias for Ones when creating individuals
# 9) Verbosity (interval between printouts)
# 10) Instances File

# PARSE PARAMETERS
instance_number=${2:-1}
instances_file=${10:-../data/instances.csv}
verbosity=${9:-10}
timeout=${1:-60}

weight_k=${3:-1.0}
weight_m=${4:-1.0}

population=${5:-50}
selection=${6:-10}
mutation=${7:-0.33}
one_bias=${8:-0.75}

# Get the serialized instance and its approved K and M
line=`sed -n "${instance_number}p" ${instances_file}`
serialized_instance=`python -c 'import sys; print(sys.argv[1].split("|")[0].strip())' "${line}"`
kcmc_k=`python -c 'import sys; print(int(sys.argv[1].split("|")[1].strip().split("(K")[-1].split("M")[0]))' "${line}"`
kcmc_m=`python -c 'import sys; print(int(sys.argv[1].split("|")[1].strip().split("M")[-1].split(")")[0]))' "${line}"`

# Prepare the command
timeout ${timeout} ../builds/optimizer_genalg_binary \
    ${verbosity} ${population} ${selection} ${mutation} ${one_bias} \
    ${kcmc_k} ${kcmc_m} ${weight_k} ${weight_m} "${serialized_instance}"
