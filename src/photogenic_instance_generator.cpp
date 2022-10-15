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
    std::cout << "Please, use the correct input for the KCMC PHOTOGENIC instance generator:" << std::endl << std::endl;
    std::cout << "./instance_generator <p> <s> <k> <area_s> <cov_v> <com_r> <kcmc_k> <kcmc_m>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "p > 0 is the number of POIs to be randomly generated" << std::endl;
    std::cout << "s > 0 is the number of Sensors to be generated" << std::endl;
    std::cout << "k > 0 is the number of Sinks to be generated. If n=1, the sink will be placed at the center of the area" << std::endl;
    std::cout << "area > 0.0 is the int length of the square area where features will be placed" << std::endl;
    std::cout << "cov_r > 0.0 is the int radius around a Sensor where it can cover POIs" << std::endl;
    std::cout << "com_r > 0.0 is the int radius around a Sensor where it can communicate with other Sensors or Sinks" << std::endl << std::endl;
    std::cout << "kcmc_k > 0 is the K parameter of the KCMC problem" << std::endl;
    std::cout << "kcmc_m > 0 is the M parameter of the KCMC problem" << std::endl << std::endl;
    exit(0);
}


int main(int argc, char* const argv[]) {
    if (argc < 7) {help(argc, argv);}

    /* ======================== *
     * PARSE THE INPUT SETTINGS *
     * ======================== */

    /* Prepare Buffers */
    int attempt, num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius, k, m,
        MAX_TRIES=100000, invalid_count=0;
    long long random_seed;
    std::unordered_set<int> emptyset, ignoredset, seed_sensors,
                            set_dinic, set_min_flood, set_max_flood,
                            set_no_reuse, set_min_reuse, set_max_reuse, set_best_reuse;
    std::unordered_map<int, int> used_installation_spots;

    /* Parse CMD SETTINGS */
    num_pois    = atoi(argv[1]);
    num_sensors = atoi(argv[2]);
    num_sinks   = atoi(argv[3]);
    area_side   = atoi(argv[4]);
    coverage_radius = atoi(argv[5]);
    communication_radius = atoi(argv[6]);
    k = atoi(argv[7]);
    m = atoi(argv[8]);
    if (argc > 9) {random_seed = atoll(argv[9]);}
    else {
        // Get a random seed
        srand(time(NULL) + getpid());  // Diferent seed in each run for each process
        random_seed = 100000000 + std::abs((rand() % 100000000)) + std::abs((rand() % 100000000));
    }

    /* ================== *
     * GENERATE INSTANCES *
     * ================== */

    for (attempt=0; attempt<MAX_TRIES; attempt++) {
        if (argc <= 9) {random_seed = random_seed + std::abs((rand() % 100000)) + 7 + attempt;}
        auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                           area_side, coverage_radius, communication_radius,
                                           random_seed);
        if ((attempt % 10000) == 0) {std::cout << "Attempt " << attempt << " (v" << attempt-invalid_count << ") Seed " << random_seed << std::endl;}
        if (not instance->validate(false, k, m)) {
            invalid_count += 1;
            continue;
        };

        /* Here, the instance is VALID. We now need it to be photogenic.
         * A photogenic instance has different results for each preprocessing algorithm */

        // DINIC
        instance->local_optima(k, m, emptyset, &set_dinic);

        // Min-Flood
        used_installation_spots.clear();
        instance->flood(k, m, false, emptyset, &used_installation_spots);
        setify(set_min_flood, &used_installation_spots);

        // Max-Flood
        used_installation_spots.clear();
        instance->flood(k, m, true, emptyset, &used_installation_spots);
        setify(set_max_flood, &used_installation_spots);

        // No-Flood Reuse
        used_installation_spots.clear();
        instance->reuse(k, m, 0,emptyset, &used_installation_spots);
        setify(set_no_reuse, &used_installation_spots);

        // Min-Flood Reuse
        used_installation_spots.clear();
        instance->reuse(k, m, 1,emptyset, &used_installation_spots);
        setify(set_min_reuse, &used_installation_spots);

        // Max-Flood Reuse
        used_installation_spots.clear();
        instance->reuse(k, m, -1,emptyset, &used_installation_spots);
        setify(set_max_reuse, &used_installation_spots);

        // Best-Reuse
        used_installation_spots.clear();
        instance->reuse(k, m,emptyset, &used_installation_spots);
        setify(set_best_reuse, &used_installation_spots);

        // Check if all sets are different (except for best-reuse, that equals one of {no, min or max}-reuse)
        if (set_diff(set_dinic, set_min_flood).empty()) {continue;}
        if (set_diff(set_dinic, set_max_flood).empty()) {continue;}
        if (set_diff(set_dinic, set_no_reuse).empty()) {continue;}
        if (set_diff(set_dinic, set_min_reuse).empty()) {continue;}
        if (set_diff(set_dinic, set_max_reuse).empty()) {continue;}

        if (set_diff(set_min_flood, set_max_flood).empty()) {continue;}
        if (set_diff(set_min_flood, set_no_reuse).empty()) {continue;}
        if (set_diff(set_min_flood, set_min_reuse).empty()) {continue;}
        if (set_diff(set_min_flood, set_max_reuse).empty()) {continue;}

        if (set_diff(set_max_flood, set_no_reuse).empty()) {continue;}
        if (set_diff(set_max_flood, set_min_reuse).empty()) {continue;}
        if (set_diff(set_max_flood, set_max_reuse).empty()) {continue;}

        if (set_diff(set_no_reuse, set_min_reuse).empty()) {continue;}
        if (set_diff(set_no_reuse, set_max_reuse).empty()) {continue;}

        if (set_diff(set_min_reuse, set_max_reuse).empty()) {continue;}

        // If we got here, we have a photogenic instance
        std::cout << "GOT IT! " << random_seed << std::endl;
        return (0);
    }

    std::cout << "FAILURE AT " << MAX_TRIES << " TRIES!" << std::endl;
    return (1);
}
