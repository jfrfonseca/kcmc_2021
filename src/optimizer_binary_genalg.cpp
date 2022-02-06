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

/** Fitness Function (MIN)
 * The best possible fitness has the minimal number of active sensors for the instance to have K-Coverage and
 * M-Connectivity at the same time.
 * Valid instances have fitness <= number of sensors.
 * Invalid instances have fitness that is the number of sensors used summed with a penalty value for each violation
 * A violation is a POI that has less than K-Coverage of M-Connectivity. The same POI might incur in several violations.
 * The penalty value in each violation is the product by its severity (i.e. a POI that has 1-Coverage when K=4 has a
 * violation of severity 3), the total number of sensors in the instance, and the weight of the type of violation (
 * coverage or connectivity).
 * Thus, the worst theorical maximal fitness is NUM_SENSORS + (((K*w_k*NUM_SENSORS) + (M*w_m*NUM_SENSORS)) * NUM_POIS)
 * @param wsn
 * @param K
 * @param M
 * @param weight_k
 * @param weight_m
 * @param chromo
 * @return
 */
double fitness_binary(KCMC_Instance *wsn, int K, int M, double weight_k, double weight_m, int *chromo) {

    // Define reused buffers
    int i, severity;
    double fitness;

    // Get the set of inactive sensors, the coverage and connectivity array at each POI
    std::unordered_set<int> inactive_sensors;
    setify(inactive_sensors, wsn->num_sensors, chromo, 0);

    // Compute the starting fitness as the number of active sensors
    fitness = (double)(wsn->num_pois - inactive_sensors.size());

    // Get the coverage and connectivity at each POI
    int coverage[wsn->num_pois], connectivity[wsn->num_pois];
    wsn->get_coverage(coverage, inactive_sensors);
    wsn->get_connectivity(connectivity, inactive_sensors, M);

    // Compute the penalties on validity violations and return the total fitness
    for (i=0; i<wsn->num_pois; i++) {
        severity = K-coverage[i];  // Get the severity of the Coverage violation. 0 or less do not incur in penalties
        if (severity > 0) {fitness += (severity*weight_k*wsn->num_sensors);}
        severity = M-connectivity[i];  // Get the severity of the Connectivity violation. 0 or less do not incur in penalties
        if (severity > 0) {fitness += (severity*weight_m*wsn->num_sensors);}
    }
    return fitness;
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
 * @param w_coverage      Weight of the penalty on coverage violations
 * @param w_connectivity  Weight of the penalty on connectivity violations
 * @return
 */
int genalg_binary(
    std::unordered_set<int> *unused_sensors,
    int print_interval, int max_generations, int pop_size, int sel_size, float mut_rate, float one_bias,
    KCMC_Instance *wsn, int K, int M,
    double w_valid, double w_invalid
) {
    // Prepare buffers
    int i, best, num_generation, parent_0, parent_1,
        chromo_size = wsn->num_sensors,
        population[pop_size][chromo_size];
    double pop_entropy, best_fitness_ever = WORST_FITNESS, fitness[pop_size], colunar_entropy[chromo_size];
    std::vector<int> selection;

    // FLAGS
    bool SAFE = true,
         ELITISM = true;  // The best individual always stays intact in the next generation

    // Prepare an alternate buffer for the population
    // Look, it's C++, OK? Sometimes things like that are necessary
    int *pop[pop_size];
    if (SAFE) {for (size_t j = 0; j<pop_size; j++) {pop[j] = population[j];}}

    // Generate a random population
    for (i=0; i<pop_size; i++) {individual_creation(one_bias, chromo_size, population[i]);}

    // Evolve "FOREVER". THE OS IS SUPPOSED TO HANDLE TIMEOUTS!
    // This software assumes that the OS will handle timeouts, thus avoiding
    // overhead and complexity in the algorithm itself. As a fallback security
    // measure, we limit the generations to a otherwise very large number.
    // The software will handle gracefully OS signals SIGINT, SIGALRM, SIGABRT and SIGTERM
    for (num_generation=0; num_generation<max_generations+1; num_generation++) {

        // If in safe mode, inspect the population once every INSPECTION_FREQUENCY generations
        if (SAFE & ((num_generation % INSPECTION_FREQUENCY) == 0)) {inspect_population(pop_size, wsn->num_sensors, pop);}

        // Evaluate the population and find the best
        for (i=0; i<pop_size; i++) {fitness[i] = fitness_binary(wsn, K, M, w_valid, w_invalid, population[i]);}
        best = ((int)(std::min_element(fitness, fitness + pop_size) - fitness));

        // If the current best is the best ever found,
        // or if we have run the appropriate interval of generations.
        if (((num_generation % print_interval) == 0) | (fitness[best] < best_fitness_ever)) {

            // Compute the population's entropy, average and by column
            pop_entropy = population_entropy(colunar_entropy, pop_size, chromo_size, pop);

            // Print the best individual in the population
            printout(num_generation, pop_entropy, chromo_size, population[best], fitness[best]);

            // Update the best fitness ever found and the resulting set of unused sensors
            best_fitness_ever = fitness[best];
            setify(*unused_sensors, chromo_size, population[best], 0);
        }

        // Select individuals for next generation
        selection_roulette(sel_size, &selection, pop_size, fitness);

        // For every population position that was *not* selected
        for (i=0; i<pop_size; i++) {
            if ((not isin(selection, i)) and ((i != best) or (not ELITISM))) {

                // Choose 2 different individuals among the selected in this generation
                parent_0 = selection_get_one(sel_size, selection, -1);
                parent_1 = selection_get_one(sel_size, selection, parent_0);

                // Replace the population position with a crossover of the selected pair
                crossover_single_point(chromo_size, population[parent_0], population[parent_1], population[i]);
            }
        }

        // For every individual in the population
        for (i=0; i<pop_size; i++) {
            // If this individual got lucky, randomly flip a bit
            if ((((double) rand() / (RAND_MAX)) < mut_rate) and ((i != best) or (not ELITISM))) {
                mutation_random_bit_flip(chromo_size, population[i]);
            }
        }
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

