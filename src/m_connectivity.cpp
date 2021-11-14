

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** BREADTH-FIRST SEARCH (BFS) ALGORITM
 */


std::unordered_set<int> KCMC_Instance::bfs(std::unordered_set<int> &seed_sensors,
                                           std::unordered_set<int> &inactive_sensors) {
    // Search each sensor of the current set, adding its neighbors to the result set
    std::unordered_set<int> result_set;
    for (const int &source : seed_sensors) {
        for (const int &neighbor : this->sensor_sensor[source]) {
            if (not isin(inactive_sensors, neighbor)) {
                result_set.insert(neighbor);
            }
        }
    }
    return result_set;
}


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
