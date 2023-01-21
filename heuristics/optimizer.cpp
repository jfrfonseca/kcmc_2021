

// STDLib dependencies
#include <sstream>    // ostringstream
#include <queue>      // queue
#include <iostream>   // cin, cout, endl
#include <iomanip>    // setfill, setw

// Dependencies from this package
//#include "kcmc_instance.h"  // KCMC Instance class headers


/** LOCAL OPTIMA DINIC ALGORITM
 * Get a set of sensors from the instance that has k-coverage and m-connectivity using the Dinic algorithm, a simple
 * greedy method that moves to the node closer to the sink.
 */

int KCMC_Instance::local_optima(int k, int m, std::unordered_set<int> &inactive_sensors, std::unordered_set<int> *result_buffer) {

    // Prepare buffers
    std::unordered_set<int> k_used_sensors, m_used_sensors, all_used_sensors;

    // Check validity, recovering the used sensors for K coverage and M connectivity
    this->validate(true, k, m, inactive_sensors, &k_used_sensors, &m_used_sensors);

    // Store the used sensors in the given buffer
    all_used_sensors = set_merge(k_used_sensors, m_used_sensors);
    result_buffer->clear();
    *result_buffer = all_used_sensors;

    // Return the real number of inactive sensors
    return this->num_sensors - ((int)all_used_sensors.size());
}


/** FLOOD-DINIC ALGORITM
 * For each POI, finds M node-disjoint paths connecting the POI to the SINK. Then "floods" the set of POIs found paths.
 * Flooding: Let path A connect POI P to sink S. Let A also be be a sequence of active sensors so that the first sensor
 * i0 in the sequence covers POI P, and the last sensor ik connects to sink S. So, A = (P)i0,i1,...ik(S).
 * The "flooded" version of path A is a set of sensors that contains, for each connected triple ix-1, ix, ix+1 in A,
 * all sensors that connect both to ix-1 and ix+1. At the starting edge of A, ix-1 is P. At the end edge of A, ix+1 is S
 */
int KCMC_Instance::flood(int k, int m, bool full,
                         std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *visited_sensors) {

    // Base case
    if (m < 1){return -1;}

    // Create the level graph, loop controls and buffers
    bool break_loop;
    int level_graph[this->num_sensors], predecessors[this->num_sensors],
        paths_found, path_end, a_poi, path_length, longest_required_path_length, previous, next_i,
        total_paths_found = 0;

    // Update the level graph
    this->level_graph(level_graph, inactive_sensors);

    // Prepare the set of "used" sensors for each POI
    std::unordered_set<int> used_sensors;

    // Validate K-Coverage
    if (this->fast_k_coverage(k, inactive_sensors, &used_sensors) != -1) {
        throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT COVERAGE)");
    }

    // Reset the results buffer
    visited_sensors->clear();

    // Add all poi-covering sensors to the result buffer, voting them as many times as they cover sensors
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        for (const int &a_sensor : this->poi_sensor[a_poi]) {
            vote(*visited_sensors, a_sensor, (int)(this->sensor_poi[a_sensor].size()));
        }
    }

    // Run for each POI, returning at the first failure
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        break_loop = false;  // Mark the loop for processing
        paths_found = 0;  // Clear the number of paths found for the POI
        longest_required_path_length = 0; // reset the stored length of the last found path
        used_sensors = inactive_sensors;  // Reset the set of used sensors for each POI

        // While the stopping criteria was not found
        while (not break_loop) {
            std::fill(predecessors, predecessors+this->num_sensors, -2);  // Reset the predecessors buffer

            // Find a path
            path_end = this->find_path(a_poi, used_sensors, level_graph, predecessors);

            // If the path ends in an invalid sensor, mark the loop to end. If we do not have enough paths, throw error
            if (path_end == -1) {
                break_loop = true;
                if (paths_found < m) { throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT CONNECTIVITY)"); }
            }

            // If it is a sucessful path
            else {

                // Reset the control buffers
                next_i = -1;
                path_length = 0;

                // Increase the counters with the newly found path
                paths_found += 1;
                total_paths_found += 1;

                // Unravel the path, marking each sensor in it as used and flooding it
                while (path_end != -1) {
                    used_sensors.insert(path_end);
                    path_length += 1;

                    // Get the previous sensor in the path
                    previous = predecessors[path_end];
                    if (previous == -2) { throw std::runtime_error("FORBIDDEN ADDRESS!"); }

                    /* If the previous sensor is a POI and the next is a SINK
                     * Add all active sensors that connect both to the POI and the SINK to the result buffer
                     */
                    if ((previous == -1) and (next_i == -1)) {
                        for (const int &bridge: this->poi_sensor[a_poi]) {
                            if (isin(this->sensor_sink, bridge) and (not isin(inactive_sensors, bridge))) {
                                vote(*visited_sensors, bridge);
                            }
                        }
                    } else {
                        /* If the previous sensor is a POI (and the next cannot be a SINK)
                         * Add all active sensors that cover the POI and connect to the path_end sensor to the result
                         */
                        if (previous == -1) {
                            for (const int &cover: this->poi_sensor[a_poi]) {
                                if (isin(this->sensor_sensor[cover], path_end) and (not isin(inactive_sensors, cover))) {
                                    vote(*visited_sensors, cover);
                                }
                            }
                        } else {
                            /* If the previous sensor is NOT a POI and the next IS a SINK
                             * Add all active sensors that connect to both the previous sensor and the sink
                             */
                            if (next_i == -1) {
                                for (const int &conn: this->sensor_sensor[previous]) {
                                    if (isin(this->sensor_sink, conn) and (not isin(inactive_sensors, conn))) {
                                        vote(*visited_sensors, conn);
                                    }
                                }
                            } else {
                                /* If the previous sensor is NOT a POI ant the next is NOT a sink
                                 * Add all active sensors that connect to both the previous and the next to the result
                                 */
                                for (const int &conn: this->sensor_sensor[previous]) {
                                    if (isin(this->sensor_sensor[conn], next_i) and (not isin(inactive_sensors, conn))) {
                                        vote(*visited_sensors, conn);
                                    }
                                }
                            }
                        }
                    }

                    // Mark that the next in the path is not a sink and advance the path
                    next_i = path_end;
                    path_end = previous;
                }

                /* FULL version:
                 * If we have enough paths, but the current is no larger than the last, continue the loop.
                 * MIN version:
                 * Stop as soon as we get M paths for this POI
                 */
                if (full) {
                    if (paths_found <= m) {
                        longest_required_path_length = (path_length > longest_required_path_length) ? path_length : longest_required_path_length;
                    }
                    if (path_length > longest_required_path_length) { break_loop = true; }
                } else {
                    longest_required_path_length = path_length;
                    if (paths_found == m) { break_loop = true; }
                }
            }
        }
    }

    // Success in each and every POI! Return the total of found paths
    return total_paths_found;
}


