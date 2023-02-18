# Adding GUROBI paraphernalia
FROM gurobi/modeling-examples:9.5.2

# Install other analisys dependencies and cleanup
RUN apt update  -y \
 && apt install -y g++ make cmake --no-install-recommends \
 && apt -y autoremove && rm -rf /var/lib/apt/lists/*


# Install the REQUIREMENTS available throughout the PYTHON PACKAGE INDEX (PIP)
RUN pip install \
          numpy pandas pyarrow \
          plotly tqdm ipywidgets \
          boto3
 
 # Acquire & compile the heuristics
COPY heuristics /app/heuristics
COPY CMakeLists.txt /app
RUN mkdir /tmp/builds \
 && cd /tmp/builds \
 && cmake -S /app -B . \
 && cmake --build . \
 && chmod +x /tmp/builds/* \
 && mv instances_generator /app \
 && mv instance_regenerator /app \
 && mv optimizer /app \
 && rm -rf /tmp/builds

