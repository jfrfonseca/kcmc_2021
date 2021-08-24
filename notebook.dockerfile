# Adding GUROBI paraphernalia
FROM gurobi/modeling-examples:9.1.2

# Install other analisys dependencies and cleanup
RUN apt update  -y \
 && apt install -y gcc g++ libcairo2-dev pkg-config python3-dev \
 && apt -y autoremove && rm -rf /var/lib/apt/lists/*

# Install the REQUIREMENTS available throught the PYTHON PACKAGE INDEX (PIP)
RUN pip install redis numpy pandas pyarrow pycairo python-igraph tqdm ipywidgets