/** MAX-REUSE
 * To minimize the number of sensors, we try to maximize reuse of sensors (i.e. the same sensor is used in multiple
 * connection paths, each from a different POI).
 * To do that, we run the max-flood method, counting how often each sensor was used (sensor frequency map).
 * Then, we run dinic's method again, but instead of the distance to the sink we use the sensor frequency map.
 * The resulting set of sensors will maximize reuse while minimizing distance to the sink.
 * This is an heuristic method - we might not get the most reduced of all sets of sensors!
 * However, it is a *fast* method, running in polynomial time even in the worst of cases.
 */

int KCMC_Instance::reuse(int k, int m, int flood_level,
                         std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *visited_sensors) {

    // Local buffers
    int num_paths, inv_frequency_array[this->num_sensors],
        paths_found, path_end, a_poi, predecessors[this->num_sensors],
        active_covering_sensors, add_sensor, pre_k_cov_sensors;
    std::priority_queue<LevelNode, std::vector<LevelNode>, CompareLevelNode> queue;

    // First we clear out the output buffer
    visited_sensors->clear();

    /* Then we get the flood of the instance:
     * MAX-FLOOD if the flood level is infinite (lower than 0)
     * NO-FLOOD  if the flood level is 0
     * MIN-FLOOD if the flood level is 1 (or more)
     */
    if (flood_level == 0) {num_paths = this->fast_m_connectivity(m, inactive_sensors, visited_sensors);}
    else {num_paths = this->flood(k, m, (flood_level < 0), inactive_sensors, visited_sensors);}
    if (num_paths >= 1000000) {throw std::runtime_error("INVALID NUMBER OF PATHS!");}

    /* Then format the frequency graph as a vector for minimization, similar to the level-graph
     * This is called the *inverse frequency array* (IFA). It holds no values smaller than 1.
     * In the IFA, sensors that were not found by the flood method have frequency num_paths
     * In the IFA, sensors that were found by the flood method have freqeuency num_paths-(orig. frequency)
     * This inversion is done so the minimization loop can still be used
     */
    std::fill(inv_frequency_array, inv_frequency_array + this->num_sensors, num_paths);
    for (const auto &i : *visited_sensors) {inv_frequency_array[i.first] = num_paths - i.second;}

    // Prepare the set of "used" sensors and clear the map of visited sensors
    std::unordered_set<int> used_sensors, set_visited_sensors, final_inactive_sensors;
    visited_sensors->clear();

    // Run for each POI, returning at the first failure
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        paths_found = 0;  // Clear the number of paths found for the POI
        used_sensors = inactive_sensors;  // Reset the set of used sensors for each POI

        // While there are still paths to be found
        while (paths_found < m) {
            std::fill(predecessors, predecessors+this->num_sensors, -2);  // Reset the predecessors buffer

            // Find a path
            path_end = this->find_path(a_poi, used_sensors, inv_frequency_array, predecessors);

            // If the path ends in an invalid sensor, break the loop. Other POIs will fix it
            if (path_end == -1) {break;}
            else {
                paths_found += 1;  // Count the newfound path
                // Unravel the path, marking each sensor in it as used
                while (path_end != -1) {
                    used_sensors.insert(path_end);
                    vote(*visited_sensors, path_end);  // Get the complete frequency map of all used sensors
                    path_end = predecessors[path_end];
                    if (path_end == -2) {throw std::runtime_error("FORBIDDEN ADDRESS!");}
                }
            }
        }
    }
    pre_k_cov_sensors = (int)(visited_sensors->size());

    /* Update the IFA
     * Update the frequencies to the IFA
     * Increase (thus, subtract from) the frequency of each sensor the number of POIs it covers
     */
    std::fill(inv_frequency_array, inv_frequency_array + this->num_sensors, num_paths);
    for (const auto &i : *visited_sensors) {inv_frequency_array[i.first] = num_paths - i.second;}
    for (const auto &i : this->sensor_poi) {inv_frequency_array[i.first] -= (int)(i.second.size());}

    /* Add the sensors required to guarantee K-Coverage
     * Compute the frequency graph into the inverse frequency array
     * For each POI, for each sensor that covers the POI
     *     If the sensor is inactive, add it to a priority queue given its value in the inverse frequency array (IFA)
     * For each POI that does not have enough coverage, add sensors until it has enough coverage.
     *     Add sensors using the priority queue.
     *     For each added sensor, decrease its value in the IFA (thus givving it more priority).
     *     For each added sensor, increase its frequency in the final frequency map of each sensor.
     */
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        while (not queue.empty()) {queue.pop();}  // Empty the queue
        // Count and enqueue the covering sensors
        active_covering_sensors = 0;
        for (const int a_sensor : this->poi_sensor[a_poi]) {
            if (isin(*visited_sensors, a_sensor)) {active_covering_sensors++;}  // Count the active covering sensors
            else {queue.push({a_sensor, inv_frequency_array[a_sensor]});}  // Add to the queue the inactive sensors
            if (active_covering_sensors >= k) {break;}  // Stop prematurely if we have enough covering sensors
        }
        // Add the first sensors in the queue until we have enough sensors
        for (add_sensor=active_covering_sensors; add_sensor < k; add_sensor++) {
            vote(*visited_sensors, queue.top().index);  // Increase the usage of this sensor
            inv_frequency_array[queue.top().index] -= 1;  // Decrease the frequency of this sensor in the IFA
            queue.pop();  // Remove the sensor from the queue
        }
    }

