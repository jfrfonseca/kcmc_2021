/*
 * KCMC Instance optimizer
 * Optimizes instances using a Genetic Algorithm that only rewards valid instances
 */


// STDLib Dependencies
#include <csignal>   // SIGINT,
#include <iostream>  // cin, cout, endl

// Dependencies from this package
#include "kcmc_instance.h"
#include "genetic_algorithm_operators.h"


/* #####################################################################################################################
 * GENETIC ALGORITHM
 * */


double fitness_binary(KCMC_Instance *wsn, int K, int M, double w_valid, double w_invalid, int *chromo) {

    // Format the chromossome as an unordered set of unused sensor installation spots,
    // and get another unordered set of ints for storing the used sensors
    std::unordered_set<int> inactive_sensors, all_used_sensors;
    for (int i=0; i<wsn->num_sensors; i++) {
        if (chromo[i] == 0) {
            inactive_sensors.insert(i);
        }
    }
    double unused_fraction = (double)(inactive_sensors.size()) / (double)(wsn->num_sensors);

    // Check the validity of the instance with the inactive sensors
    if (wsn->fast_k_coverage(K, inactive_sensors) == -1) {
        if (wsn->fast_m_connectivity(M, inactive_sensors, &all_used_sensors) == -1) {
            return unused_fraction * w_valid;
        }
    }
    return unused_fraction * w_invalid;
}


/** Genetic Algorithm with binary tiers of fitness, for valid and invalid solutions
 *
 * @param unused_sensors  Output Buffer
 * @param print_best      Generations interval until printing the best individual to STDOUT
 * @param max_generations MAX GenAlg Generations (Iterations)
 * @param pop_size        Population Size
 * @param sel_size        Selection/Crossover Group Size
 * @param mut_rate        Mutation Rate
 * @param wsn             KCMC WSN Instance
 * @param K               KCMC K
 * @param M               KCMC M
 * @param w_valid         Maximum fitness for valid solutions
 * @param w_invalid       Maximum fitness for invalid solutions
 * @return
 */
int genalg_binary(
    std::unordered_set<int> *unused_sensors,
    int print_best, int max_generations, int pop_size, int sel_size, float mut_rate, float one_bias,
    KCMC_Instance *wsn, int K, int M,
    double w_valid, double w_invalid
) {
    // Prepare buffers
    int i, local_best, local_worst, num_generation, parent_0, parent_1, chromo_size = wsn->num_sensors;
    int population[pop_size][chromo_size], local_optima[chromo_size];
    double fitness[pop_size], best_fitness = 0.0, local_optima_fitness;
    std::unordered_set<int> local_optima_used_sensors;
    std::vector<int> selection;
    bool SAFE = true;

    // Prepare an alternate buffer for the population
    // Look, it's C++, OK? Sometimes things like that are necessary
    int *pop[pop_size];
    if (SAFE) {for (size_t j = 0; j<pop_size; j++) {pop[j] = population[j];}}

    // Generate a random population
    for (i=0; i<pop_size; i++) {
        individual_creation(one_bias, chromo_size, population[i]);
    }

    // Evolve "FOREVER". THE OS IS SUPPOSED TO HANDLE TIMEOUTS!
    // This software assumes that the OS will handle timeouts, thus avoiding
    // overhead and complexity in the algorithm itself. As a fallback security
    // measure, we limit the generations to a otherwise very large number.
    // The software will handle gracefully OS signals SIGINT, SIGALRM, SIGABRT and SIGTERM
    for (num_generation=0; num_generation<max_generations+1; num_generation++) {

        // Evaluate the population
        for (i=0; i<pop_size; i++) {fitness[i] = fitness_binary(wsn, K, M, w_valid, w_invalid, population[i]);}

        // Print the best individual, that will be stored in the unused_sensors set.
        local_best = get_best_individual(
            print_best,
            unused_sensors,
            chromo_size,
            pop_size,
            pop,
            fitness,
            num_generation,
            best_fitness
        );

        // If the local best individual has a higher fitness than the overall best
        if (fitness[local_best] > best_fitness) {

            // Update the overall best fitness ever found
            best_fitness = fitness[local_best];

            // Create an idealized version (local optima) of the best individual
            wsn->local_optima(K, M, *unused_sensors, &local_optima_used_sensors);
            for (i=0; i<chromo_size; i++) {local_optima[i] = isin(local_optima_used_sensors, i) ? 1: 0;}

            // Replace the worst fitted individual with the local optima, and recompute its fitness
            local_worst = ((int)(std::min_element(fitness, fitness + pop_size) - fitness));
            for (i=0; i<chromo_size; i++) {population[local_worst][i] = local_optima[i];}
            fitness[local_worst] = fitness_binary(wsn, K, M, w_valid, w_invalid, local_optima);

            // Printout the local optima
            // The local_worst position is now the location of the local optima!
            //  To avoid reprinting the header, there is this dirty trick at the num_generation parameter
            printout(((num_generation > 0) ? num_generation : 1), chromo_size, local_optima, fitness[local_worst]);
        }

        // Select individuals for next generation
        selection_roulette(sel_size, &selection, pop_size, fitness);

        // For every population position that was *not* selected
        for (i=0; i<pop_size; i++) {
            if (not isin(selection, i)) {

                // Choose 2 different individuals among the selected in this generation
                parent_0 = selection_get_one(sel_size, selection, -1);
                parent_1 = selection_get_one(sel_size, selection, parent_0);

                // Replace the population position with a crossover of the selected pair
                crossover_single_point(chromo_size, population[parent_0], population[parent_1], population[i]);
            }
        }

        // For every individual in the population
        for (i=0; i<pop_size; i++) {
            // If this individual got lucky,
            if (((double) rand() / (RAND_MAX)) < mut_rate) {
                // If it is likely to be a VALID individual, flip a random one to a zero
                if (fitness[i] > w_invalid) {
                    mutation_random_reset(chromo_size, population[i]);
                // if not, flip a zero to a one
                } else {
                    mutation_random_set(chromo_size, population[i]);
                }
                // mutation_random_bit_flip(chromo_size, population[i]);
            }
        }

        // If in safe mode, inspect the population once every 10 generations
        if (SAFE & ((num_generation % 10) == 0)) {inspect_population(pop_size, wsn->num_sensors, pop);}
    }
    std::cerr << " Reached HARD-LIMIT OF GENERATIONS (" << num_generation-1 << "). Exiting gracefully..." << std::endl;
    return num_generation;
}


