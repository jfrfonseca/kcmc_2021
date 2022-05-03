

// STDLib dependencies
#include <sstream>    // ostringstream
#include <queue>      // priority_queue

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** LEVEL-GRAPH ALGORITM
 * Sets in each active sensor its level, that is the lowest distance to a sink using only active sensors
 */
int KCMC_Instance::level_graph(int level_graph[], std::unordered_set<int> &inactive_sensors) {
    /* Sets the lowest distance in hops from each active sensor to the nearest sink using only active sensors
     */

    // Reused buffers
    int level = 0;
    std::unordered_set<int> visited, work_set, next_set;

    // Get the set of active neighbors of sinks. Set each neighbor's level to 0
    for (const auto &a_sink : this->sink_sensor) {
       for (const int &neighbor : a_sink.second) {
           if (not isin(inactive_sensors, neighbor)) {
               level_graph[neighbor] = 0;
               work_set.insert(neighbor);
           }
       }
    }

    // Mark all sensors in the set as visited, as well as the inactive sensors
    visited = set_merge(inactive_sensors, work_set);

    // While there are still sensors to visit, find and visit them and set their level
    while (!work_set.empty()) {
        // advance the level
        level++;

        // update the next set and the levels of the sensors in the work set
        next_set.clear();
        for (const int &source : work_set) {
            for (const int &neighbor : this->sensor_sensor[source]) {
                if (not isin(visited, neighbor)) {
                    next_set.insert(neighbor);
                    level_graph[neighbor] = level;
                }
            }
        }

        // Mark the work set as visited and swap it to the next set
        visited = set_merge(visited, work_set);
        work_set = next_set;
    }

    // Return the max level found
    return level;
}


/** A* (A-STAR) PATHFINDING ALGORITHM
 */
int KCMC_Instance::find_path(const int poi_number, std::unordered_set<int> &used_sensors,
                             int level_graph[], int predecessors[]) {

    // Local buffers
    int i_sensor;
    std::priority_queue<LevelNode, std::vector<LevelNode>, CompareLevelNode> queue;

    // Prepare a queue with each active unused sensor that covers the POI
    // Also add each sensor to the predecessors map, having "-1" as the predecessor
    for (const int &a_sensor : this->poi_sensor[poi_number]) {
        if (not isin(used_sensors, a_sensor)) {
            queue.push({a_sensor, level_graph[a_sensor]});
            predecessors[a_sensor] = -1;
        }
    }

    // Iterate until the queue is empty
    while (not queue.empty()) {
        // Get the top sensor in the queue (lowest level) and visit it
        i_sensor = queue.top().index;
        queue.pop();

        // If the sensor is neighbor of a sink, return the path
        if (isin(this->sensor_sink, i_sensor)) {return i_sensor;}

        // Add the unvisited active neighbors of the sensor to the queue
        // Each sensor is also added to the predecessors map
        for (const int &neighbor : this->sensor_sensor[i_sensor]) {
            if ((not isin(used_sensors, neighbor)) and (predecessors[neighbor] == -2)){
                queue.push({neighbor, level_graph[neighbor]});
                predecessors[neighbor] = i_sensor;
            }
        }
    }

    // If we got here, there is no possible path :(
    return -1;
}


/** FAST M-CONNECTIVITY VALIDATOR USING DINIC'S ALGORITHM
 * Fastest validator.
 * It could also validate if every POI has at least M connections to sensors,
 *     but it would be unnecessary if K-coverage has already been validated
 *     and K >= M. Thus we do not try.
 * We do, however, note every single sensor used anywhere in the resulting WSN
 */
int KCMC_Instance::fast_m_connectivity(const int m, std::unordered_set<int> &inactive_sensors,
                                       std::unordered_set<int> *all_used_sensors) {
    /** Verify if every POI has at least M different disjoint paths to all SINKs
     */
    // Clear the set of active sensors
    all_used_sensors->clear();

    // Base case
    if (m < 1){return -1;}

    // Create the level graph
    int level_graph[this->num_sensors];
    this->level_graph(level_graph, inactive_sensors);

    // Prepare the set of "used" sensors
    std::unordered_set<int> used_sensors;

    // Create a loop control flag and pointer buffers
    int paths_found, path_end, a_poi, predecessors[this->num_sensors];

    // Run for each POI, returning at the first failure
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        paths_found = 0;  // Clear the number of paths found for the POI
        used_sensors = inactive_sensors;  // Reset the set of used sensors for each POI

        // While there are still paths to be found
        while (paths_found < m) {
            std::fill(predecessors, predecessors+this->num_sensors, -2);  // Reset the predecessors buffer

            // Find a path
            path_end = this->find_path(a_poi, used_sensors, level_graph, predecessors);

            // If the path ends in an invalid sensor, return the failure.
            if (path_end == -1) {
                return (a_poi*1000000)+paths_found;  // Encoded the two ints. We must not have more than a Million POIs!

            // If success, count the path and mark all the sensors with predecessors as "used"
            } else {
                paths_found += 1;  // Count the newfound path
                // Unravel the path, marking each sensor in it as used
                while (path_end != -1) {
                    used_sensors.insert(path_end);
                    all_used_sensors->insert(path_end);  // Get the complete list of all used sensors
                    path_end = predecessors[path_end];
                    if (path_end == -2) {throw std::runtime_error("FORBIDDEN ADDRESS!");}
                }
            }
        }
    }

    // Success in each and every POI!
    return -1;
}

