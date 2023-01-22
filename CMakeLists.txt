cmake_minimum_required(VERSION 3.13)
project(kcmc_heuristic)

set(CMAKE_CXX_STANDARD 14)

# Utilities and KCMC Instance Object ------------------------------------------
ADD_LIBRARY(KCMC_Module
            heuristics/kcmc_instance.cpp
            heuristics/k_coverage.cpp
            heuristics/m_connectivity.cpp
            heuristics/optimizer.cpp
            heuristics/kcmc_instance.h
)
target_link_libraries(KCMC_Module)

# Instance generator ----------------------------------------------------------
ADD_EXECUTABLE(instances_generator heuristics/instances_generator.cpp)
target_link_libraries(instances_generator KCMC_Module)

ADD_EXECUTABLE(instance_regenerator heuristics/instance_regenerator.cpp)
target_link_libraries(instance_regenerator KCMC_Module)

# Optimizer -------------------------------------------------------------------
ADD_EXECUTABLE(optimizer heuristics/optimizer_runtime.cpp)
target_link_libraries(optimizer KCMC_Module)
