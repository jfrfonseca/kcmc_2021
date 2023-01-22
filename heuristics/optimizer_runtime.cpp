

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


void printout(KCMC_Instance *instance, int k, int m,
              const std::string optimization_method,
              const long duration, std::unordered_set<int> &solution) {

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
        << duration << "\t"
        << solution.size() << "\t"
        << std::setprecision(6) << solution.size() / instance->num_sensors << "\t";

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
    int k, m;
    k = atoi(argv[2]);
    m = atoi(argv[3]);

    // Process the KCOV-DINIC optimization
    start = std::chrono::high_resolution_clock::now();
    instance->kcov_dinic(k, m, solution);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printout(instance, k, m, "KCOV-DINIC", duration, solution);

}
