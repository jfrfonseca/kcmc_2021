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


void setify(std::unordered_set<int> &target, int size, int source[], int reference) {
    target.clear();
    for (int i=0; i<size; i++) {if (source[i] == reference) {target.insert(i);}}
}

/* #####################################################################################################################
 * KCMC-PROBLEM PAYLOAD METHODS
 */


/** Coverage getter
 * Gets the coverage at each POI, and the number of POIs with any coverage at all
 */
int KCMC_Instance::get_coverage(int buffer[], std::unordered_set<int> &inactive_sensors) {

    // For each POI, count its coverage and if it has coverage at all
    int has_coverage = 0;
    for (int n_poi=0; n_poi < this->num_pois; n_poi++) {
        buffer[n_poi] = (int)(set_diff(poi_sensor[n_poi], inactive_sensors).size());
        has_coverage += buffer[n_poi] > 0 ? 1 : 0;
    }

    // Return the number of POIs that have any coverage at all
    return has_coverage;
}


/** Degree Getter
 * Gets the degree of each active sensor, and the number of sensors with degre larger than 0
 */
int KCMC_Instance::get_degree(int buffer[], std::unordered_set<int> &inactive_sensors) {

    // For each Sensor, count its coverage, returning the number of sensors with any conection at all
    int has_connection = 0;
    for (int n_sensor=0; n_sensor < this->num_sensors; n_sensor++) {
        buffer[n_sensor] = (int)(set_diff(sensor_sensor[n_sensor], inactive_sensors).size());
        has_connection += 1;
    }

    // Return the number of Sensors that have any degree at all
    return has_connection;
}


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
        active_coverage = set_diff(poi_sensor[n_poi], inactive_sensors).size();
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
        buffer_set = set_diff(poi_sensor[n_poi], inactive_sensors);
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


/* #####################################################################################################################
 * INSTANCE UTILITY STATIC METHODS
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
              + pow((source.y - target.x), 2.0));
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


/* #####################################################################################################################
 * INSTANCE OPERATION
 */


/** RANDOM-INSTANCE (RE)GENERATOR
 * Generates the instance's placements and edges, assuming the instance already have all main attributes
 */
void KCMC_Instance::regenerate() {
    /** Random-instance (re)generator
     * This constructor is used only to generate a new random instance that already has the seed attributes
     */

    // Iteration buffers
    int i, j;

    // Prepare the placement buffers. The scope of these buffers is only the constructor itself
    Placement pl_pois[this->num_pois], pl_sensors[this->num_sensors], pl_sinks[this->num_sinks];

    // Prepare the random number generators
    std::mt19937 gen(this->random_seed);
    std::uniform_real_distribution<> point(0, this->area_side);

    // Set the POIs buffers
    for (i=0; i<this->num_pois; i++) {
        Node a_poi = {tPOI, i};
        this->poi.push_back(a_poi);
        pl_pois[i] = {&a_poi, (int)(point(gen)), (int)(point(gen))};
    }

    // Set the SENSORs buffers
    for (i=0; i<this->num_sensors; i++) {
        Node a_sensor = {tSENSOR, i};
        this->sensor.push_back(a_sensor);
        pl_sensors[i] = {&a_sensor, (int)(point(gen)), (int)(point(gen))};
    }

    // Set the SINKs buffers (if there is a single sink, it will be at the center of the area)
    if (this->num_sinks == 1) {
        Node a_sink = {tSINK, 0};
        this->sink.push_back(a_sink);
        pl_sinks[0] = {&a_sink, (int)(this->area_side / 2.0), (int)(this->area_side / 2.0)};
    } else {
        for (i=0; i<this->num_sinks; i++) {
            Node a_sink = {tSINK, i};
            this->sink.push_back(a_sink);
            pl_sinks[i] = {&a_sink, (int)(point(gen)), (int)(point(gen))};
        }
    }

    // Iterate each sensor and find its connections
    for (i=0; i<this->num_sensors; i++) {

        // Iterate each POI, identifying sensor-poi coverage
        for (j=0; j < this->num_pois; j++) {
            if (distance(pl_sensors[i], pl_pois[j]) <= this->sensor_coverage_radius) {
                push(this->poi_sensor, j, i);
            }
        }

        // Verify if the sensor can connect to a SINK
        for (j=0; j<this->num_sinks; j++) {
            if (distance(pl_sensors[i], pl_sinks[j]) <= this->sensor_communication_radius) {
                push(this->sensor_sink, i, j);  // Symetric communication between sink and sensors
                push(this->sink_sensor, j, i);  // Symetric communication between sink and sensors
            }
        }

        // Iterate each further sensor, identifying connections between sensors
        for (j=i+1; j < this->num_sensors; j++) {
            if (distance(pl_sensors[i], pl_sensors[j]) <= this->sensor_communication_radius) {
                push(this->sensor_sensor, i, j);  // Symetric communication between sensors
                push(this->sensor_sensor, j, i);  // Symetric communication between sensors
            }
        }
    }
    // From here on, the placement buffers are no longer needed
}


