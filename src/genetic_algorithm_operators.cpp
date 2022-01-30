/*
 * Genetic algorithm operators
 * Set of Genetic Algorithm operators that can be used in many configurations of the algorithm
 * Many are used in our implementation of Gupta's Genetic Algorithm for the KCMC Problem
 */


#include <chrono>     // time functions
#include <iomanip>    // setfill, setw
#include <iostream>   // cout, endl
#include <sstream>    // ostringstream
#include <cstdlib>    // rand
#include <numeric>    // accumulate
#include <algorithm>  // copy, fill

// Dependencies from this package
#include "kcmc_instance.h"
#include "genetic_algorithm_operators.h"


void exit_signal_handler(int signal) {
   std::cerr << "Interrupt signal (" << signal << ") received. Exiting gracefully..." << std::endl;
   exit(0);
}


void printout(int num_generation, int chromo_size, int *individual, double fitness) {

    // Get the number of USED sensors in the individual
    int num_used = std::accumulate(individual, individual+chromo_size, 0);

    // Prepare the output buffer
    std::ostringstream out;

    // Print header in the first generation
    if (num_generation == 0) {out << "GEN_IT\tTIMESTAMP_MS\tSIZE\tUNUSED\tFITNESS\tCHROMOSSOME" << std::endl;}

    // Print a line with:
    // - The number of the current generation
    // - The current timestamp
    // - The number of used sensors in the individual
    // - The percentage of UNused sensors in the individual
    // - The given fitness value
    // - The individual itself
    out << std::setfill('0') << std::setw(5) << num_generation
        << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        << "\t" << std::setfill('0') << std::setw(5) << num_used
        << "\t" << std::fixed << std::setprecision(3) << ((double)(chromo_size - num_used)) / ((double)chromo_size)
        << "\t" << std::fixed << std::setprecision(3) << fitness
        << "\t";
    for (int i=0; i<chromo_size; i++) {out << individual[i];}
    // Flush
    std::cout << out.str() << std::endl;
}


int get_best_individual(int interval, std::unordered_set<int> *unused_sensors, int chromo_size, int pop_size,
                        int **population, double *fitness, int num_generation, double overall_best) {

    // Get the position of the best fitness in the fitness array
    int i, best = ((int)(std::max_element(fitness, fitness + pop_size) - fitness));

    // Reset and Store the best individual in the unused sensors set - only the unused sensors' positions
    unused_sensors->clear();
    for (i=0; i<chromo_size; i++) {
        if (population[best][i] == 0) {
            unused_sensors->insert(i);
        }
    }

    // If the best individual has higher fitness than the overall best value, or we are at the right moment, printout
    if (((num_generation % interval) == 0) | (fitness[best] > overall_best)) {
        printout(num_generation, chromo_size, population[best], fitness[best]);
    }

    // Return the best fitness POSITION
    return best;
}


/* #####################################################################################################################
 * CHROMOSSOME GENERATION
 * */


int individual_creation(float one_bias, int size, int chromo[]) {
    int num_ones = 0;
    for (int i=0; i<size; i++) {
        chromo[i] = ((double) rand() / (RAND_MAX))< one_bias ? 1 : 0;
        num_ones += chromo[i];
    }
    return num_ones;
}

bool inspect_individual(int size, int *individual) {
    for (int i=0; i<size; i++) {
        if ((individual[i] < 0) | (individual[i] > 1)) {
            return false;
        }
    }
    return true;
}

bool inspect_population(int pop_size, int size, int **population) {
    for (int j=0; j<pop_size; j++) {
        if (not inspect_individual(size, population[j])) {
            return false;
        }
    }
    return true;
}


/* #####################################################################################################################
 * SELECTION
 * */


int selection_roulette(int sel_size, std::vector<int> *selection, int pop_size, double *fitness) {
    // USED BY GUPTA

    // Clear out the selection array
    selection->clear();

    // Prepare an array to store already selected individual positions
    double selected[pop_size];
    std::fill(selected, selected+pop_size, 1.0);

    // Compute the total fitness
    double total_fitness = std::accumulate(fitness, fitness+pop_size, 0.0);

    // Generate a random value between 0 and the total fitness
    double random_value = ((double)rand() / (RAND_MAX)) * total_fitness;

    // While we still have not selected all values
    int pos = -1, iterations = 0;
    while (selection->size() < sel_size) {

        // THIS FUNCTION WILL JAM INTO AN INFINITE LOOP IF THE SUM OF ALL FITNESS IS <= 0.0!
        // Thus we make a test first
        if (total_fitness <= 0.0) {throw std::runtime_error("THE SUM OF FITNESS MUST BE A POSITIVE VALUE!");}

        // While the random value has not been fully drained
        while (random_value > 0.0) {
            pos = (pos + 1) % pop_size;  // Advance one position
            random_value -= (fitness[pos] * selected[pos]);  // Drain the fitness of the current position, if unselected
            iterations++;
        }

        // Select the position
        selected[pos] = 0.0;
        selection->push_back(pos);

        // Decrease the total fitness value by the fitness of the selected value
        total_fitness -= fitness[pos];

        // Reset the position and the random value
        pos = -1;
        random_value = ((double) rand() / (RAND_MAX)) * total_fitness;
    }

    // Return the number of iterations
    return iterations;
}

int selection_get_one(int sel_size, std::vector<int> selection, int avoid) {
    int pos = rand() % sel_size;
    while (selection[pos] == avoid) {
        pos = rand() % sel_size;
    }
    return selection[pos];
}


/* #####################################################################################################################
 * CROSSOVER
 * */


int crossover_single_point(int size, int *chromo_a, int *chromo_b, int output[]) {
    // USED BY GUPTA

    int pos = rand() % size;  // Random bit

    // Copy the prefix of A into the output
    std::copy(chromo_a, chromo_a+pos, output);

    // Copy the suffix of source B into the output
    std::copy(chromo_b+pos+1, chromo_b+size, output);

    // Return the crossover point
    return pos;
}


/* #####################################################################################################################
 * MUTATION
 * */


int mutation_random_bit_flip(int size, int chromo[]) {
    // USED BY GUPTA

    int pos = rand() % size;  // Random bit
    chromo[pos] = (chromo[pos] == 1) ? 0 : 1 ;  // Bit flip

    // Return the position of the flipped bit
    return pos;
}


int mutation_random_set(int size, int chromo[]) {
    // Randomly set a zero to a one, with at most 2*size attempts

    int pos, limit = 0;
    do {pos = rand() % size; limit++;} while (chromo[pos] == 1 and (limit < (size*2))); // random bit that is a one
    chromo[pos] = 1 ;  // Set bit

    // Return the position of the set bit
    return pos;
}


int mutation_random_reset(int size, int chromo[]) {
    // Randomly set a one to a zero, with at most 2*size attempts

    int pos, limit = 0;
    do {pos = rand() % size; limit++;} while (chromo[pos] == 0 and (limit < (size*2))); // random bit that is a one
    chromo[pos] = 0 ;  // Reset bit

    // Return the position of the reset bit
    return pos;
}
