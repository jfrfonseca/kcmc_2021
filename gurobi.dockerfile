# USING GUROBI IMAGE
FROM gurobi/python:9.1.2

# Install GNU PARALLEL & CLEANUP
RUN apt-get update \
 && apt-get install -y parallel \
 && apt-get -y autoremove && rm -rf /var/lib/apt/lists/*

# Install python dependencies
RUN pip install --no-cache-dir boto3 simplejson ijson python_dynamodb_lock