/** RANDOM-INSTANCE GENERATOR CONSTRUCTOR
 * Constructor of a random KCMC instance object
 */
KCMC_Instance::KCMC_Instance(int num_pois, int num_sensors, int num_sinks,
                             int area_side, int coverage_radius, int communication_radius,
                             long long random_seed) {
    /** Random-instance generator constructor
     * This constructor is used only to generate a new random instance
     */

    // Copy the variables
    this->num_pois = num_pois;
    this->num_sensors = num_sensors;
    this->num_sinks = num_sinks;
    this->area_side = area_side;
    this->sensor_coverage_radius = coverage_radius;
    this->sensor_communication_radius = communication_radius;
    this->random_seed = random_seed;

    // Now that we have the main attributes, we can (re)generate the instance
    this->regenerate();
}


/** INSTANCE DE-SERIALIZER CONSTRUCTOR
 * Constructor of a KCMC instance object from a serialized string
 */
KCMC_Instance::KCMC_Instance(const std::string& serialized_kcmc_instance) {
    /** Instance de-serializer constructor
     * This constructor is used to load a previously-generated instance. Node placements are irrelevant
     */

    // Iterate the string, looking for tokens
    size_t previous = 0, pos = 0;
    std::string token;
    int stage = 0, has_edges = 0;
    while ((pos = serialized_kcmc_instance.find(';', previous)) != std::string::npos) {
        token = serialized_kcmc_instance.substr(previous, pos-previous);
        std::stringstream s_token(token);

        switch(stage) {
            case 0:
                // VALIDATE PREFIX
                if ("KCMC" != token) {throw std::runtime_error("INSTANCE DOES NOT STARTS WITH PREFIX 'KCMC'");}
                stage = 1;
                break;
            case 1:
                s_token >> this->num_pois;
                s_token >> this->num_sensors;
                s_token >> this->num_sinks;
                stage = 2;
                break;
            case 2:
                s_token >> this->area_side;
                s_token >> this->sensor_coverage_radius;
                s_token >> this->sensor_communication_radius;
                stage = 3;
                break;
            case 3:
                s_token >> this->random_seed;
                stage = 4;
                break;
            case 4:
                // FIRST-STAGE PARSER
                has_edges = 1;
                stage = this->parse_edge(stage, token);
                break;
            case 5:
                // POI-SENSOR (PS) STAGE
                has_edges = 1;
                stage = this->parse_edge(stage, token);
                break;
            case 6:
                // SENSOR-SENSOR (SS) STAGE
                has_edges = 1;
                stage = this->parse_edge(stage, token);
                break;
            case 7:
                // SENSOR-SINK (SK) STAGE
                has_edges = 1;
                stage = this->parse_edge(stage, token);
                break;
            case 8:
                // END-STAGE
                break;
            default: throw std::runtime_error("FORBIDDEN STAGE!");
        }
        previous = pos+1;
    }
    if (this->num_pois == 0) {throw std::runtime_error("INSTANCE HAS NO POIS!");}
    if (this->num_sensors == 0) {throw std::runtime_error("INSTANCE HAS NO SENSORS!");}
    if (this->num_sinks == 0) {throw std::runtime_error("INSTANCE HAS NO SINKS!");}

    // If we got here and have no edges, we must re-generate this instance
    if (has_edges == 0) { this->regenerate(); }
}


/** Utility method to the De-Serializer Constructor
 */
