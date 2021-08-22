# USING DEBIAN 10 "Buster" OS
FROM gcc:10.3-buster

# INSTALL DEPS & CLEANUP
RUN apt -y update \
 && apt install -y cmake \
 && apt -y autoremove && rm -rf /var/lib/apt/lists/*

# USER CODE MUST BE PLACED IN /app
WORKDIR /app
