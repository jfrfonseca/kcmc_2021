/*
 * KCMC Instance generator
 * Generates 1+ instances for the KCMC problem. INSTANCES MIGHT NOT BE VALID FOR ALL VALUES OF K AND M
 */


// STDLib Dependencies
#include <unistd.h>  // getpid
#include <iostream>  // cin, cout, endl, printf, fprintf

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
    std::cout << "./instance_generator <pois> <sensors> <sinks> <area_s> <cov_v> <com_r> <k> <m> <seed>+" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "pois > 0 is the number of POIs to be randomly generated" << std::endl;
    std::cout << "sensors > 0 is the number of Sensors to be generated" << std::endl;
    std::cout << "IGNORED PARAMETER sinks > 0 is the number of Sinks to be generated. If n=1, the sink will be placed at the center of the area" << std::endl;
    std::cout << "area > 0.0 is the int length of the square area where features will be placed" << std::endl;
    std::cout << "cov_r > 0.0 is the int radius around a Sensor where it can cover POIs" << std::endl;
    std::cout << "com_r > 0.0 is the int radius around a Sensor where it can communicate with other Sensors or Sinks" << std::endl << std::endl;
    std::cout << "k > 0 is the KCMC K" << std::endl;
    std::cout << "m >= k is the KCMC M" << std::endl;
    std::cout << "seed is an integer number that is used as seed of the PRNG." << std::endl;
    std::cout << "If many seeds are provided, one instance will be generated for each." << std::endl;
    std::cout << "If the seed is 0, a random seed will be generated." << std::endl;
    std::cout << "Very photogenic instance: 3 20 1 300 50 100 3 2 616917773" << std::endl;
    exit(0);
}


long long generate_kcmc_instance(
    int num_pois, int num_sensors, int num_sinks,
    int area_side, int coverage_radius, int communication_radius,
    int k, int m, long long random_seed,
    int budget
) {
    // Prepare buffers
    long long num_iterations = -1;
    std::unordered_set<int> emptyset;

    // Try many times until get a valid instance
    while (num_iterations < budget) {  // MANY ATTEMPTS!
        num_iterations++;

        // Makes a new instance with the given parameters
        auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                           area_side, coverage_radius, communication_radius,
                                           random_seed+num_iterations);

        // Validate the instance. If invalid, skip to the next loop
        if (not validate_kcmc_instance(instance, k, m, emptyset)) {continue;}

        // If we got here, success! Return the successfull random seed
        return random_seed+num_iterations;
    }

    // If we got here, FAIL!
    return 0;
}


int main(int argc, char* const argv[]) {
    if (argc < 9) {help(argc, argv);}

    /* ======================== *
     * PARSE THE INPUT SETTINGS *
     * ======================== */

    /* Prepare Buffers */
    int i, num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius, k, m, budget=10000;
    long long previous_seed=0, random_seed, malted_seed;
    std::unordered_set<int> emptyset, previous_seeds;

    /* Parse CMD SETTINGS */
    num_pois    = atoi(argv[1]);
    num_sensors = atoi(argv[2]);
    num_sinks   = 1;  //atoi(argv[3]);
    area_side   = atoi(argv[4]);
    coverage_radius = atoi(argv[5]);
    communication_radius = atoi(argv[6]);
    k = atoi(argv[7]);
    m = atoi(argv[8]);

    /* ================== *
     * GENERATE INSTANCES *
     * ================== */

    for (i=9; i<argc; i++) {
        random_seed = atoll(argv[i]);
        if (random_seed == 9876543210) {continue;}  // Secret code to skip printout

        if (random_seed == 0) {
            srand(time(NULL) + getpid()*20);  // Diferent seed in each run for each process
            do {
                random_seed = 100000000 + std::abs((rand() % 100000000)) + std::abs((rand() % 100000000));  // LARGE but random-er number
            } while (isin(previous_seeds, random_seed));
            previous_seeds.insert(random_seed);
        }

        malted_seed = generate_kcmc_instance(
            num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius,
            k, m, random_seed, budget
        );

        // If the seed malted, success!
        if (malted_seed > 0) {
            auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                               area_side, coverage_radius, communication_radius,
                                               malted_seed);

            printf("KCMC;%s;END\t%d\t%d\n", instance->key().c_str(), k, m);

            // Print as TIKZ if there was only one instance
            // Also test validity, serialization and regeneration
            if (argc == 10) {

                // Validity
                if (not validate_kcmc_instance(instance, k, m, emptyset)) {
                    throw std::runtime_error("INVALID INSTANCE!");
                }

                // Serialization (long format)
                std::string serial = instance->serialize();

                // Deserialization (long format)
                auto *second_instance = new KCMC_Instance(serial);
                std::string second_serial = second_instance->serialize();
                if (not std::equal(serial.begin(), serial.end(), second_serial.begin(), second_serial.end())) {
                    throw std::runtime_error("LONG SERIALIZATION/DESERIALIZATION FAILED");
                }

                // Serialization/deserialization (short format)
                second_serial = instance->key();
                second_serial.insert(0, "KCMC;");
                second_serial.append(";END");
                //printf("%% %s", second_serial.c_str());

                second_instance = new KCMC_Instance(second_serial);
                second_serial = second_instance->serialize();
                if (not std::equal(serial.begin(), serial.end(), second_serial.begin(), second_serial.end())) {
                    throw std::runtime_error("SHORT SERIALIZATION/DESERIALIZATION FAILED");
                }

                // Print as TIKZ
                print_tikz(instance, 10);
            }

        // If not, failure!
        } else {
            printf("UNABLE TO GENERATE VALID INSTANCE WITH PARAMETERS %d %d %d %d %d %d 0 %d %d %lld IN %d iterations\n",
                   num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius,
                   k, m, random_seed, budget);
        }
    }
    return (0);
}
