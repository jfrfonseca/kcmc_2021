
// STDLib dependencies
#include <cmath>   // sqrt, pow
#include <sstream> // ostringstream
#include <random>  // mt19937, uniform_real_distribution
#include <queue>   // queue
#include<cstring>  // memset

// Dependencies from this package
#include "kcmc_components.h"


/* #####################################################################################################################
 * FUNCTIONAL UTILITIES IMPLEMENTATIONS
 */


/* ISIN (IS IN)
 * A method to determine if a given value is in a given set of values, overloaded for maximum re-usability
 */
bool isin(std::unordered_map<int, std::unordered_set<int>> ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_set<int> ref, const int item){return ref.find(item) != ref.end();}
bool isin(std::unordered_set<std::string> ref, const std::string &item){return ref.find(item) != ref.end();}


/* DISTANCE
 * Distance between two components, overloaded for maximum re-usability
 */
double distance(COMPONENT source, COMPONENT target){
    /** Euclidean distance */
    return sqrt(pow(abs(abs(source.x) - abs(target.x)), 2.0)
              + pow(abs(abs(source.y) - abs(target.y)), 2.0));
}
double distance(COMPONENT source, int x, int y){
    COMPONENT target = {x, y};
    return distance(source, target);
}


/* #####################################################################################################################
 * CONSTRUCTORS
 */


KCMC_Instance::KCMC_Instance(int num_pois, int num_sensors, int num_sinks,
                             int area_side, int coverage_radius, int communication_radius,
                             long long random_seed, bool print) {

    // Copy the variables
    this->num_pois = num_pois;
    this->num_sensors = num_sensors;
    this->num_sinks = num_sinks;
    this->area_side = area_side;
    this->sensor_coverage_radius = coverage_radius;
    this->sensor_communication_radius = communication_radius;
    this->random_seed = random_seed;

    // Prepare buffers
    this->splitted = false;
    this->split_sensors.clear();
    this->sensor_poi.clear();
    this->poi_sensor.clear();
    this->sensor_sensor.clear();
    this->sensor_sink.clear();
    this->sink_sensor.clear();
    int i, j;
    COMPONENT pois[MAX_POIS], sensors[MAX_SENSORS], sinks[MAX_SINKS];

    // Prepare the random number generators
    std::mt19937 gen(random_seed);
    std::uniform_real_distribution<> point(0, area_side);

    // Set the SINKs buffer (if there is a single sink, it will be at the center of the area)
    if (num_sinks == 1) {
        sinks[0] = {(int)(area_side / 2.0), (int)(area_side / 2.0)};
    } else {
        for (i=0; i<num_sinks; i++) {
            sinks[i] = {(int)(point(gen)), (int)(point(gen))};
        }
    }

    // Set the POIs buffer
    for (i=0; i<num_pois; i++) {
        pois[i] = {(int)(point(gen)), (int)(point(gen))};
    }

    // Set the sensors buffer
    for (i=0; i<num_sensors; i++) {
        sensors[i] = {(int)(point(gen)), (int)(point(gen))};
    }

    // Iterate each sensor and add its connections to the current instance
    for (i=0; i<num_sensors; i++) {

        // Iterate each POI, identifying sensor-poi coverage
        for (j = 0; j < this->num_pois; j++) {
            if (distance(sensors[i], pois[j]) <= coverage_radius) {
                push(this->sensor_poi, i, j);
                push(this->poi_sensor, j, i);
            }
        }

        // Verify if the sensor can connect to a SINK
        for (j=0; j<this->num_sinks; j++) {
            if (distance(sensors[i], sinks[j]) <= communication_radius) {
                push(this->sensor_sink, i, j);  // Symetric communication between sink and sensors
                push(this->sink_sensor, j, i);
            }
        }

        // Iterate each further sensor, identifying connections between sensors
        for (j=i+1; j < this->num_sensors; j++) {
            if (distance(sensors[i], sensors[j]) <= communication_radius) {
                push(this->sensor_sensor, i, j);
                push(this->sensor_sensor, j, i);  // Symetric communication between sensors
            }
        }
    }

    // Print as GRAPHVIZ, if required
    if (print) {
        printf("digraph G {\n");

        printf("  subgraph pois {\n    node [style=filled];\n");
        for (std::pair<int, std::unordered_set<int>> source_poi : this->poi_sensor) {
            for (const auto& target_sensor : source_poi.second) {
                printf("    P%d -> S%d;\n", source_poi.first, target_sensor);
            }
        }
        printf("    label = \"POI Connections\";\n    color = green;\n  }\n");

        for (std::pair<int, std::unordered_set<int>> source_sensor : this->sensor_sensor) {
            for (const auto& target_sensor : source_sensor.second) {
                printf("  S%d -> S%d;\n", source_sensor.first, target_sensor);
            }
        }

        printf("  subgraph sinks {\n    node [style=filled];\n");
        for (std::pair<int, std::unordered_set<int>> source_sensor : this->sensor_sink) {
            for (const auto& target_sink : source_sensor.second) {
                printf("    S%d -> K%d;\n", source_sensor.first, target_sink);
            }
        }
        printf("    label = \"SINK Connections\";\n    color = black;\n  }\n");
        printf("}\n");
    }
}

