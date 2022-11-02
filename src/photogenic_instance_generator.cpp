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
    bool must_break;
    int attempt, num_pois, num_sensors, num_sinks, area_side, coverage_radius, communication_radius, k, m,
        MAX_TRIES=200000, invalid_count=0, compare_buffer[7], i, j, last_print, valid_cases, level_graph[10000],
        algo_results[7][10000];
    long long random_seed;
    std::string name_map[7];
    std::unordered_set<int> emptyset, ignoredset, seed_sensors, set_dinic;
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
     * - 3 20 1;300 50 100;7150115257
     * - 3 25 1;300 50 100;986917529
     *
     * UNFIXED
     * - 3 20 1;300 50 100;222551069
     * - 3 20 1;300 50 100;6752538684
     * - 3 20 1;300 50 100;11014816988
     * - 3 21 1;300 50 100;335057979
     * - 3 21 1;300 50 100;2211105303
     * - 3 25 1 300 50 100 3 2 358542789
     * ================== */

    last_print = 0;
    for (attempt=0; attempt<MAX_TRIES; attempt++) {
        if (argc <= 9) { random_seed = random_seed + std::abs((rand() % 100000)) + 7 + attempt; }
        auto *instance = new KCMC_Instance(num_pois, num_sensors, num_sinks,
                                           area_side, coverage_radius, communication_radius,
                                           random_seed);
        valid_cases = attempt - invalid_count;
        if ((((attempt % 5000) == 0) or ((valid_cases % 50) == 0)) and (valid_cases != last_print)) {
            std::cout << "Attempt " << attempt << " (v" << valid_cases << ") Seed " << random_seed << std::endl;
            last_print = valid_cases;
        }
        if (not instance->validate(false, k, m)) {
            invalid_count += 1;
            continue;
        };

        /* Here, the instance is VALID. We now need it to be photogenic.
         * A photogenic instance has different results for each preprocessing algorithm */

        try {
            // DINIC
            name_map[0] = "DINIC";
            instance->local_optima(k, m, emptyset, &set_dinic);
            compare_buffer[0] = (int) set_dinic.size();
            for (i=0; i<num_sensors; i++) {algo_results[0][i] = (isin(set_dinic, i)) ? 1:0;}

            // Min-Flood
            name_map[1] = "MIN-FLOOD";
            used_installation_spots.clear();
            instance->flood(k, m, false, emptyset, &used_installation_spots);
            compare_buffer[1] = (int) used_installation_spots.size();
            for (i=0; i<num_sensors; i++) {algo_results[1][i] = (isin(used_installation_spots, i)) ? 1:0;}

            // Speed-Up the most common case 01
            if (compare_buffer[0] == compare_buffer[1]) {continue;}

            // Max-Flood
            name_map[2] = "MAX-FLOOD";
            used_installation_spots.clear();
            instance->flood(k, m, true, emptyset, &used_installation_spots);
            compare_buffer[2] = (int) used_installation_spots.size();
            for (i=0; i<num_sensors; i++) {algo_results[2][i] = (isin(used_installation_spots, i)) ? 1:0;}

            // Speed-Up the third most common case 12
            if (compare_buffer[1] == compare_buffer[2]) {continue;}

            // No-Flood Reuse
            name_map[3] = "NO-FLOOD REUSE";
            used_installation_spots.clear();
            instance->reuse(k, m, 0, emptyset, &used_installation_spots);
            compare_buffer[3] = (int) used_installation_spots.size();
            for (i=0; i<num_sensors; i++) {algo_results[3][i] = (isin(used_installation_spots, i)) ? 1:0;}

            // Speed-Up the forth most common case 03
            if (compare_buffer[0] == compare_buffer[3]) {continue;}

            // Min-Flood Reuse
            name_map[4] = "MIN-FLOOD REUSE";
            used_installation_spots.clear();
            instance->reuse(k, m, 1, emptyset, &used_installation_spots);
            compare_buffer[4] = (int) used_installation_spots.size();
            for (i=0; i<num_sensors; i++) {algo_results[4][i] = (isin(used_installation_spots, i)) ? 1:0;}

            // Speed-Up the second and sixth most common cases 34 and 04
            if (compare_buffer[3] == compare_buffer[4]) {continue;}
            if (compare_buffer[0] == compare_buffer[4]) {continue;}

            // Max-Flood Reuse
            name_map[5] = "MAX-FLOOD REUSE";
            used_installation_spots.clear();
            instance->reuse(k, m, -1, emptyset, &used_installation_spots);
            compare_buffer[5] = (int) used_installation_spots.size();
            for (i=0; i<num_sensors; i++) {algo_results[5][i] = (isin(used_installation_spots, i)) ? 1:0;}

            // Speed-Up the fiftheventh and eigth most common cases 45, 35 and 05
            if (compare_buffer[4] == compare_buffer[5]) {continue;}
            if (compare_buffer[3] == compare_buffer[5]) {continue;}
            if (compare_buffer[0] == compare_buffer[5]) {continue;}

            // Best-Reuse
            name_map[6] = "BEST REUSE";
            used_installation_spots.clear();
            instance->reuse(k, m, emptyset, &used_installation_spots);
            compare_buffer[6] = -1;  // (int) used_installation_spots.size();  Always ignored in comparisons
            for (i=0; i<num_sensors; i++) {algo_results[6][i] = (isin(used_installation_spots, i)) ? 1:0;}

        } catch (const std::exception &exc) {
            std::cout << "Attempt " << attempt << " (v" << attempt - invalid_count << ") Seed " << random_seed
                      << std::endl;
            throw std::runtime_error("INVALID INSTANCE!");
        }

        // Test the photogenicity
        must_break = false;
        for (i = 0; i < 7; i++) {
            if (must_break) { break; }
            for (j = i + 1; j < 7; j++) {
                if (compare_buffer[i] == compare_buffer[j]) {
                    std::cout << "Seed " << random_seed << " Case " << i << j << std::endl;
                    must_break = true;
                    break;
                }
            }
        }

        if (not must_break) {
            // If we got here, we have a photogenic instance
            std::cout << "GOT IT! " << instance->serialize() << std::endl;

            // Print the instance placements in DOT-compatible language
            Placement pl_pois[num_pois], pl_sensors[num_sensors], pl_sinks[num_sinks];
            instance->get_placements(pl_pois, pl_sensors, pl_sinks);

            std::cout << "SINK [pos=\"" << pl_sinks[0].x << "," << pl_sinks[0].y << "!\"]" << std::endl;
            for (j=0; j<num_pois; j++) {std::cout << "POI_" << j << " [pos=\"" << pl_pois[j].x << "," << pl_pois[j].y << "!\"]" << std::endl;}
            for (j=0; j<num_sensors; j++) {std::cout << "i" << j << " [pos=\"" << pl_sensors[j].x << "," << pl_sensors[j].y << "!\"]" << std::endl;}
            std::cout << std::endl;

            // Print the connections
            for (j=0; j<num_pois; j++) {
                for (i=0; i<num_sensors; i++) {
                    if (isin(instance->poi_sensor[j], i)) {std::cout << "POI_" << j << " -> i" << i << ';' << std::endl;}
                }
            }
            std::cout << std::endl;
            for (i=0; i<num_sensors; i++) {
                if (isin(instance->sink_sensor[0], i)) {std::cout << "SINK -> i" << i << ';' << std::endl;}
            }
            std::cout << std::endl;
            for (j=0; j<num_sensors; j++) {
                for (i=j; i<num_sensors; i++) {
                    if (isin(instance->sensor_sensor[j], i)) {std::cout << "i" << j << " -> i" << i << ';' << std::endl;}
                }
            }
            std::cout << std::endl;

            // Print the algorithm results
            for (j=0; j<7; j++) {
                std::cout << "ALGO " << name_map[j];
                for (i=0; i<num_sensors; i++) {
                    if (algo_results[j][i] == 1) {
                        std::cout << "; i" << i;
                    }
                }
                std::cout << ";" << std::endl;
            }

            // Print the level-graph
            // instance->level_graph(level_graph, emptyset);
            // for (i=0; i<num_sensors; i++) {std::cout << "SENSOR " << i << " LEVEL " << level_graph[i] << std::endl;}

            return (0);
        }
    }

    std::cout << "FAILURE AT " << MAX_TRIES << " TRIES!" << std::endl;
    return (1);
}
