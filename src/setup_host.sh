#!/bin/sh

# Install GIT, DOCKER and DOCKER-COMPOSE
sudo yum install -y git docker
sudo pip3 install docker-compose

# Clone the repo
git clone https://github.com/jfrfonseca/kcmc_heuristic.git
