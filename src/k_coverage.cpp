/** KCMC_INSTANCE_HANDLING.cpp
 * Implementation of non-payload services of the KCMC instance object headers
 * Jose F. R. Fonseca
 */


// STDLib dependencies
#include <sstream>    // ostringstream
#include <random>     // mt19937, uniform_real_distribution
#include <algorithm>  // std::find

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/* #####################################################################################################################
 * UTILITY FUNCTIONS
 */


/* ISIN (IS IN)
 * A method to determine if a given value is in a given set of values, overloaded for maximum re-usability
 */
bool isin(std::unordered_map<int, std::unordered_set<int>> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_map<int, int> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_set<int> &ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_set<std::string> &ref, const std::string &item){return ref.find(item) != ref.end();}  // CANNOT BE A POINTER TO THE SET!
bool isin(std::vector<int> &ref, const int item){return std::find(ref.begin(), ref.end(), item) != ref.end();}
bool isin(std::vector<int> *ref, const int item){return std::find(ref->begin(), ref->end(), item) != ref->end();}


/* SET DIFF
 * Returns the set that is the difference between the given sets
 */
std::unordered_set<int> set_diff(const std::unordered_set<int> &left, const std::unordered_set<int> &right) {
    auto rightend = right.end();
    std::unordered_set<int> remainder;
    for (const int &item : left) {
        if (right.find(item) == rightend) { // If item NOT IN right side
            remainder.insert(item);
        }
    }
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


/* VOTE
 * Adds a vote to an element in an unordered map. Adds the element to the map if not there
 */
void vote(std::unordered_map<int, int> &buffer, const int target, const int value){
    if (isin(buffer, target)){buffer[target] = buffer[target] + value;}
    else {buffer[target] = value;}
}
void vote(std::unordered_map<int, int> &buffer, const int target) {vote(buffer, target, 1);}


void setify(std::unordered_set<int> &target, int size, int source[], int reference) {
    target.clear();
    for (int i=0; i<size; i++) {if (source[i] == reference) {target.insert(i);}}
}


void setify(std::unordered_set<int> &target, std::unordered_map<int, int> *reference) {
    target.clear();
    for (const auto &i : *reference) {target.insert(i.first);}
}


/* #####################################################################################################################
 * KCMC-PROBLEM K-COVERAGE METHODS
 */


/** K-Coverage Validator
 * Very trivial k-coverage validator
 */
int KCMC_Instance::fast_k_coverage(const int k, std::unordered_set<int> &inactive_sensors) {
    // Base case
    if (k < 1){return -1;}

    // Start buffers
    unsigned long active_coverage;

    // For each POI, count its coverage, returning and error if insufficient
    for (int n_poi=0; n_poi < this->num_pois; n_poi++) {
        active_coverage = set_diff(this->poi_sensor[n_poi], inactive_sensors).size();
        if (active_coverage < k) {
            return (n_poi*1000000)+(int)(active_coverage);
        }
    }

    // Success in each and every POI!
    return -1;
}


/** K-Coverage Validator that also returns the used sensors in k coverage
 * Very trivial k-coverage validator
 */
int KCMC_Instance::fast_k_coverage(const int k, std::unordered_set<int> &inactive_sensors, std::unordered_set<int> *result_buffer) {
    // Clear the set of active sensors
    result_buffer->clear();

    // Base case
    if (k < 1){return -1;}

    // Start buffers
    std::unordered_set<int> buffer_set, all_used_sensors;
    unsigned long active_coverage;

    // For each POI, count its coverage, returning and error if insufficient. Also note all used sensors
    for (int n_poi=0; n_poi < this->num_pois; n_poi++) {
        buffer_set = set_diff(this->poi_sensor[n_poi], inactive_sensors);
        active_coverage = buffer_set.size();
        all_used_sensors = set_merge(all_used_sensors, buffer_set);
        *result_buffer = all_used_sensors;
        if (active_coverage < k) {
            return (n_poi*1000000)+(int)(active_coverage);
        }
    }

    // Success in each and every POI!
    return -1;
}


/** K-COVERAGE VALIDATOR
 * Wrapper around the fastest validator, to allow for better process message passing.
 */
std::string KCMC_Instance::k_coverage(const int k, std::unordered_set<int> &inactive_sensors) {
    int failure_at = this->fast_k_coverage(k, inactive_sensors);
    if (failure_at == -1) {return "SUCCESS";}
    else {
        std::ostringstream out;
        int n_poi = failure_at / 1000000, active_coverage = failure_at % 1000000;  // Decode the result
        out << "POI " << n_poi << " COVERAGE " << active_coverage;
        return out.str();
    }
}
