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


void help() {
    std::cout << "Please, use the correct input for the KCMC instance generator:" << std::endl << std::endl;
    std::cout << "./graph_instance_generator <k_range> <m_range> <p> <s> <k> <cov_v> <com_r> <seed>+" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "k_range >= 0 is the maximum value of K for each instance to be tested for. Only if K>=M" << std::endl;
    std::cout << "m_range >= 0 is the maximum value of M for each instance to be tested for. Only if K>=M" << std::endl;
    std::cout << "p > 0 is the number of POIs to be randomly generated" << std::endl;
    std::cout << "s > 0 is the number of Sensors to be generated" << std::endl;
    std::cout << "n > 0 is the number of Sinks to be generated. If n=1, the sink will be placed at the center of the area" << std::endl;
    std::cout << "area > 0.0 is the int length of the square area where features will be placed" << std::endl;
    std::cout << "cov_r > 0.0 is the int radius around a Sensor where it can cover POIs" << std::endl;
    std::cout << "com_r > 0.0 is the int radius around a Sensor where it can communicate with other Sensors or Sinks" << std::endl << std::endl;
    std::cout << "seed is an integer number that is uded as seed of the PRNG." << std::endl;
    std::cout << "++ If more than one seed is provided, many instances will be generated" << std::endl;
    std::cout << "++ If a single instance is provided, its de-serialization will be tested" << std::endl;
    exit(0);
}



int main(int argc, char* const argv[]) {
    if (argc < 8) {help();}

    /* ======================== *
     * PARSE THE INPUT SETTINGS *
     * ======================== */

    /* Prepare Buffers */
    int k_range, m_range, k, m, i, num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius;
    long long random_seed;
    std::unordered_set<int> emptyset;
    std::string msg_k[10], msg_m[10];
    msg_k[0] = "SUCCESS";
    msg_m[0] = "SUCCESS";

    /* Parse CMD SETTINGS */
    k_range = atoi(argv[1]);
    m_range = atoi(argv[2]);
    num_pois    = atoi(argv[3]);
    num_sensors = atoi(argv[4]);
    num_sinks   = atoi(argv[5]);
    area_side   = atoi(argv[6]);
    coverage_radius = atoi(argv[7]);
    communication_radius = atoi(argv[8]);

    for (i=9; i<argc; i++) {
        random_seed = atoll(argv[i]);

        try {
            auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                               area_side, coverage_radius, communication_radius,
                                               random_seed);
            printf("%s\n", instance->serialize().c_str());

            /* TEST EACH VALUE OF K */
            for (k=1; k<=k_range; k++) {      // There is no point in testing for K=0
                if (msg_k[k-1] != "SUCCESS") {msg_k[k] = msg_k[k-1];}
                else {msg_k[k] = instance->k_coverage(k, emptyset);}
            }

            /* TEST EACH VALUE OF M */
            for (m=1; m<=m_range; m++) {  // There is no point in testing for M=0
                if (msg_m[m-1] != "SUCCESS") {msg_m[m] = msg_m[m-1];}
                else {msg_m[m] = instance->m_connectivity(m, emptyset);}
            }

            /* TEST EACH VALUE OF K AND M COMBINATION OF K AND M */
            for (k=1; k<=k_range; k++) {      // There is no point in testing for K=0
                for (m=1; m<=m_range; m++) {  // There is no point in testing for M=0
                    if (k < m) {continue;}    // Cases where k < m are invalid
                    std::cout << k << ' ' << m << " | " << msg_k[k] << " | " << msg_m[m] << ";\t";
            }}
            std::cout << '\n';

            /* FOR VERIFICATION */
            if (argc == 8){
                std::string serialized_instance = instance->serialize();
                auto *new_instance = new KCMC_Instance(serialized_instance);
                if (new_instance->serialize() == instance->serialize()){
                    printf("%s\nEQUAL\n", new_instance->serialize().c_str());
                } else {throw std::runtime_error("NOT EQUAL!");}
            }

        } catch (const std::exception &exc) {
            if (argc == 8) {throw  exc;}  // Only throw the exception if we're in DEBUG mode
            fprintf(stderr, "%lld\t%s\n", random_seed, exc.what());
        }
    }

    return (0);
}
