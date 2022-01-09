/*
 * Genetic algorithm operators
 * Set of Genetic Algorithm operators that can be used in many configurations of the algorithm
 * Many are used in our implementation of Gupta's Genetic Algorithm for the KCMC Problem
 */

#include <cstdlib>    // rand
#include <numeric>    // accumulate
#include <algorithm>  // copy, fill

// Dependencies from this package
#include "kcmc_instance.h"

#ifndef GENETIC_ALGORITHM_OPERATORS_H
#define GENETIC_ALGORITHM_OPERATORS_H

int get_best_individual(int interval, std::unordered_set<int> *unused_sensors, int chromo_size, int pop_size,
                        int **population, double *fitness, int num_generation, int previous_best);

int individual_creation(float one_bias, int size, int chromo[]);
bool inspect_individual(int size, int *individual);
bool inspect_population(int pop_size, int size, int **population);

int selection_roulette(int sel_size, std::vector<int> *selection, int pop_size, double *fitness);
int selection_get_one(int sel_size, std::vector<int> selection, int avoid);

int crossover_single_point(int size, int *chromo_a, int *chromo_b, int output[]);

int mutation_random_bit_flip(int size, int chromo[]);

double fitness_gupta(KCMC_Instance *wsn, int K, int M, double w1, double w2, double w3, int *chromo);

#endif
