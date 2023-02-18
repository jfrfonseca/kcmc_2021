
# Stop at first error
set -e

# YOU MIGHT WANT TO INSTALL jq WITH `sudo apt-get install -y jq`. It formats bash strings as JSON

# Get the constants
source .env
COUNT=1

# Prepare the payload for the OPTIMIZER GUROBI
if [ ${1} = "gurobi" ]; then
# echo "Using GUROBI"

TASK_DEFINITION=${2}

# Prepare the command. The hard part is to make it split right
PYARGS=${@}
COMMAND=$(python << EOF0
import shlex, json, sys
args = "${PYARGS}"
pref, suf = args.split('KCMC;')
key, suf = suf.split(';END')
key = 'KCMC;'+key.replace(' ', '_')+';END'
args = pref+key+suf
args = shlex.split(args)[2:]
print(json.dumps(["/app/run_gurobi.sh"]+args))
EOF0
)

# Prepare the container overrides
OVERRIDES=$(jq --compact-output << EOF
{
  "containerOverrides": [{
    "name": "worker",
    "command": ${COMMAND},
    "cpu": 1024,
    "memory": 8192,
    "environment": [
      {"name": "RESULTS_TARGET",
       "value": "s3://kcmc-heuristic/2023/gurobi"}
    ]
  }]
}
EOF
)

else

OVERRIDES=$(jq --compact-output << EOF2
{
  "cpu": "4096",
  "containerOverrides": [{
    "name": "worker",
    "command": ["/app/run_heuristics.sh"],
    "cpu": 4096,
    "memory": 8192,
    "environment": [
      {"name": "RESULTS_TARGET",
       "value": "s3://kcmc-heuristic/2023/heuristic"}
    ]
  }]
}
EOF2
)
fi

# Prepare the Network object
# Que pendejo ese Bezos! You should not need a public IP for FARGATE-ECR sync https://stackoverflow.com/a/66802973
NETWORK_CONF=$(jq --compact-output << EOF3
{
  "awsvpcConfiguration": {
    "subnets": ["${SUBNET_ID}"],
    "securityGroups": ["${SECURITY_GROUP_ID}"],
    "assignPublicIp": "ENABLED"
  }
}
EOF3
)


# Run the command
aws ecs run-task \
  --cluster ${CLUSTER_NAME} \
  --task-definition ${TASK_DEFINITION}  \
  --count ${COUNT} \
  --network-configuration ${NETWORK_CONF} \
  --launch-type FARGATE \
  --region ${AWS_REGION} \
  --overrides ${OVERRIDES} \
  --query "tasks[0].taskArn"
