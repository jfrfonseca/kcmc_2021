

// STDLib dependencies
#include <queue>      // queue

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** Helper Functions
 * Functions that are not optimizers themselves, but are used by optimizer
 * invert_sensor_set gets a set of sensors (i.e. all inacitive sensors) and returns the set of sensors in the instance that are *not* in the set
 * add_k_cov adds sensors to a set until the set is enough to hold the property of k-coverage
 * strongest_flow_first_search is a search that prioritizes sensors that are part of many DINIC paths to the sink
 */


void KCMC_Instance::invert_sensor_set(std::unordered_set<int> &source_set, std::unordered_set<int> &target_set) {
    target_set.clear();
    for (int i=0; i<this->num_sensors; i++) {
        if (not isin(source_set, i)) {
            target_set.insert(i);
        }
    }
}


int KCMC_Instance::add_k_cov(int k, std::unordered_set<int> &active_sensors) {

    // Prepare buffers
    int kcov, num_added_sensors=0, best_ic, best_kcov, ic_kcov;
    std::unordered_set<int> ignored, candidates, setminus;
    this->invert_sensor_set(active_sensors, ignored);

    // Check if we already have enough coverage. If we do, return immediately
    kcov = this->has_coverage(k, ignored);
    if (kcov >= this->num_pois) {return num_added_sensors;}

    // If we do not have enough coverage, get all the POI-Covering sensors not already used
    for (const auto &a_poi : this->poi_sensor) {
        for (const int ic_sensor : a_poi.second) {
            if (not isin(active_sensors, ic_sensor)) {
                candidates.insert(ic_sensor);
            }
        }
    }

    // While we still to not have enough coverage
    while (kcov < this->num_pois) {
        best_kcov = kcov;

        // For each POI-covering sensor not already used
        for (const int ic_sensor : candidates) {

            // Checks if the ic_sensor, if included, would improve coverage
            setminus = set_diff(ignored, {ic_sensor});
            ic_kcov = this->has_coverage(k, setminus);

            // If it does improve coverage over the current best, mark it as the best
            if (ic_kcov > best_kcov) {
                best_ic = ic_sensor;
                best_kcov = ic_kcov;
            }
        }

        // Add the best selected POI-covering unused sensor to the set of used sensors
        // and remove it from the set of candidates.
        active_sensors.insert(best_ic);
        num_added_sensors++;
        ignored = set_diff(ignored, {best_ic});
        candidates = set_diff(candidates, {best_ic});

        // Store the best coverage as the current coverage
        kcov = best_kcov;
    }

    // Return the number of added sensors to the result
    return num_added_sensors;
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
