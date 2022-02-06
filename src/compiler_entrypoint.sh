# BUILD IN TWO STEPS
mkdir /tmp/build
cd /tmp/build
cmake -S /app -B .
cmake --build .

# COPY THE OUTPUT EXECUTABLES TO THE BUILDS DIRECTORY
cp instance_generator /app/builds
cp instance_evaluator /app/builds
# cp optimizer_gupta_exact /app/builds
# cp optimizer_gupta_adapted /app/builds
cp optimizer_genalg_binary /app/builds