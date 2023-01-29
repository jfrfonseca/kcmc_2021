
# Stop at first error
set -e

# YOU MIGHT WANT TO INSTALL jq WITH `sudo apt-get install -y jq`. It formats bash strings as JSON

# Get the constants
source .env
COUNT=1


# Prepare the payload for the OPTIMIZER GUROBI
if [ $1 = "gurobi" ]; then
echo "Using GUROBI"
OVERRIDES=$(jq --compact-output << EOF
{
  "containerOverrides": [{
    "name": "worker",
    "command": [
      "/app/run_gurobi.sh", "KCMC;100_500_1;300_50_100;157982033;END", "3", "2",
      "multi-flow"
    ],
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
  --overrides ${OVERRIDES}
