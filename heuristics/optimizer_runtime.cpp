

// STDLib dependencies
#include <iostream>   // cin, cout, endl
#include <chrono>     // time functions
#include <iomanip>    // ostringstream
#include <algorithm>  // transform

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/* #####################################################################################################################
 * RUNTIME
 * */


bool validate_kcmc_instance(KCMC_Instance *instance, int k, int m, std::unordered_set<int> active_sensors) {

    // Prepare buffers
    int coverage;
    std::unordered_set<int> buffer_set;

    // Checks if it has enough coverage. If not, return false
    coverage = instance->has_coverage(k, buffer_set);
    if (coverage < instance->num_pois) {return false;}

    // Checks if the coverage set is made EXCLUSIVELY from the active sensors
    if (not set_diff(buffer_set, active_sensors).empty()) {return false;}
    buffer_set.clear();

    // Checks if it has enough connectivity. If not, return false
    instance->dinic(m, buffer_set);
    if (buffer_set.empty()) {return false;}

    // Checks if the communication set is made EXCLUSIVELY from the active sensors
    if (not set_diff(buffer_set, active_sensors).empty()) {return false;}

    // If we got here, success!
    return true;
}


void printout(KCMC_Instance *instance, int k, int m,
              const std::string optimization_method,
              const int cost,
              const long duration,
              std::unordered_set<int> &solution) {

    // Validate the instance
    bool validity = validate_kcmc_instance(instance, k, m, solution);

    // Prepare the output buffer
    std::ostringstream out;

    // Prepare a sorted array of integers containing the solution
    int array_solution[instance->num_sensors], pos=0;
    std::fill(array_solution, array_solution+instance->num_sensors, INFTY);
    for (const int sensor : solution) {
        array_solution[pos] = sensor;
        pos++;
    }
    std::sort(array_solution, array_solution+solution.size());

    // Printout the settings of the processed solution,
    // its duration, size and percent of the original sensors that belong in the solution
    out << "KCMC;" << instance->key() << ";END\t"
        << k << "\t"
        << m << "\t"
        << optimization_method << "\t"
        << validity << "\t"
        << cost << "\t"
        << duration << "\t"
        << solution.size() << "\t"
        << int(1000.0 * (double)(solution.size()) / (double)(instance->num_sensors)) << "\t";

    // Printout the obtained solution as a JSON list of used installation spots / active sensors
    out << "["
        << array_solution[0];
    for (pos=1; pos<solution.size(); pos++) {
        out << ", " << array_solution[pos];
    }
    out << "]";

    // Flush
    std::cout << out.str() << std::endl;
}


void help() {
    std::cout << "Please, use the correct input for the KCMC instance heuristic optimizer:" << std::endl << std::endl;
    std::cout << "./optimizer_dinic <instance> <k> <m>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "<instance> is the serialized KCMC instance (long or short format)" << std::endl;
    std::cout << "Integer 0 < K < 10 is the desired K coverage" << std::endl;
    std::cout << "Integer 0 < M < 10 is the desired M connectivity" << std::endl;
    exit(0);
}


int main(int argc, char* const argv[]) {
    if (argc < 3) { help(); }

    // Prepare the solution buffer
    std::unordered_set<int> solution;

    // Prepare the clock buffers
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    long duration;

    // Parse the input
    auto *instance = new KCMC_Instance(argv[1]);
    int k, m, cost;
    k = atoi(argv[2]);
    m = atoi(argv[3]);

    // Process the KCOV-DINIC optimization
    solution.clear();
    start = std::chrono::high_resolution_clock::now();
    cost = instance->kcov_dinic(k, m, solution);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout(instance, k, m, "KCOV-DINIC", cost, duration, solution);

    // Process the REUSE-DINIC optimization
    solution.clear();
    start = std::chrono::high_resolution_clock::now();
    cost = instance->reuse_dinic(k, m, solution);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout(instance, k, m, "REUSE-DINIC", cost, duration, solution);

    // Process the FLOOD-DINIC optimization
    solution.clear();
    start = std::chrono::high_resolution_clock::now();
    cost = instance->flood_dinic(k, m, solution);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout(instance, k, m, "FLOOD-DINIC", cost, duration, solution);

    // Process the BEST-DINIC optimization
    solution.clear();
    start = std::chrono::high_resolution_clock::now();
    cost = instance->best_dinic(k, m, solution);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout(instance, k, m, "BEST-DINIC", cost, duration, solution);

}
