

// STDLib dependencies
#include <sstream>    // ostringstream
#include <queue>      // queue
#include <iostream>   // cin, cout, endl
#include <iomanip>    // setfill, setw

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers
#include "genetic_algorithm_operators.h"  // exit_signal_handler


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

    // Add all poi-covering sensors to the result buffer
    for (a_poi=0; a_poi < this->num_pois; a_poi++) {
        for (const int &a_sensor : this->poi_sensor[a_poi]) {
            vote(*visited_sensors, a_sensor);
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

            // Reset the control buffers
            next_i = -1;
            path_length = 0;

            // If the path ends in an invalid sensor, mark the loop to end. If we do not have enough paths, throu error
            if (path_end == -1) {
                break_loop = true;
                if (paths_found < m) { throw std::runtime_error("INVALID INSTANCE! (INSUFFICIENT CONNECTIVITY)"); }
            }

            // If it is a sucessful path
            else {
                paths_found += 1;  // Count the newfound path
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
