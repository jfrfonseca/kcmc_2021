/*
 * KCMC Instance evaluator
 * Evaluates instances for the KCMC problem. Operates like an UNIX filter
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


double fitness_gupta_exact(KCMC_Instance *wsn, int K, int M, double w1, double w2, double w3, int *chromo) {

    // Reused buffers
    double f1, f2 = 0.0, f3 = 0.0;
    int i, num_active_sensors = 0, poi_coverage[wsn->num_pois], sensor_degree[wsn->num_sensors];

    // Format the chromossome as an unordered set of unused sensor installation spots. Count the number of used ones
    std::unordered_set<int> inactive_sensors;
    for (i=0; i<wsn->num_sensors; i++) {
        if (chromo[i] == 0) {
            inactive_sensors.insert(i);
        } else {
            num_active_sensors += 1;
        }
    }

    // Objective F1 of Gupta - minimize the fraction of selected installation spots ------------------------------------

    // Compute the fraction of used sensor installation spots from the total available
    f1 = (double)(num_active_sensors) / (double)(wsn->num_sensors);

    // Objective F2 of Gupta - Maximize the proper fraction of the target coverage of POIs -----------------------------

    // Compute the total coverage of each POI
    wsn->get_coverage(poi_coverage, inactive_sensors);

    // Accumulate the coverage, using Gupta's normalization
    for (i=0; i<wsn->num_pois; i++) {
        f2 += (double)((poi_coverage[i] >= K) ? K : (K - poi_coverage[i]));  // either K or K-coverage(poi_i)
    }

    // Normalize to the 0-1 interval, as a proper fraction of the K*num_pois target coverage
    f2 = f2 / (double)(K * wsn->num_pois);

    // Objective F3 - Maximize the proper fraction of the target connectivity of POIs ----------------------------------
    // Gupta considers only the degree of each sensor as its connectivity!

    // Compute the total degree of each Sensor
    wsn->get_degree(sensor_degree, inactive_sensors);
    for (i=0; i<wsn->num_sensors; i++) {
        f3 += (double)((sensor_degree[i] >= M) ? M : (M - sensor_degree[i]));  // either M or M-degree(sensor_i)
    }

    // Normalize to the 0-1 interval, as a proper fraction of the M*num_sensors target connectivity
    f3 = f3 / (double)(M * wsn->num_sensors);

    // Weighted Linear Combination of the objectives -------------------------------------------------------------------
    return (w1*(1.0-f1)) + (w2*f2) + (w3*f3) / (w1+w2+w3);  // Normalized between 0 and 1
}


/** Genetic Algorithm Exactly as defined by Gupta (2015)
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
 * @param w1              GUPTA (2015) Weight for Objective 1
 * @param w2              GUPTA (2015) Weight for Objective 2
 * @param w3              GUPTA (2015) Weight for Objective 3
 * @return
 */
int genalg_gupta_exact(
    std::unordered_set<int> *unused_sensors,
    int print_best, int max_generations, int pop_size, int sel_size, float mut_rate,
    KCMC_Instance *wsn, int K, int M,
    double w1, double w2, double w3
) {
    // Prepare buffers
    int i, level_best=INT32_MAX, current_best, num_generation, parent_0, parent_1, chromo_size = wsn->num_sensors;
    int population[pop_size][chromo_size];
    double fitness[pop_size];
    std::vector<int> selection;
    bool SAFE = true;

    // Prepare an alternate buffer for the population
    // Look, it's C++, OK? Sometimes things like that are necessary
    int *pop[chromo_size];
    if (SAFE) {for (size_t j = 0; j<pop_size; j++) {pop[j] = population[j];}}

    // Generate a random population
    for (i=0; i<pop_size; i++) {
        individual_creation(0.5, chromo_size, population[i]);  // EQUAL BIAS FOR ONES AND ZEROES
    }

    // Evolve "FOREVER". THE OS IS SUPPOSED TO HANDLE TIMEOUTS!
    // This software assumes that the OS will handle timeouts, thus avoiding
    // overhead and complexity in the algorithm itself. As a fallback security
    // measure, we limit the generations to a otherwise very large number.
    // The software will handle gracefully OS signals SIGINT, SIGALRM, SIGABRT and SIGTERM
    for (num_generation=0; num_generation<max_generations+1; num_generation++) {

        // Evaluate the population
        for (i=0; i<pop_size; i++) {fitness[i] = fitness_gupta_exact(wsn, K, M, w1, w2, w3, population[i]);}

        // Print the best individual, that will be stored in the unused_sensors set
        current_best = get_best_individual(print_best, unused_sensors, chromo_size, pop_size, pop, fitness, num_generation, level_best);
        level_best = current_best < level_best ? current_best : level_best;  // Get always the smallest as best

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
            // If this individual got lucky, randomly flip a bit
            if (((double) rand() / (RAND_MAX)) < mut_rate) {mutation_random_bit_flip(chromo_size, population[i]);}
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
    std::cout << "Please, use the correct input for the KCMC instance optimizer, Exact Gupta (2015) version:" << std::endl << std::endl;
    std::cout << "./optimizer_gupta_exact <v> <p> <c> <r> <k> <m> <w1> <w2> <w3> <instance>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "V >= 0 is the desired Verbosity level - generations interval between individual printouts" << std::endl;
    std::cout << "P > 5 is the desired Population size" << std::endl;
    std::cout << "C > 3 is the desired Selection/Crossover Population Size" << std::endl;
    std::cout << "0 <= R <= 1.0 is the desired Individual Mutation Rate" << std::endl;
    std::cout << "K > 0 is the desired K coverage" << std::endl;
    std::cout << "M >= K is the desired M connectivity" << std::endl;
    std::cout << "w1 > 0.0 is the double weight for the F1 objective of Gupta (2015)" << std::endl;
    std::cout << "w2 > 0.0 is the double weight for the F2 objective of Gupta (2015)" << std::endl;
    std::cout << "w3 > 0.0 is the double weight for the F3 objective of Gupta (2015)" << std::endl;
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
    float mut_rate;
    double w1, w2, w3;
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
    k = std::stoi(argv[5]);
    m = std::stoi(argv[6]);
    w1 = std::stod(argv[7]);
    w2 = std::stod(argv[8]);
    w3 = std::stod(argv[9]);
    auto *instance = new KCMC_Instance(argv[10]);

    // Optimize the instance using one of the optimization methods
    genalg_gupta_exact(&unused_installation_spots, print_interval, 100000,
                       pop_size, sel_size, mut_rate,
                       instance, k, m, w1, w2, w3);

    return 0;
}

