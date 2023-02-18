/*
 * KCMC Instance Regenerator
 * Outputs the full KCMC instance from a short input. Operates like an UNIX filter
 */


// STDLib Dependencies
#include <iostream>  // cin, cout, endl

// Dependencies from this package
#include "kcmc_instance.h"


/* #####################################################################################################################
 * RUNTIME
 * */


void help() {
    std::cout << "Please, use the correct input for the KCMC instance regenerator:" << std::endl << std::endl;
    std::cout << "./instance_regenerator <instance> [<kcmc_k> <kcmc_m> [<i+>]]" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "<instance> is the short form of the serialized KCMC instance" << std::endl;
    std::cout << "<kcmc_k> (optional) is the KCMC K to validate the coverage in the instance. Must be given together with the KCMC M" << std::endl;
    std::cout << "<kcmc_m> (optional) is the KCMC M to validate the connectivity in the instance. Must be given together with the KCMC K" << std::endl;
    std::cout << "<i+> (optional) If not given, ignored. If given it must be a sequence of 1 or more sensors that are active in the instance. All sensors not listed are considered inactive. Can only be used with KCMC K and M" << std::endl;
    exit(0);
}


int main(int argc, char* const argv[]) {
    if ((argc == 0) or (argc == 1) or (argc == 3)) { help(); }

    // If we have 4 or more arguments, revalidate the instance possibly using a limited set of active sensors
    if (argc >= 4) {

        // Get the KCMC constants
        int k, m;
        k = atoi(argv[2]);
        m = atoi(argv[3]);

        // Get the set of active sensors
        std::unordered_set<int> active_sensors;
        for (int i = 4; i<argc; i++) {active_sensors.insert(atoi(argv[i]));}

        // De-serialize the instance (it might be the short or long form)
        auto *instance = new KCMC_Instance(argv[1], active_sensors);

        // Validate the instance
        if (not validate_kcmc_instance(instance, k, m, active_sensors)) {
            throw std::runtime_error("INVALID INSTANCE!");
        }

        // Print the instance as LaTeX TIKZ, a sign of success
        print_tikz(instance, 10);

    // If we have only two arguments
    } else {

        // De-serialize the instance (it might be the short or long form)
        auto *instance = new KCMC_Instance(argv[1]);

        // Print the long form of the instance
        printf("%s\n", instance->serialize().c_str());
    }

    return 0;
}

