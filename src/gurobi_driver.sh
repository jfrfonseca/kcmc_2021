
# Echo the KEY of the instance we are working with
echo "${1}" | cut -c1-50

# Run the instance in the GUROBI Optimizer
exec python -u /home/gurobi/src/gurobi_optimizer.py \
                 -t 60 -p 1 \
                 -k 5 -m 3 \
                 -i "${1}"
