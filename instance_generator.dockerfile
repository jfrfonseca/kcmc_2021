# USING DEBIAN 11 "Bull's Eye" OS
FROM python:bullseye

# Install GNU PARALLEL & CLEANUP
RUN apt -y update \
 && apt install -y parallel \
 && apt -y autoremove && rm -rf /var/lib/apt/lists/*

# Install python dependencies
RUN pip install --no-cache-dir asyncio aioredis numpy pandas pyarrow redis

# Add in the executables
COPY builds/instance_generator /app/instance_generator

# Work directory configurations
WORKDIR '/app'
