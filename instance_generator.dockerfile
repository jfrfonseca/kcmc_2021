# USING DEBIAN 11 "Bull's Eye" OS
FROM python:bullseye

# Install python dependencies
RUN pip install --no-cache-dir asyncio aioredis

# Add in the executables
COPY builds/instance_generator /app/instance_generator

# Work directory configurations
WORKDIR '/app'
