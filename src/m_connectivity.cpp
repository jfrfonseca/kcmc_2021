

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** M-CONNECTIVITY VALIDATOR USING DINIC'S ALGORITHM
 */


std::string KCMC_Instance::m_connectivity(const int m, std::unordered_set<int> &inactive_sensors) {
    /** Verify if every POI has at least M different disjoint paths to all SINKs
     */
    // Base case
    if (m < 1){return "SUCCESS";}

    //TODO

    // Success in each and every POI!
    return "SUCCESS";
}
