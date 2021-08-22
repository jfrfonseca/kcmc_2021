cmake_minimum_required(VERSION 3.13)
project(kcmc_heuristic)

set(CMAKE_CXX_STANDARD 14)

# Utilities and KCMC Instance Object ------------------------------------------
ADD_LIBRARY(KCMC_Module
    src/kcmc_components.cpp
    src/kcmc_components.h
)
target_link_libraries(KCMC_Module)


# Instance generator ----------------------------------------------------------
ADD_EXECUTABLE(instance_generator src/instance_generator.cpp)
target_link_libraries(instance_generator KCMC_Module)

## Optimizer -------------------------------------------------------------------
#ADD_EXECUTABLE(kcmc_2021 src/main.cpp)
#target_link_libraries(kcmc_2021 KCMC_Module)
