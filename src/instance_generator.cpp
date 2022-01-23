/*
 * KCMC Instance generator
 * Generates 1+ instances for the KCMC problem. INSTANCES MIGHT NOT BE VALID FOR ALL VALUES OF K AND M
 */


// STDLib Dependencies
#include <iostream>  // cin, cout, endl

// Dependencies from this package
#include "kcmc_instance.h"


/* #####################################################################################################################
 * RUNTIME
 * */


void help(int argc, char* const argv[]) {
    std::cout << "RECEIVED LINE (" << argc << "): ";
    for (int i=0; i<argc; i++) {std::cout << argv[i] << " ";}
    std::cout << std::endl;
    std::cout << "Please, use the correct input for the KCMC instance generator:" << std::endl << std::endl;
    std::cout << "./instance_generator <p> <s> <k> <area_s> <cov_v> <com_r> <seed>+" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "p > 0 is the number of POIs to be randomly generated" << std::endl;
    std::cout << "s > 0 is the number of Sensors to be generated" << std::endl;
    std::cout << "k > 0 is the number of Sinks to be generated. If n=1, the sink will be placed at the center of the area" << std::endl;
    std::cout << "area > 0.0 is the int length of the square area where features will be placed" << std::endl;
    std::cout << "cov_r > 0.0 is the int radius around a Sensor where it can cover POIs" << std::endl;
    std::cout << "com_r > 0.0 is the int radius around a Sensor where it can communicate with other Sensors or Sinks" << std::endl << std::endl;
    std::cout << "seed is an integer number that is uded as seed of the PRNG." << std::endl;
    std::cout << "++ If more than one seed is provided, many instances will be generated" << std::endl;
    std::cout << "++ If a single instance is provided, its de-serialization will be tested" << std::endl;
    exit(0);
}



int main(int argc, char* const argv[]) {
    if (argc < 7) {help(argc, argv);}

    /* ======================== *
     * PARSE THE INPUT SETTINGS *
     * ======================== */

    /* Prepare Buffers */
    int i, num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius, success, k, m;
    long long random_seed, previous_seed = 1000000000;
    std::unordered_set<int> emptyset, ignoredset;

    /* Parse CMD SETTINGS */
    num_pois    = atoi(argv[1]);
    num_sensors = atoi(argv[2]);
    num_sinks   = atoi(argv[3]);
    area_side   = atoi(argv[4]);
    coverage_radius = atoi(argv[5]);
    communication_radius = atoi(argv[6]);

    /* ================== *
     * GENERATE INSTANCES *
     * ================== */

    for (i=7; i<argc; i++) {
        random_seed = atoll(argv[i]);

        // FAIL-SAFE mode
        if (random_seed == 0) {

            // Read K and M
            k = atoi(argv[i+1]);
            m = atoi(argv[i+2]);
            i += 2;

            // Start from the last random seed
            random_seed = previous_seed + 1;

            // Try many times until get a valid instance
            while (random_seed < (previous_seed + 10000)) {  // MANY ATTEMPTS!
                success = 0;
                auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                                   area_side, coverage_radius, communication_radius,
                                                   random_seed);
                success = instance->fast_k_coverage(k, emptyset);
                if (success == -1) {
                    success = instance->fast_m_connectivity(m, emptyset, &ignoredset);
                    if (success == -1) {
                        printf("%s\t(K%dM%d)\n", instance->serialize().c_str(), k, m);
                        previous_seed = random_seed;
                        break;
                    }
                }
                random_seed++;
            }
            if (success != -1) {printf("UNABLE TO GENERATE VALID INSTANCE WITH PARAMETERS %d %d %d %d %d %d 0 %d %d\n",
                                       num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius, k, m);}
        } else {
            // FAIL-PRONE MODE
            try {
                auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                                   area_side, coverage_radius, communication_radius,
                                                   random_seed);
                printf("%s\n", instance->serialize().c_str());

                /* FOR VERIFICATION */
                if (argc == 7) {
                    std::string serialized_instance = instance->serialize();
                    auto *new_instance = new KCMC_Instance(serialized_instance);
                    if (new_instance->serialize() == instance->serialize()) {
                        printf("%s\nEQUAL\n", new_instance->serialize().c_str());
                    } else { throw std::runtime_error("NOT EQUAL!"); }
                }

            } catch (const std::exception &exc) {
                if (argc == 8) { throw exc; }  // Only throw the exception if we're in DEBUG mode
                fprintf(stderr, "%lld\t%s\n", random_seed, exc.what());
            }
        }
    }

    return (0);
}
