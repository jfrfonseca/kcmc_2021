/** KCMC_INSTANCE_HANDLING.cpp
 * Implementation of non-payload services of the KCMC instance object headers
 * Jose F. R. Fonseca
 */


// STDLib dependencies
#include <random>     // mt19937, uniform_real_distribution
#include <algorithm>  // std::find, std::set_difference

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** POI coverage retrieval
 * Find the set of active sensors that cover a POI
 * @param num_poi (int) the ID of the poi
 * @param set_inactive_sensors (std::set<int>) a set of sensors not to be considered while measuring coverage
 * @param active_covering_sensors (std::set<int>) the buffer for the results
 */
void KCMC_Instance::poi_coverage(int num_poi, std::set<int> &set_inactive_sensors, std::set<int> &active_covering_sensors) {
    // Uses the ORDERED set difference for greater implementation speed but similar complexity to the UNORDERED
    active_covering_sensors.clear();
    std::set_difference(
        this->set_poi_sensor[num_poi].begin(), this->set_poi_sensor[num_poi].end(),
        set_inactive_sensors.begin(), set_inactive_sensors.end(),
        std::inserter(active_covering_sensors, active_covering_sensors.begin())
    );
}


/** K-Coverage verification method
 * Returns the number of POIs that each have at least K-Coverage
 * @param k (int) the desired coverage
 * @param inactive_sensors (std::unordered_set<int>) a set of sensors not to be considered while measuring coverage
 * @param quiet if the method should print to STDERR the identity of the insufficiently covered sensors
 * @return
 */
int KCMC_Instance::has_coverage(const int k, std::unordered_set<int> &inactive_sensors, const bool quiet) {

    // Prepare buffers
    int total_coverage=0;
    std::set<int> active_covering_sensors, set_inactive_sensors;

    // Make the UNORDERED set of inactive sensors into an ORDERED set
    std::copy(inactive_sensors.begin(), inactive_sensors.end(),
              std::inserter(set_inactive_sensors, set_inactive_sensors.end()));

    // Iterates every POI, identifying its active coverage
    for (int n_poi=0; n_poi < this->num_pois; n_poi++) {

        // Uses the ORDERED set difference for greater implementation speed but similar complexity
        this->poi_coverage(n_poi, set_inactive_sensors, active_covering_sensors);

        // If not enough active coverage, log and skip the POI
        if (active_covering_sensors.size() < k) {
            if (not quiet) {
                fprintf(stderr, "\nPOI %d has insufficient coverage - %lu/%d", n_poi, active_covering_sensors.size(), k);
            }
        // If enough coverage, count the POI
        } else {
            total_coverage += 1;
        }
    }
    return total_coverage;
}
int KCMC_Instance::has_coverage(const int k, std::unordered_set<int> &inactive_sensors) {
    return this->has_coverage(k, inactive_sensors, true);
}


/* #####################################################################################################################
 * UTILITY FUNCTIONS
 */


/* ISIN (IS IN)
 * A method to determine if a given value is in a given set of values, overloaded for maximum re-usability
 */
bool isin(std::unordered_map<int, std::unordered_set<int>> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_map<int, std::set<int>> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_map<int, int> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_set<int> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(const std::set<int> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_set<std::string> &ref, const std::string &item){return ref.find(item) != ref.end();}  // CANNOT BE A POINTER TO THE SET!


/* SET DIFF
 * Returns the set that is the difference between the given sets
 */
std::unordered_set<int> set_diff(const std::unordered_set<int> &left, const std::unordered_set<int> &right) {

    // Prepare buffers
    std::unordered_set<int> remainder = left;

    // Remove the right from the left and return it
    for (const int &item : right) {remainder.erase(item);}
    return remainder;
}


/* DISTANCE
 * Distance between two components, overloaded for maximum re-usability
 */
double distance(Placement source, Placement target){
    /** Euclidean distance */
    return sqrt(pow((source.x - target.x), 2.0)
                 + pow((source.y - target.y), 2.0));
}


/* PUSH
 * Adds and element to an HashMap of Unordered Sets
 */
void push(std::unordered_map<int, std::unordered_set<int>> &buffer, const int source, const int target){
    if (isin(buffer, source)){buffer[source].insert(target);}
    else {
        std::unordered_set<int> new_set;
        new_set.insert(target);
        buffer[source] = new_set;
    }
}
void push(std::unordered_map<int, std::set<int>> &buffer, const int source, const int target){
    if (isin(buffer, source)){buffer[source].insert(target);}
    else {
        std::set<int> new_set;
        new_set.insert(target);
        buffer[source] = new_set;
    }
}
