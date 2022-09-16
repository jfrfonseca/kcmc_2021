

// STDLib dependencies
#include <csignal>    // SIGINT and other signals
#include <queue>      // queue
#include <iostream>   // cin, cout, endl
#include <chrono>     // time functions
#include <iomanip>    // setfill, setw
#include <cstring>    // strcpy

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers
#include "genetic_algorithm_operators.h"  // exit_signal_handler


/* #####################################################################################################################
 * RUNTIME
 * */


void printout_short(KCMC_Instance *instance, int k, int m,
                    const int num_sensors, const std::string operation,
                    const long duration, std::unordered_set<int> &used_installation_spots) {

    // Validate the instance
    std::unordered_set<int> inactive_sensors;
    instance->invert_set(used_installation_spots, &inactive_sensors);
    bool valid = instance->validate(false, k, m, inactive_sensors);

    // Reformat the used installation spots as an array of 0/1
    int individual[num_sensors];
    std::fill(individual, individual+num_sensors, 0);
    for (const int &used_spot : used_installation_spots) { individual[used_spot] = 1; }

    // Prepare the output buffer
    std::ostringstream out;

    // Print a line with:
    // - The key of the instance
    // - The name of the current operation
    // - The amount of microsseconds the method needed to run
    // - The number of used installation spots
    // - The resulting map of the instance, as a binary of num_sensors bits
    out << instance->key() << "\t" << k << "\t" << m
        << "\t" << operation
        << "\t" << duration
        << "\t" << (valid ? "OK" : "INVALID")
        << "\t" << used_installation_spots.size()
        << "\t" << std::fixed << std::setprecision(5) << (double)(inactive_sensors.size()) / (double)num_sensors
        << "\t";
    for (int i=0; i<num_sensors; i++) {out << individual[i];}
    // Flush
    std::cout << out.str() << std::endl;
}


void help() {
    std::cout << "Please, use the correct input for the KCMC instance heuristic optimizer:" << std::endl << std::endl;
    std::cout << "./optimizer_dinic <instance> <k> <m>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "<instance> is the serialized KCMC instance" << std::endl;
    std::cout << "Integer 0 < K < 10 is the desired K coverage" << std::endl;
    std::cout << "Integer 0 < M < 10 is the desired M connectivity" << std::endl;
    std::cout << "K migth be the pair K,M in the format (K{k}M{m}). In this case M is ignored" << std::endl;
    exit(0);
}


int main(int argc, char* const argv[]) {
    if (argc < 3) { help(); }

    // Registers the signal handlers
    signal(SIGINT, exit_signal_handler);
    signal(SIGALRM, exit_signal_handler);
    signal(SIGABRT, exit_signal_handler);
    signal(SIGSTOP, exit_signal_handler);
    signal(SIGTERM, exit_signal_handler);
    signal(SIGKILL, exit_signal_handler);

    // Buffers
    int k, m, num_paths;
    std::string serialized_instance, alt_k;
    std::unordered_set<int> emptyset, seed_sensors, set_used_installation_spots;
    std::unordered_map<int, int> used_installation_spots;

    /* Parse base Arguments
     * Serialized KCMC Instance (will be immediately de-serialized)
     * KCMC K and M parameters
     * */
    auto *instance = new KCMC_Instance(argv[1]);
    alt_k = argv[2];
    std::transform(alt_k.begin(), alt_k.end(),alt_k.begin(), ::toupper);
    char p[alt_k.size()];
    strcpy(p, alt_k.c_str());
    if (alt_k.find('K') != std::string::npos) {
        k = ((int)p[2]) - ((int)'0');  // ONLY FOR K,M < 10!!!
        m = ((int)p[4]) - ((int)'0');  // ONLY FOR K,M < 10!!!
    } else {
        k = std::stoi(argv[2]);
        m = std::stoi(argv[3]);
    }

    // Prepare the clock buffers
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    long duration;

    // Print the header
    // printf("Key\tK\tM\tOperation\tRuntime\tValid\tObjective\tCompression\tSolution\n");

    // Validate the whole instance, getting the first local optima using DINIC Algorithm
    set_used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    instance->local_optima(k, m, emptyset, &set_used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout_short(instance, k, m, instance->num_sensors,
                   "dinic",
                   duration, set_used_installation_spots);

    // Process the Minimal-Flood mapping of the instance
    used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    num_paths = instance->flood(k, m, false, emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    set_used_installation_spots.clear();
    setify(set_used_installation_spots, &used_installation_spots);
    printout_short(instance, k, m, instance->num_sensors,
                   "min_flood_" + std::to_string(num_paths),  // Add the number of paths found
                   duration, set_used_installation_spots);

    // Process the Max-Flood mapping of the instance
    used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    num_paths = instance->flood(k, m, true, emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    set_used_installation_spots.clear();
    setify(set_used_installation_spots, &used_installation_spots);
    printout_short(instance, k, m, instance->num_sensors,
                   "max_flood_" + std::to_string(num_paths),  // Add the number of paths found
                   duration, set_used_installation_spots);

    // Process the No-Flood Reuse mapping of the instance
    used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    num_paths = instance->reuse(k, m, 0,emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    set_used_installation_spots.clear();
    setify(set_used_installation_spots, &used_installation_spots);
    printout_short(instance, k, m, instance->num_sensors,
                   "no_reuse_" + std::to_string(num_paths),  // Add the number of added sensors for k-coverage
                   duration, set_used_installation_spots);

    // Process the Min-Flood Reuse mapping of the instance
    used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    num_paths = instance->reuse(k, m, 1,emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    set_used_installation_spots.clear();
    setify(set_used_installation_spots, &used_installation_spots);
    printout_short(instance, k, m, instance->num_sensors,
                   "min_reuse_" + std::to_string(num_paths),  // Add the number of added sensors for k-coverage
                   duration, set_used_installation_spots);

    // Process the Max-Flood Reuse mapping of the instance
    used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    num_paths = instance->reuse(k, m, -1,emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    set_used_installation_spots.clear();
    setify(set_used_installation_spots, &used_installation_spots);
    printout_short(instance, k, m, instance->num_sensors,
                   "max_reuse_" + std::to_string(num_paths),  // Add the number of added sensors for k-coverage
                   duration, set_used_installation_spots);

    // Process the Best-Reuse mapping of the instance
    used_installation_spots.clear();
    start = std::chrono::high_resolution_clock::now();
    num_paths = instance->reuse(k, m,emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    set_used_installation_spots.clear();
    setify(set_used_installation_spots, &used_installation_spots);
    printout_short(instance, k, m, instance->num_sensors,
                   "best_reuse_" + std::to_string(num_paths),  // Add the number of added sensors for k-coverage
                   duration, set_used_installation_spots);

    return 0;
}