KCMC_Instance::KCMC_Instance(
        int num_pois, int num_sensors, int num_sinks,
        int area_side, int coverage_radius, int communication_radius,
        long long random_seed
): KCMC_Instance(num_pois, num_sensors, num_sinks,
                 area_side, coverage_radius, communication_radius,
                 random_seed, false) {}

KCMC_Instance::KCMC_Instance(const std::string& serialized_kcmc_instance) {

    // Set the flag
    this->splitted = false;

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


/* #####################################################################################################################
 * FUNCTIONAL INSTANCE IMPLEMENTATIONS
 */


std::string KCMC_Instance::key() const{
    std::ostringstream out;
    out << num_pois  <<' '<< num_sensors            <<' '<< num_sinks                   << ';';
    out << area_side <<' '<< sensor_coverage_radius <<' '<< sensor_communication_radius << ';';
    out << random_seed;
    return out.str();
}


std::string KCMC_Instance::serialize(){
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


/* #####################################################################################################################
 * PRIVATE INSTANCE IMPLEMENTATIONS
 */


void KCMC_Instance::push(std::unordered_map<int, std::unordered_set<int>> &buffer, int source, int target){
    if (isin(buffer, source)){buffer[source].insert(target);}
    else {
        std::unordered_set<int> new_set;
        new_set.insert(target);
        buffer[source] = new_set;
    }
}


int KCMC_Instance::parse_edge(const int stage, const std::string& token){

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
            push(this->sensor_poi, target, source);
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


void KCMC_Instance::split_all_sensors(){

    // Memoized verification
    if (this->splitted){return;}

    // For each sensor
    int source_in, source_out;
    for (std::pair<int, std::unordered_set<int>> source_pair : this->sensor_sensor) {

        /* Create two new sensors: a sensor and its inverse for the output.
         * The double of the integer ID number of each sensor will be the integer ID of its in-copy, and its out-copy
         * will be the integer value of its in-copy ID plus 1
         */
        source_in = 2*source_pair.first;
        source_out = 1+source_in;

        // Create an directed edge from the in-copy to the out-copy
        push(split_sensors, source_in, source_out);

        // For each target sensor, create an edge starting from the source out-copy to the target in-copy
        for (const auto& target_in : source_pair.second) {
            push(split_sensors, source_out, 2*target_in);
        }
    }

    // Set the memoized flag
    this->splitted = true;
}


/* #####################################################################################################################
 * K-COVERAGE VALIDATOR
 */


std::string KCMC_Instance::k_coverage(const int k) {
    /** Verify if every POI has at least K different sensors covering it
     */
     if (k < 1){return "SUCCESS";}

    // Start buffers
    std::ostringstream out;

    // For each POI
    for (int poi=0; poi < num_pois; poi++) {

        // If not enough sensors cover the POI, return False
        if (poi_sensor[poi].size() < k) {
            out << "POI " << poi << " COVERAGE " << poi_sensor[poi].size();
            return out.str();
        }
    }

    // Success in each and every POI!
    return "SUCCESS";
}


std::string KCMC_Instance::m_connectivity(const int m) {
    /** Verify if every POI has at least M different disjoint paths to all SINKs
     */

    // If the value for M is invalid, return SUCCESS
    if (m <= 0){return "INVALID VALUE FOR M";}

    // Check if we have at least K=M coverage beforehand
    std::string k_cov_msg = this->k_coverage(m);
    if (k_cov_msg != "SUCCESS") {return "NOT ENOUGH COVERAGE - "+k_cov_msg;}

    // Start the reused buffers. Due to the split-vertex implementation, the buffers must be two times the normal size
    std::ostringstream out;
    int poi, sensor, num_paths, predecessors[MAX_SENSORS*2], i;
    bool used[MAX_SENSORS * 2], visited[MAX_SENSORS * 2];
    std::queue<int> bfs_queue;

    /* Compute the split-vertex version of this instance (memoized function)
     * We need the split-vertex version of this instance instead of the regular version because the algorithm used
     * (first proposed by Ford and Fulkerson in 1956, then revisited by Edmonds and Karp in 1972 and now altered to our
     * needs in 2021) finds *edge-disjoint* paths from a POI to any sink, and we need *node-disjoint* paths, as are
     * sensors that are prone to fail and not the link between them.
     * The split-vertex version of the instance creates two copies of each sensor, named the "in" and "out" copies.
     * All edges that start at the original sensor must now start at the "out" copy, and all edges that arrive at the
     * original sensor must now arrive at the "in" copy. There must be a directed edge from the "in" to the "out" copy.
     * Since our implementation of the graphs supports directed edges, we use the same hashmap data structure.
     */
    this->split_all_sensors();

    // For each POI
    for (poi=0; poi < num_pois; poi++) {

        /* Reset the buffers used for the POI
         * - Set "0" as the total number of found paths from this POI to any sink
         * - Prepare the used array filled with "false", meaning that no sensor is used (yet)
         * - Prepare the visited array filled with "false", meaning that no sensor was visited (yet)
         */
        num_paths = 0;
        memset(used, false, sizeof(bool)*MAX_SENSORS*2);
        memset(visited, false, sizeof(bool)*MAX_SENSORS*2);

        /* While we still do not have the required number of paths
         * - Run a modified BFS iteration in the split-vertex version of this instance.
         *   - Each iteration starts with a queue of all the sensors that cover the POI and has not yet been used.
         *   - One iteration of the modified BFS runs until at least one sink-adjacent sensor is found.
         *   - An iteration cannot use a sensor that has already been marked as used, and can only return a single path.
         *   - At the end of the iteration, all sensors that are part of the found POI->sink path are marked as used.
         *   - If no paths are found and the BFS queue is empty, the iteration breaks the loop, for the POI has failed.
         */
        while (num_paths < m) {

            /* Reset the buffers for the new iteration
             * - Prepare the predecessors array filled with the null sensor
             * - Prepare the BFS queue as filled ONLY with the in-copies of the sensors that cover the current POI
             *   - Set the inverse of the integer ID of the current POI as the predecessor of each sensor in the queue.
             *     - It is negative to make it different from the ID of any sensor.
             */
            memset(predecessors, NULL_SENSOR, sizeof(int)*MAX_SENSORS*2);
            while (!bfs_queue.empty()) {bfs_queue.pop();}
            for (const int covering_sensor : poi_sensor[poi]) {
                predecessors[(2*covering_sensor)] = -1*poi;
                bfs_queue.push((2*covering_sensor));
                //printf("POI %d -> S%d\n", poi, 2*covering_sensor);
            }

            // Iterate until the queue is empty or the loop is broken from within
            while (!bfs_queue.empty()) {

                // Extract the first sensor in the queue and mark it as visited
                sensor = bfs_queue.front();
                bfs_queue.pop();
                visited[sensor] = true;
                //printf("\tS%d -> S%d\n", predecessors[sensor], sensor);

                /* If the sensor connects to a sink
                 * - Mark all the predecessors of the sensor as "used"
                 * - Increase the number of paths
                 * - If we have "m" paths, break the loop.
                 *   - If not, finish just the current iteration
                 */
                if (isin(sensor_sink, (int)(sensor/2))) {
                    //printf("\t\tS%d -> K\n", sensor);

                    i = sensor;
                    while (predecessors[i] != (-1*poi)) {
                        if (predecessors[i] == NULL_SENSOR) {throw std::runtime_error("NULL SENSOR AS PREDECESSOR!");}
                        used[predecessors[i]] = true;
                        i = predecessors[i];
                    }
                    num_paths += 1;
                    if (num_paths >= m) {break;}
                    else {continue;}
                }

                /* If we got here, the sensor does not connect to any sink. Thus, we continue the iteration
                 * - For each unused and unvisited neighbor of the sensor (in the split-vertex graph)
                 *   - Add it to the queue
                 *   - Mark the predecessor of each neighbor as the minimal of the current sensor or its current predecessor
                 *     - The NULL_PREDECESSOR is always a value larger than any split-sensor.
                 *     - The original predecessor (the POI) is the only negative value
                 */
                for (const int neighbor_sensor : split_sensors[sensor]) {
                    if ((not used[neighbor_sensor]) and (not visited[neighbor_sensor])) {
                        predecessors[neighbor_sensor] = std::min(sensor, predecessors[neighbor_sensor]);
                        bfs_queue.push(neighbor_sensor);
                    }
                }
            }

            // Stop the POI iteration if we already have enough paths
            if (num_paths >= m) {break;}
        }

        // If we still do not have enough paths for the POI, return the error
        if (num_paths < m) {
            out << "POI " << poi << " CONNECTIVITY " << num_paths;
            return out.str();
        }
        //else {printf("POI %d FLOW %d \n", poi, num_paths);}
    }

    // Success in each and every POI!
    return "SUCCESS";
}
