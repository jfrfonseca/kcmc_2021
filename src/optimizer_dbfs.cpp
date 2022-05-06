

// STDLib dependencies
#include <csignal>    // SIGINT and other signals
#include <sstream>    // ostringstream
#include <queue>      // queue
#include <iostream>   // cin, cout, endl
#include <chrono>     // time functions
#include <iomanip>    // setfill, setw
#include <cstring>    // strcpy

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers
#include "genetic_algorithm_operators.h"  // exit_signal_handler


/** DIRECTED BREADTH-FIRST SEARCH ALGORITM
 * Finds all active sensors that have lower level than the given seed set, and can be reached/visited from the seed set
 */
int KCMC_Instance::directed_bfs(std::unordered_set<int> &seed_sensors,
                                std::unordered_set<int> &inactive_sensors,
                                std::unordered_set<int> *visited_sensors) {

    // Local buffers
    int i_sensor, level_graph[this->num_sensors], pushes = (int)seed_sensors.size();
    std::queue<int> queue;

    // Clear the set of visited sensors
    visited_sensors->clear();

    // Prepare a queue with each given active seed sensor
    for (const int &a_sensor : seed_sensors) {if (not isin(inactive_sensors, a_sensor)) { queue.push(a_sensor); }}

    // Process the level graph for the current instance
    this->level_graph(level_graph, inactive_sensors);

    // Iterate until the queue is empty
    while (not queue.empty()) {

        // Pop the sensor from the front of the queue and visit it
        i_sensor = queue.front();
        queue.pop();
        visited_sensors->insert(i_sensor);

        // For each non-visited active neighbor of the current i_sensor that has a non-larger level, add it to the queue
        for (const int &neighbor : this->sensor_sensor[i_sensor]) {
            if (    (not isin(inactive_sensors, neighbor))
                and (not isin(*visited_sensors, neighbor))
                and (level_graph[neighbor] <= level_graph[i_sensor])
            ) {
                queue.push(neighbor);
                pushes++;
            }
        }
    }

    // Return the size of the visited sensors set
    return pushes;
}


/* #####################################################################################################################
 * RUNTIME
 * */


void printout_short(const std::string key, int k, int m,
                    const int num_sensors, const std::string operation,
                    const long duration, std::unordered_set<int> &used_installation_spots) {

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
    out << key << "\t" << k << "\t" << m
        << "\t" << operation
        << "\t" << duration
        << "\t" << used_installation_spots.size()
        << "\t" << std::fixed << std::setprecision(5) << (double)(num_sensors - used_installation_spots.size()) / (double)num_sensors
        << "\t";
    for (int i=0; i<num_sensors; i++) {out << individual[i];}
    // Flush
    std::cout << out.str() << std::endl;
}


void help() {
    std::cout << "Please, use the correct input for the KCMC instance optimizer, DBFS version:" << std::endl << std::endl;
    std::cout << "./optimizer_dbfs <instance> <k> <m>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "<instance> is the serialized KCMC instance" << std::endl;
    std::cout << "Integer 0 < K < 10 is the desired K coverage" << std::endl;
    std::cout << "Integer 0 < M < 10 is the desired M connectivity" << std::endl;
    std::cout << "K migth be the pair K,M in the format (K{k}M{m}). In this case M is optional" << std::endl;
    exit(0);
}


int main(int argc, char* const argv[]) {
    if (argc < 2) { help(); }

    // Registers the signal handlers
    signal(SIGINT, exit_signal_handler);
    signal(SIGALRM, exit_signal_handler);
    signal(SIGABRT, exit_signal_handler);
    signal(SIGSTOP, exit_signal_handler);
    signal(SIGTERM, exit_signal_handler);
    signal(SIGKILL, exit_signal_handler);

    // Buffers
    int k, m, poi;
    std::string serialized_instance, alt_k;
    std::unordered_set<int> emptyset, used_installation_spots, seed_sensors;

    /* Parse base Arguments
     * Serialized KCMC Instance (will be immediately de-serialized
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

    // Print the header
    // printf("Key\tK\tM\tOperation\tRuntime\tObjective\tQuality\tSolution\n");

    // Validate the whole instance, getting the first local optima
    auto start = std::chrono::high_resolution_clock::now();
    instance->local_optima(k, m, emptyset, &used_installation_spots);
    auto end = std::chrono::high_resolution_clock::now();
    long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout_short(instance->key(), k, m, instance->num_sensors, "local_optima", duration, used_installation_spots);
    used_installation_spots.clear();

    // Process the Directed Breadth-First Search as a local optima, using the poi-covering sensors as seed
    for (poi=0; poi < instance->num_pois; poi++) {
        for (const int &sensor : instance->poi_sensor[poi]) {
            seed_sensors.insert(sensor);
        }
    }
    start = std::chrono::high_resolution_clock::now();
    instance->directed_bfs(seed_sensors, emptyset, &used_installation_spots);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout_short(instance->key(), k, m, instance->num_sensors, "directed_bfs", duration, used_installation_spots);

    return 0;
}
