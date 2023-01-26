
# Stop at first error
set -e

# YOU MIGHT WANT TO INSTALL jq WITH `sudo apt-get install -y jq`. It formats bash strings as JSON

# Get the constants
source .env

# Prepare the payload
COUNT=1
OVERRIDES=$(jq --compact-output << EOF
{
  "containerOverrides": [{
    "name": "worker",
    "command": ["/app/heuristics/run_heuristics.sh"],
    "cpu": 4096,
    "memory": 8192,
    "environment": [
      {"name": "RESULTS_TARGET",
       "value": "s3://kcmc-heuristic/2023/heuristic"}
    ]
  }]
}
EOF
)

# Prepare the Network object
# Que pendejo ese Bezos! You should not need a public IP for FARGATE-ECR sync https://stackoverflow.com/a/66802973
NETWORK_CONF=$(jq --compact-output << EOF2
{
  "awsvpcConfiguration": {
    "subnets": ["${SUBNET_ID}"],
    "securityGroups": ["${SECURITY_GROUP_ID}"],
    "assignPublicIp": "ENABLED"
  }
}
EOF2
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
