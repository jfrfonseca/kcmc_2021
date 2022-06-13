#!/bin/sh

# Install GIT, DOCKER and DOCKER-COMPOSE
sudo yum install -y git docker
sudo pip3 install docker-compose

# Clone the repo
git clone https://github.com/jfrfonseca/kcmc_heuristic.git
cd kcmc_heuristic

# Compile the C++ code
sudo docker-compose --profile compiler build
sudo docker-compose --profile compiler up

# Prepare the optimizer
sudo docker-compose --profile optimizer build
sudo docker-compose --profile optimizer up
