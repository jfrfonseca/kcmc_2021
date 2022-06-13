#!/bin/sh

# Prepare the SWAP space (24Gb)
sudo dd if=/dev/zero of=/swapfile bs=1024M count=24
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# Install GIT, DOCKER and DOCKER-COMPOSE
sudo yum install -y git docker htop
sudo pip3 install docker-compose

# Prepare the docker runtime
sudo usermod -a -G docker ec2-user
sudo systemctl enable docker.service
sudo systemctl start docker.service

# Clone the repo
git clone https://github.com/jfrfonseca/kcmc_heuristic.git
cd kcmc_heuristic

# Compile the C++ code
docker-compose --profile compiler build
docker-compose --profile compiler up

# Prepare the optimizer
docker-compose --profile optimizer build

# Untar the instances
cd data
tar -xzvf instances.tar.gz
cd ../

# Now you must prepare your gurobi licences file and your .env environment file by hand