int KCMC_Instance::parse_edge(const int stage, const std::string& token){
    /* Instance de-serializer helper method. Parses a single edge */

    // Parse the stage itself
    std::unordered_set<std::string> tags = {"PS", "SS", "SK", "END"};
    if (isin(tags, token)){
        if      (token == "PS"){return 5;}
        else if (token == "SS"){return 6;}
        else if (token == "SK"){return 7;}
        else if (token == "END"){return 8;}
    } else if (stage == 4) {throw std::runtime_error("UNKNOWN TOKEN!");}

    // Parsing at the current stage
    std::stringstream s_token(token);
    int source, target;
    s_token >> source;
    s_token >> target;
    switch (stage) {
        case 5:
            push(this->poi_sensor, source, target);
            return 5;
        case 6:
            push(this->sensor_sensor, source, target);
            push(this->sensor_sensor, target, source);
            return 6;
        case 7:
            push(this->sensor_sink, source, target);
            push(this->sink_sensor, target, source);
            return 7;
        case 8: return 8;
        default: throw std::runtime_error("FORBIDDEN STAGE!");
    }
}


/* #####################################################################################################################
 * FUNCTIONAL CLASS SERVICES
 */


/** Instance identification utility
 */
std::string KCMC_Instance::key() const {
    /* Returns the settings KEY of the instance */
    std::ostringstream out;
    out << num_pois  <<' '<< num_sensors            <<' '<< num_sinks                   << ';';
    out << area_side <<' '<< sensor_coverage_radius <<' '<< sensor_communication_radius << ';';
    out << random_seed;
    return out.str();
}


/** Instance serializer
 */
std::string KCMC_Instance::serialize() {
    /* Serializes an instance as an string */
    int source, target;

    std::ostringstream out;
    out << "KCMC;" << this->key() << ';';

    // Set the poi-sensor connections
    out << "PS;";
    for (source=0; source<num_pois; source++) {
        for (target=0; target<num_sensors; target++) {
            if (isin(poi_sensor[source], target)) {
                out << source << ' ' << target << ';';  // MUCH SLOWER than iterating the hashmap, but deterministic
            }
        }
    }

    // Set the sensor-sensor connections
    out << "SS;";
    for (source=0; source<num_sensors; source++) {
        for (target=source; target<num_sensors; target++) {
            if (isin(this->sensor_sensor[source], target)) {
                out << source << ' ' << target << ';';  // MUCH SLOWER than iterating the hashmap, but deterministic
            }
        }
    }

    // Set the sensor-sink connections
    out << "SK;";
    for (source=0; source<num_sensors; source++) {
        for (target=0; target<num_sinks; target++) {
            if (isin(this->sensor_sink[source], target)) {
                out << source << ' ' << target << ';';  // MUCH SLOWER than iterating the hashmap, but deterministic
            }
        }
    }

    // Return the out string
    out << "END";
    return out.str();
}

int KCMC_Instance::invert_set(std::unordered_set<int> &source_set, std::unordered_set<int> *target_set) {
    target_set->clear();
    for (int i=0; i<num_sensors; i++) {
        if (not isin(source_set, i)) {
            target_set->insert(i);
        }
    }
    return target_set->size();
}


bool KCMC_Instance::validate(const bool raise, const int k, const int m,
                             std::unordered_set<int> &inactive_sensors,
                             std::unordered_set<int> *k_used_sensors,
                             std::unordered_set<int> *m_used_sensors) {

    // Check validity, recovering the used sensors for K coverage and M connectivity
    int valid = this->fast_k_coverage(k, inactive_sensors, k_used_sensors);
    if (valid != -1) {
        if (raise) { throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT COVERAGE)"); }
        else { return false; }
    }
    valid = this->fast_m_connectivity(m, inactive_sensors, m_used_sensors);
    if (valid != -1) {
        if (raise) { throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT CONNECTIVITY)"); }
        else { return false; }
    }
    return true;
}

bool KCMC_Instance::validate(const bool raise, const int k, const int m,
                             std::unordered_set<int> &inactive_sensors) {
    // Prepare the ignored results buffer and the empty set of inactive sensors
    std::unordered_set<int> ignored;
    return this->validate(raise, k, m, inactive_sensors, &ignored, &ignored);
}


bool KCMC_Instance::validate(const bool raise, const int k, const int m) {
    // Prepare the ignored results buffer and the empty set of inactive sensors
    std::unordered_set<int> emptyset;
    return this->validate(raise, k, m, emptyset);
}