//    // Re-Validate IN DEBUG MODE
//    setify(set_visited_sensors, visited_sensors);
//    this->invert_set(final_inactive_sensors, &set_visited_sensors);  // Sensors not visited
//    final_inactive_sensors = set_diff(final_inactive_sensors, inactive_sensors);  // Remove the pre-inactivated
//    bool valid = this->validate(true, k, m, final_inactive_sensors);  // Final check-up, safe-mode only
//    if (not valid) {throw std::runtime_error("REUSE VALIDATION ERROR!");}
//    // END re-validate IN DEBUG MODE

    // Return the number of otherwise inactive sensors that were added only to guarantee k-coverage
    return ((int)(visited_sensors->size()))-pre_k_cov_sensors;
}
int KCMC_Instance::reuse(int k, int m,
                         std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *visited_sensors) {
    int added_min_r, min_r,  // Buffer for the number of nodes added for K-coverage and the resulting number of nodes
        added_no_r,  no_r,   // for each pair of buffers, we use one of the reuse variations
        added_max_r, max_r;
    std::unordered_map<int, int> min_visited, no_visited, max_visited;
    std::unordered_set<int> set_used_installation_spots;

    added_min_r = this->reuse(k, m, -1, inactive_sensors, &min_visited);
    setify(set_used_installation_spots, &min_visited);
    min_r = (int)(set_used_installation_spots.size());
    added_no_r  = this->reuse(k, m,  0, inactive_sensors, &no_visited);
    setify(set_used_installation_spots, &no_visited);
    no_r = (int)(set_used_installation_spots.size());
    added_max_r = this->reuse(k, m,  1, inactive_sensors, &max_visited);
    setify(set_used_installation_spots, &max_visited);
    max_r = (int)(set_used_installation_spots.size());

    // Return the smallest
    if (min_r <= no_r) {
        if (min_r <= max_r) {
            visited_sensors->insert(min_visited.begin(), min_visited.end());
            return added_min_r;
        } else {
            visited_sensors->insert(max_visited.begin(), max_visited.end());
            return added_max_r;
        }
    } else {
        if (no_r <= max_r) {
            visited_sensors->insert(no_visited.begin(), no_visited.end());
            return added_no_r;
        } else {
            visited_sensors->insert(max_visited.begin(), max_visited.end());
            return added_max_r;
        }
    }
}
