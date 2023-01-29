# Adding GUROBI paraphernalia
FROM gurobi/modeling-examples:9.5.2

# Install other analisys dependencies and cleanup
RUN apt update  -y \
 && apt install -y time gcc g++ libcairo2-dev pkg-config python3-dev \
 && apt -y autoremove && rm -rf /var/lib/apt/lists/*

# Install the REQUIREMENTS available throughout the PYTHON PACKAGE INDEX (PIP)
RUN pip install redis ijson numpy pandas fastparquet pyarrow pycairo plotly python-igraph tqdm ipywidgets

