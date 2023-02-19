

// STDLib dependencies
#include <queue>      // queue
#include <algorithm>  // std::find, std::set_intersection

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** Helper Functions
 * Functions that are not optimizers themselves, but are used by optimizer
 * invert_sensor_set gets a set of sensors (i.e. all inacitive sensors) and returns the set of sensors in the instance that are *not* in the set
 * add_k_cov adds sensors to a set until the set is enough to hold the property of k-coverage
 * strongest_flow_first_search is a search that prioritizes sensors that are part of many DINIC paths to the sink
 */


int KCMC_Instance::add_k_cov(int k, std::unordered_set<int> &active_sensors) {

    // Prepare buffers
    int initial_size = (int)(active_sensors.size()), active_coverage, lacking;
    std::set<int> set_inactive_sensors;
    std::unordered_map<int, int> tally, poi_deficit;
    std::unordered_map<int, std::set<int>> map_deficit;
    std::set<LevelNode, CompareLevelNode> q;

    // Get a SET of INACTIVE sensors
    // Ise std::set instead of std::unordered_set for set intersection efficiencies in implementation
    for (int i=0; i < this->num_sensors; i++){
        if (not isin(active_sensors, i)) {
            set_inactive_sensors.insert(i);
        }
    }

    // For each POI
    for (const auto &a_poi : this->set_poi_sensor) {

        // Get the set of INACTIVE SENSORS that COULD cover the POI if were active
        std::set<int> inactive_coverage;  // A NEW set must be created in each iteration!
        std::set_intersection(a_poi.second.begin(), a_poi.second.end(),
                              set_inactive_sensors.begin(), set_inactive_sensors.end(),
                              std::inserter(inactive_coverage, inactive_coverage.end()));

        // If the POI has enough coverage, skip it
        active_coverage = (int)(a_poi.second.size() - inactive_coverage.size());
        if (active_coverage >= k) {
            continue;
        }

        // Store the inactive coverage for the POI
        map_deficit[a_poi.first] = inactive_coverage;
        poi_deficit[a_poi.first] = k - active_coverage;  // The DEFICIT is how many more sensors we need for the POI

        // Vote on each inactive sensor that could cover the POI
        for (const int i : inactive_coverage) {
            if (not isin(tally, i)) {tally[i] = 0;}
            tally[i]++;
        }
    }

    // If we have no POIs that need coverage, return immediately!
    if (map_deficit.empty()) {return 0;}

    // If we got here, we need to add more sensors to the solution.
    // Make a queue with the sensors that could cover more POIs first
    for (const auto &a_voted_sensor : tally) {q.insert({a_voted_sensor.first, a_voted_sensor.second});}

    // For each POI that needs coverage
    for (const auto &a_poi : map_deficit) {
        lacking = poi_deficit[a_poi.first];  // How many we need

        // For each sensor in the queue, in order of sensors that cover the most POIs with insufficient coverage
        for (const LevelNode a_voted_sensor : q) {

            // If the sensor covers the POI
            if (isin(a_poi.second, a_voted_sensor.index)) {

                // Add it to the solution
                active_sensors.insert(a_voted_sensor.index);

                // Decrease the number of sensors we still need
                lacking--;

                // If we need no more sensors, we are done processing of the current POI
                if (lacking == 0) {break;}
            }
        }
    }

    // If we got here, all POIs now have enough coverage! Hopefully not many sensors were added. We return how many
    return (int)(active_sensors.size()) - initial_size;
}


int KCMC_Instance::strongest_flow_first_search(int m, bool flood, std::unordered_set<int> &all_visited) {

    // Prepare buffers
    int tally[this->num_sensors],
        i, total_paths=0, paths_found, neigh;
    std::priority_queue<LevelNode, std::vector<LevelNode>, CompareLevelNode> q;
    std::unordered_set<int> visited_in_poi, phi;
    all_visited.clear();

    // Runs DINIC to validate m-connectivity and get the tally of paths per sensor
    // If the set of visited sensors is empty, then we do not have a viable solution
    this->dinic(m, flood, all_visited, tally);
    if (all_visited.empty()) {return 0;}

    // Invert the tally to negative values and clear the visited sensors set
    all_visited.clear();
    for (i=0; i<this->num_sensors; i++) {
        tally[i] = tally[i] * -1;
    }

    // Iterate each POI, in each resetting the poi-visited sensors and paths found
    for (const auto &a_poi : this->poi_sensor) {
        visited_in_poi.clear();
        paths_found = 0;

        // Enqueue each neighbor of the POI prioritizing those with more paths (smaller inverted values in the tally)
        for (const int a_sensor : a_poi.second) {
            q.push({a_sensor, tally[a_sensor]});
        }

        // Until the entire queue is consumed, pop the top element
        while (not q.empty()) {
            neigh = q.top().index;
            q.pop();

            // Find a path from the top element to the sink. If Phi is not empty, the path is valid
            get_path(neigh, tally, visited_in_poi, phi);
            if (not phi.empty()) {
                total_paths++;
                paths_found++;
                visited_in_poi = set_merge(visited_in_poi, phi);

                // If we have enough paths for the POI, break the cycle
                if (paths_found >= m) {break;}
            }
        }

        // If we got here and still to not have enough paths, return an invalid value.
        // If not, add the sensors to the solution
        if (paths_found < m) {
            all_visited.clear();
            return total_paths;
        } else {
            all_visited = set_merge(all_visited, visited_in_poi);
        }
    }

    // If we got here, we have valid connectivity! Return the total paths found
    return total_paths;
}