/* #####################################################################################################################
 * RUNTIME
 * */


void help() {
    std::cout << "Please, use the correct input for the KCMC instance optimizer, binary tiers version:" << std::endl << std::endl;
    std::cout << "./optimizer_gupta_exact <v> <p> <c> <r> <k> <m> <o_b> <w_v> <w_i> <instance>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "V >= 0 is the desired Verbosity level - generations interval between individual printouts" << std::endl;
    std::cout << "P > 5 is the desired Population size" << std::endl;
    std::cout << "C > 3 is the desired Selection/Crossover Population Size" << std::endl;
    std::cout << "0 <= R <= 1.0 is the desired Individual Mutation Rate" << std::endl;
    std::cout << "0.0 < one_bias < 1.0 is the bias for ones when generating individuals" << std::endl;
    std::cout << "K > 0 is the desired K coverage" << std::endl;
    std::cout << "M >= K is the desired M connectivity" << std::endl;
    std::cout << "w_valid > 0.0 is the double maximum fitness of valid solutions" << std::endl;
    std::cout << "w_invalid > 0.0 is the double maximum fitness of valid solutions" << std::endl;
    std::cout << "<instance> is the serialized KCMC instance" << std::endl;
    exit(0);
}

int main(int argc, char* const argv[]) {
    if (argc < 10) { help(); }

    // Registers the signal handlers
    signal(SIGINT, exit_signal_handler);
    signal(SIGALRM, exit_signal_handler);
    signal(SIGABRT, exit_signal_handler);
    signal(SIGSTOP, exit_signal_handler);
    signal(SIGTERM, exit_signal_handler);
    signal(SIGKILL, exit_signal_handler);

    // Buffers
    int print_interval, pop_size, sel_size, i, k, m;
    float mut_rate, one_bias;
    double w_valid, w_invalid;
    std::unordered_set<int> unused_installation_spots;
    std::string serialized_instance, k_cov, m_conn;

    /* Parse base Arguments
     * KCMC K and M parameters
     * Serialized KCMC Instance (will be immediately de-serialized
     * Weights for the 3 optimization objectives of Gupta (2015)
     * */
    print_interval = std::stoi(argv[1]);
    pop_size = std::stoi(argv[2]);
    sel_size = std::stoi(argv[3]);
    mut_rate = std::stof(argv[4]);
    one_bias = std::stof(argv[5]);
    k = std::stoi(argv[6]);
    m = std::stoi(argv[7]);
    w_valid = std::stod(argv[8]);
    w_invalid = std::stod(argv[9]);
    auto *instance = new KCMC_Instance(argv[10]);
    std::unordered_set<int> emptyset, ignoredset;

    if (instance->fast_k_coverage(k, emptyset) != -1) {throw std::runtime_error("INVALID INSTANCE!");}
    if (instance->fast_m_connectivity(m, emptyset, &ignoredset) != -1) {throw std::runtime_error("INVALID INSTANCE!");}

    // Optimize the instance using one of the optimization methods
    genalg_binary(&unused_installation_spots, print_interval, 100000,
                  pop_size, sel_size, mut_rate, one_bias,
                  instance, k, m, w_valid, w_invalid);

    return 0;
}

