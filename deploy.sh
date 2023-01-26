#!/bin/bash

# I hope you have configured the appropriate AWS key using `aws configure`...

# Stop at first error
set -e

# Get constants
source .env

# Deploy cloud env FOR THE FIRST TIME
# aws cloudformation create-stack --stack-name kcmc --template-body file://environment.cfm --capabilities CAPABILITY_NAMED_IAM

# Deploy/update cloud env
# aws cloudformation update-stack --stack-name kcmc --template-body file://environment.cfm --capabilities CAPABILITY_NAMED_IAM

# Login to ECR
aws ecr get-login-password | docker login --username AWS --password-stdin "${DOCKER_REPOSITORY}"

# Compile and upload the containers
docker-compose build heuristic_optimizer
docker-compose push  heuristic_optimizer
