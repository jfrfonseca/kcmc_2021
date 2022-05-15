# BUILD IN TWO STEPS
mkdir /tmp/build
cd /tmp/build
cmake -S /app -B .
cmake --build .

# COPY THE OUTPUT EXECUTABLES TO THE BUILDS DIRECTORY
cp instance_generator /app/builds
cp instance_evaluator /app/builds
cp placements_visualizer /app/builds
cp optimizer_* /app/builds