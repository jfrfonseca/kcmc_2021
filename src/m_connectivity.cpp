

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


/** M-CONNECTIVITY VALIDATOR USING DINIC'S ALGORITHM
 */


std::string KCMC_Instance::m_connectivity(const int m, std::unordered_set<int> &inactive_sensors) {
    /** Verify if every POI has at least M different disjoint paths to all SINKs
     */
    // Base case
    if (m < 1){return "SUCCESS";}

    // Create the level graph
    int level_graph[this->num_sensors];
    this->level_graph(level_graph, inactive_sensors);

    // Prepare the output buffer and the set of "used" sensors
    std::ostringstream out;
    std::unordered_set<int> used_sensors;

    // Create a loop control flag and pointer buffers
    int paths_found, path_end, a_poi, predecessors[this->num_sensors];

    // Run for each POI, returning at the first failure
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        paths_found = 0;  // Clear the number of paths found for the POI
        used_sensors = inactive_sensors;  // Reset the set of used sensors for each POI

        // While there are still paths to be found
        while (paths_found < m) {
            std::fill(predecessors, predecessors+this->num_sensors, -2);

            // Find a path
            path_end = this->find_path(a_poi, used_sensors, level_graph, predecessors);

            // If the path ends in an invalid sensor, return the failure.
            if (path_end == -1) {
                out << "POI " << a_poi << " CONNECTIVITY " << paths_found;
                return out.str();

            // If success, count the path and mark all the sensors with predecessors as "used"
            } else {
                paths_found += 1;  // Count the newfound path
                // Unravel the path, marking each sensor as used
                while (path_end != -1) {
                    used_sensors.insert(path_end);
                    path_end = predecessors[path_end];
                    if (path_end == -2) {throw std::runtime_error("FORBIDDEN ADDRESS!");}
                }
            }
        }
    }

    // Success in each and every POI!
    return "SUCCESS";
}
