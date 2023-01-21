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
    std::cout << "./instance_regenerator <instance>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "<instance> is the short form of the serialized KCMC instance" << std::endl;
    exit(0);
}

int main(int argc, char* const argv[]) {
    if (argc != 2) { help(); }

    // De-serialize the instance (it might be the short or long form)
    auto *instance = new KCMC_Instance(argv[1]);

    // Print the long form of the instance
    printf("%s\n", instance->serialize().c_str());

    return 0;
}

