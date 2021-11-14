/** KCMC_INSTANCE_HANDLING.cpp
 * Implementation of non-payload services of the KCMC instance object headers
 * Jose F. R. Fonseca
 */


// STDLib dependencies
#include <sstream> // ostringstream
#include <random>  // mt19937, uniform_real_distribution

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/* ISIN (IS IN)
 * A method to determine if a given value is in a given set of values, overloaded for maximum re-usability
 */
bool KCMC_Instance::isin(const std::unordered_map<int, std::unordered_set<int>> &ref, const int item){return ref.find(item) != ref.end();}
bool KCMC_Instance::isin(const std::unordered_set<int> &ref, const int item){return ref.find(item) != ref.end();}
bool KCMC_Instance::isin(const std::unordered_set<std::string> ref, const std::string &item){return ref.find(item) != ref.end();}  // CANNOT BE A POINTER TO THE SET!


/* SET DIFF
 * Returns the set that is the difference between the given sets
 */
std::unordered_set<int> KCMC_Instance::set_diff(const std::unordered_set<int> &left, const std::unordered_set<int> &right) {
    auto rightend = right.end();
    std::unordered_set<int> remainder;
    for (const int &item : left) {
        if (right.find(item) == rightend) { // If item NOT IN right side
            remainder.insert(item);
        }
    }
    return remainder;
}


/** K-Coverage Validator
 * Very trivial k-coverage validator
 */
std::string KCMC_Instance::k_coverage(const int k, std::unordered_set<int> &inactive_sensors) {
    // Base case
    if (k < 1){return "SUCCESS";}

    // Start buffers
    unsigned long active_coverage;
    std::ostringstream out;

    // For each POI, count its coverage, returning and error if insufficient
    for (int n_poi=0; n_poi < this->num_pois; n_poi++) {
        active_coverage = set_diff(poi_sensor[n_poi], inactive_sensors).size();
        if (active_coverage < k) {
            out << "POI " << n_poi << " COVERAGE " << active_coverage;
            return out.str();
        }
    }

    // Success in each and every POI!
    return "SUCCESS";
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
void KCMC_Instance::push(std::unordered_map<int, std::unordered_set<int>> &buffer, const int source, const int target){
    if (isin(buffer, source)){buffer[source].insert(target);}
    else {
        std::unordered_set<int> new_set;
        new_set.insert(target);
        buffer[source] = new_set;
    }
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

    // Iteration buffers
    int i, j, source, target;

    // Copy the variables
    this->num_pois = num_pois;
    this->num_sensors = num_sensors;
    this->num_sinks = num_sinks;
    this->area_side = area_side;
    this->sensor_coverage_radius = coverage_radius;
    this->sensor_communication_radius = communication_radius;
    this->random_seed = random_seed;

    // Prepare the placement buffers. The scope of these buffers is only the constructor itself
    Placement pl_pois[num_pois], pl_sensors[num_sensors], pl_sinks[num_sinks];

    // Prepare the random number generators
    std::mt19937 gen(random_seed);
    std::uniform_real_distribution<> point(0, area_side);

    // Set the POIs buffers
    for (i=0; i<num_pois; i++) {
        Node a_poi = {tPOI, i, -1};
        this->poi.push_back(a_poi);
        pl_pois[i] = {&a_poi, (int)(point(gen)), (int)(point(gen))};
    }

    // Set the SENSORs buffers
    for (i=0; i<num_sensors; i++) {
        Node a_sensor = {tSENSOR, i, -1};
        this->sensor.push_back(a_sensor);
        pl_sensors[i] = {&a_sensor, (int)(point(gen)), (int)(point(gen))};
    }

    // Set the SINKs buffers (if there is a single sink, it will be at the center of the area)
    if (num_sinks == 1) {
        Node a_sink = {tSINK, 0, 0};
        this->sink.push_back(a_sink);
        pl_sinks[0] = {&a_sink, (int)(area_side / 2.0), (int)(area_side / 2.0)};
    } else {
        for (i=0; i<num_sinks; i++) {
            Node a_sink = {tSINK, i, 0};
            this->sink.push_back(a_sink);
            pl_sinks[i] = {&a_sink, (int)(point(gen)), (int)(point(gen))};
        }
    }

    // Iterate each sensor and find its connections
    for (i=0; i<num_sensors; i++) {

        // Iterate each POI, identifying sensor-poi coverage
        for (j=0; j < this->num_pois; j++) {
            if (distance(pl_sensors[i], pl_pois[j]) <= coverage_radius) {
                push(this->poi_sensor, j, i);
            }
        }

        // Verify if the sensor can connect to a SINK
        for (j=0; j<this->num_sinks; j++) {
            if (distance(pl_sensors[i], pl_sinks[j]) <= communication_radius) {
                push(this->sensor_sink, i, j);  // Symetric communication between sink and sensors
                push(this->sink_sensor, j, i);  // Symetric communication between sink and sensors
            }
        }

        // Iterate each further sensor, identifying connections between sensors
        for (j=i+1; j < this->num_sensors; j++) {
            if (distance(pl_sensors[i], pl_sensors[j]) <= communication_radius) {
                push(this->sensor_sensor, i, j);  // Symetric communication between sensors
                push(this->sensor_sensor, j, i);  // Symetric communication between sensors
            }
        }
    }
    // From here on, the placement buffers are no longer needed
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
    int stage = 0;
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
                stage = this->parse_edge(stage, token);
                break;
            case 5:
                // POI-SENSOR (PS) STAGE
                stage = this->parse_edge(stage, token);
                break;
            case 6:
                // SENSOR-SENSOR (SS) STAGE
                stage = this->parse_edge(stage, token);
                break;
            case 7:
                // SENSOR-SINK (SK) STAGE
                stage = this->parse_edge(stage, token);
                break;
            case 8:
                // END-STAGE
                break;
            default: throw std::runtime_error("FORBIDDEN STAGE!");
        }
        previous = pos+1;
    }
}


int KCMC_Instance::parse_edge(const int stage, const std::string& token){
    /* Instance de-serializer helper method. Parses a single edge */

    // Parse the stage itself
    if (isin({"PS", "SS", "SK", "END"}, token)){
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


/** FUNCTIONAL CLASS SERVICES
 */


std::string KCMC_Instance::key() const{
    /* Returns the settings KEY of the instance */
    std::ostringstream out;
    out << num_pois  <<' '<< num_sensors            <<' '<< num_sinks                   << ';';
    out << area_side <<' '<< sensor_coverage_radius <<' '<< sensor_communication_radius << ';';
    out << random_seed;
    return out.str();
}


std::string KCMC_Instance::serialize(){
    /* Serializes an instance as an string */
    int source, target;

    std::ostringstream out;
    out << "KCMC;" << this->key() << ';';

    // Set the poi-sensor connections
    out << "PS;";
    for (source=0; source<num_pois; source++) {
        for (target=0; target<num_sensors; target++) {
            if (isin(this->poi_sensor[source], target)) {
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
