/*
 * KCMC Instance evaluator
 * Evaluates instances for the KCMC problem. Operates like an UNIX filter
 */


// STDLib Dependencies
#include <iostream>  // cin, cout, endl

// Dependencies from this package
#include "kcmc_instance.h"


/* #####################################################################################################################
 * RUNTIME
 * */


void help() {
    std::cout << "Please, use the correct input for the KCMC instance evaluator:" << std::endl << std::endl;
    std::cout << "./instance_evaluator <k> <m> <instance> <inactive+>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "K > 0 is the evaluated K coverage. If K <=0, the instance will not be evaluated but regenerated from its key, and M is ignored." << std::endl;
    std::cout << "M >= K is the evaluated M connectivity. Ignored if K <= 0" << std::endl;
    std::cout << "<instance> is the serialized KCMC instance" << std::endl;
    std::cout << "<inactive+> is the set of 0+ inactive sensors, as integers. Ignored if K <= 0" << std::endl;
    exit(0);
}

int main(int argc, char* const argv[]) {
    if (argc < 3) { help(); }

    // Buffers
    int k, m;
    std::unordered_set<int> inactive_sensors;
    std::string serialized_instance, k_cov, m_conn;

    /* Parse CMD SETTINGS */
    k = atoi(argv[1]);
    m = atoi(argv[2]);
    serialized_instance = argv[3];

    // Parse the inactive sensors
    if (argc > 3) {for (int i=4; i<argc; i++){inactive_sensors.insert(atoi(argv[i]));}}

    // De-serialize the instance
    auto *instance = new KCMC_Instance(serialized_instance);

    // If K <= 0, just print the instance and return
    if (k <= 0) {
        printf("%s\n", instance->serialize().c_str());
        return 0;
    }

    // Evaluate the instance, printinf the output
    k_cov = instance->k_coverage(k, inactive_sensors);
    m_conn = instance->m_connectivity(m, inactive_sensors);
    printf("K-COV: %s\t|\tM-CON: %s", k_cov.c_str(), m_conn.c_str());

    return 0;
}