/** OPTIMIZER kcov_dinic
 */

long long KCMC_Instance::kcov_dinic(int k, int m, std::unordered_set<int> &solution) {

    // Prepare buffers
    int kcov, num_iterations=1;

    // Check the current coverage. If insufficient, return immediately
    solution.clear();
    kcov = this->has_coverage(k, solution);
    if (kcov < this->num_pois) {return num_iterations;}

    // Run the DINIC algorithm, ignoring its TALLY and FLOOD features
    solution.clear();
    num_iterations += dinic(m, solution);

    // Check the solution for the DINIC algorithm. If empty, the result is an error.
    if (solution.empty()) {return num_iterations;}

    // If we got here, we know the instance is capable of k-coverage,
    // AND we found the sensors required for m-connectivity.
    // Now we add sensors to the solution until it holds both properties
    // The last digits of the num_iterations value hold the number of added sensors by the add_k_cov method
    num_iterations = num_iterations * SEP_BAND;
    num_iterations += this->add_k_cov(k, solution);

    // Return the total number of iterations of the DINIC and AddKCov procedures
    return num_iterations;
}


/** OPTIMIZER reuse_dinic
 */

long long KCMC_Instance::reuse_dinic(int k, int m, std::unordered_set<int> &solution) {

    // Prepare buffers
    int kcov, num_iterations=1;

    // Check the current coverage. If insufficient, return immediately
    solution.clear();
    kcov = this->has_coverage(k, solution);
    if (kcov < this->num_pois) {return num_iterations;}

    // Run the Strongest Flow First Search (San Francisco First search) algorithm, using FLOOD=FALSE
    solution.clear();
    num_iterations += this->strongest_flow_first_search(m, false, solution);

    // Check the solution for the DINIC algorithm. If empty, the result is an error.
    if (solution.empty()) {return num_iterations;}

    // If we got here, we know the instance is capable of k-coverage,
    // AND we found the sensors required for m-connectivity.
    // Now we add sensors to the solution until it holds both properties
    // The last digits of the num_iterations value hold the number of added sensors by the add_k_cov method
    num_iterations = num_iterations * SEP_BAND;
    num_iterations += this->add_k_cov(k, solution);

    // Return the total number of iterations of the DINIC and AddKCov procedures
    return num_iterations;
}


/** OPTIMIZER flood_dinic
 */

long long KCMC_Instance::flood_dinic(int k, int m, std::unordered_set<int> &solution) {

    // Prepare buffers
    int kcov, num_iterations=1;

    // Check the current coverage. If insufficient, return immediately
    solution.clear();
    kcov = this->has_coverage(k, solution);
    if (kcov < this->num_pois) {return num_iterations;}

    // Run the Strongest Flow First Search (San Francisco First search) algorithm, using FLOOD=TRUE
    solution.clear();
    num_iterations += this->strongest_flow_first_search(m, true, solution);

    // Check the solution for the DINIC algorithm. If empty, the result is an error.
    if (solution.empty()) {return num_iterations;}

    // If we got here, we know the instance is capable of k-coverage,
    // AND we found the sensors required for m-connectivity.
    // Now we add sensors to the solution until it holds both properties
    // The last digits of the num_iterations value hold the number of added sensors by the add_k_cov method
    num_iterations = num_iterations * SEP_BAND;
    num_iterations += this->add_k_cov(k, solution);

    // Return the total number of iterations of the DINIC and AddKCov procedures
    return num_iterations;
}


/** OPTIMIZER best_dinic
 */

long long KCMC_Instance::best_dinic(int k, int m, std::unordered_set<int> &solution) {

    // Prepare buffers
    long long raw_cost=0;
    int cost_kcovd, added_kcovd, cost_reused, added_reused, cost_flodd, added_flodd, added_best;
    std::unordered_set<int> emptyset, sol_kcovd, sol_reused, sol_floodd;

    // Run each previous method
    raw_cost = kcov_dinic(k, m, sol_kcovd);
    cost_kcovd = (int) (raw_cost / SEP_BAND);
    added_kcovd = (int) (raw_cost % SEP_BAND);

    raw_cost = reuse_dinic(k, m, sol_reused);
    cost_reused = (int) (raw_cost / SEP_BAND);
    added_reused = (int) (raw_cost % SEP_BAND);

    raw_cost = flood_dinic(k, m, sol_floodd);
    cost_flodd = (int) (raw_cost / SEP_BAND);
    added_flodd = (int) (raw_cost % SEP_BAND);

    // Store the best result
    solution.clear();
    solution = set_merge(emptyset, sol_kcovd);
    added_best = added_kcovd;

    if (sol_reused.size() < solution.size()) {
        solution = set_merge(emptyset, sol_reused);
        added_best = added_reused;
    }

    if (sol_floodd.size() < solution.size()) {
        solution = set_merge(emptyset, sol_floodd);
        added_best = added_flodd;
    }

    // Return the processed cost
    return ((cost_kcovd + cost_reused + cost_flodd) * SEP_BAND) + added_best;
}
