# USING GUROBI IMAGE
FROM gurobi/python:9.1.2

# Install python dependencies
RUN pip install --no-cache-dir boto3 simplejson ijson

# Prepare the runpoint
ENTRYPOINT ["python", "-u", "/home/gurobi/src/gurobi_optimizer.py"]
CMD ["--help"]