cmake_minimum_required(VERSION 3.13)
project(kcmc_heuristic)

set(CMAKE_CXX_STANDARD 14)

# Utilities and KCMC Instance Object ------------------------------------------
ADD_LIBRARY(KCMC_Module
            src/kcmc_instance.cpp
            src/m_connectivity.cpp
            src/kcmc_instance.h
            src/genetic_algorithm_operators.cpp
            src/genetic_algorithm_operators.h
)
target_link_libraries(KCMC_Module)


# Instance generator ----------------------------------------------------------
ADD_EXECUTABLE(instance_generator src/instance_generator.cpp)
target_link_libraries(instance_generator KCMC_Module)


# Instance evaluator ----------------------------------------------------------
ADD_EXECUTABLE(instance_evaluator src/instance_evaluator.cpp)
target_link_libraries(instance_evaluator KCMC_Module)


## Optimizer -------------------------------------------------------------------
ADD_EXECUTABLE(optimizer_gupta_exact src/optimizer_gupta_exact.cpp)
target_link_libraries(optimizer_gupta_exact KCMC_Module)