/** M-CONNECTIVITY VALIDATOR USING DINIC'S ALGORITHM
 * Wrapper around the fastest validator, to allow for better process message passing.
 */
std::string KCMC_Instance::m_connectivity(const int m, std::unordered_set<int> &inactive_sensors) {
    std::unordered_set<int> used_sensors;
    int failure_at = this->fast_m_connectivity(m, inactive_sensors, &used_sensors);
    if (failure_at == -1) {return "SUCCESS";}
    else {
        std::ostringstream out;
        int a_poi = failure_at / 1000000, paths_found = failure_at % 1000000;  // Decode the result
        out << "POI " << a_poi << " CONNECTIVITY " << paths_found;
        return out.str();
    }
}



/** Connectivity getter
 * Gets the connectivity at each POI, and the number of POIs with any connectivity at all
 * For faster results, limit the connectivity at "target".
 */
int KCMC_Instance::get_connectivity(int buffer[], std::unordered_set<int> &inactive_sensors, int target) {
    // This method is a targeted variance to allow for a LARGE speedup in finding a smaller target

    // Create the level graph
    int level_graph[this->num_sensors];
    this->level_graph(level_graph, inactive_sensors);

    // Prepare the buffer set of "used" sensors
    std::unordered_set<int> used_sensors;

    // Create a loop control flag and pointer buffers, and a counter for the number of connected POIs
    int paths_found, path_end, a_poi, predecessors[this->num_sensors], has_connection = 0;

    // Run for each POI, returning at the first failure
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        paths_found = 0;  // Clear the number of paths found for the POI
        used_sensors = inactive_sensors;  // Reset the set of used sensors for each POI

        // While there are still paths to be found
        while (paths_found < target) {
            std::fill(predecessors, predecessors+this->num_sensors, -2);  // Reset the predecessors buffer

            // Find a path
            path_end = this->find_path(a_poi, used_sensors, level_graph, predecessors);

            // If the path ends in an invalid sensor, stop the WHILE loop
            if (path_end == -1) {
                buffer[a_poi] = paths_found;
                has_connection += 1;
                paths_found = target;

            // If success, count the path and mark all the sensors with predecessors as "used"
            } else {
                paths_found += 1;  // Count the newfound path
                // Unravel the path, marking each sensor in it as used
                while (path_end != -1) {
                    used_sensors.insert(path_end);
                    path_end = predecessors[path_end];
                    if (path_end == -2) {throw std::runtime_error("FORBIDDEN ADDRESS!");}
                }
                // Store if we've reached the target
                if (paths_found >= target) {
                    buffer[a_poi] = paths_found;
                }
            }
        }
    }

    // Return the number of connected POIs
    return has_connection;
}
int KCMC_Instance::get_connectivity(int buffer[], std::unordered_set<int> &inactive_sensors) {
    return this->get_connectivity(buffer, inactive_sensors, 10);  // Default value for target
}


int KCMC_Instance::local_optima(int k, int m, std::unordered_set<int> &inactive_sensors, std::unordered_set<int> *result_buffer) {

    // Prepare buffers
    int valid;
    std::unordered_set<int> k_used_sensors, m_used_sensors, all_used_sensors;

    // Check validity, recovering the used sensors for K coverage and M connectivity
    valid = this->fast_k_coverage(k, inactive_sensors, &k_used_sensors);
    if (valid != -1) {throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT COVERAGE)");}
    valid = this->fast_m_connectivity(m, inactive_sensors, &m_used_sensors);
    if (valid != -1) {throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT CONNECTIVITY)");}

    // Store the used sensors in the given buffer
    all_used_sensors = set_merge(k_used_sensors, m_used_sensors);
    result_buffer->clear();
    *result_buffer = all_used_sensors;

    // Return the real number of inactive sensors
    return this->num_sensors - ((int)all_used_sensors.size());
}
