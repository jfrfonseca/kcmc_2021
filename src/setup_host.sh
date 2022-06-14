#!/bin/sh

# Prepare the SWAP space (24Gb)
sudo dd if=/dev/zero of=/swapfile bs=512M count=48
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile swap swap defaults 0 0' | sudo tee -a /etc/fstab

# Install GIT, DOCKER and DOCKER-COMPOSE
sudo yum install -y git docker htop
sudo pip3 install docker-compose

# Clone the repo
git clone https://github.com/jfrfonseca/kcmc_heuristic.git
cd kcmc_heuristic

# Untar the instances
cd data
tar -xzvf instances.tar.gz
cd ../

# Prepare the docker runtime
sudo usermod -a -G docker ec2-user
sudo systemctl enable docker.service
sudo systemctl start docker.service

# You might have to LOG-OUT and then LOG IN again after running the code above and before running the code below

# Compile the C++ code
docker-compose --profile compiler build
docker-compose --profile compiler up

# Prepare the optimizer
docker-compose --profile optimizer build

# Now you must prepare your gurobi licences file and your .env environment file by hand
#
# Example for the .env file:
# rm .env && nano .env
# And then fill the file
#
# Example for the license file
# nano data/licence.lic
# And then fill the file

# And then you can run the optimizer stack, with:
# nohup docker-compose --profile optimizer up &> log.out &
