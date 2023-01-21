

// STDLib dependencies
#include <sstream>    // ostringstream
#include <queue>      // priority_queue

// Dependencies from this package
#include "kcmc_instance.h"  // KCMC Instance class headers


/** M-Connectivity verification method
 * @param m (int) the M-Connectivity expected
 * @param flood (bool) if we should use the flood mode
 * @param visited_sensors (std::unordered_set<int>) return set of visited sensors for M-connectivity
 * @param tally (int[num_sensors]) return array of num_sensors positions for the number of paths in each sensor
 * @param quiet (bool) if we should tattle on the sensor without enough connectivity to stderr
 * @return the total number of paths found
 */
int KCMC_Instance::dinic(int m, bool flood, std::unordered_set<int> &visited_sensors, int tally[], const bool quiet) {

    // Prepare the buffers and the level vector
    int longest_path = 0, paths_found, neigh, lv[this->num_sensors], total_paths=0;
    std::unordered_set<int> visited_in_poi, phi;
    std::priority_queue<LevelNode, std::vector<LevelNode>, CompareLevelNode> q;
    visited_sensors.clear();
    std::fill(tally, tally+this->num_sensors, 0);
    this->level_vector(lv);

    // For each POI
    for (const auto &a_poi : this->poi_sensor) {
        // Reset the buffers and enqueue the POI's neighbors
        visited_in_poi.clear();
        paths_found = 0;
        while (!q.empty()) { q.pop(); }
        for (const int &neighbor : a_poi.second) { q.push({neighbor, lv[neighbor]}); }

        // Iterate while the queue is not empty or until enough paths are found
        while (!q.empty()) {
            neigh = q.top().index;
            q.pop();
            // Computes a path and makes PHI the set of its members
            get_path(neigh, lv, visited_in_poi, phi);
            // If phi is not empty we have found a path!
            if (!phi.empty()) {
                total_paths++;
                // Add the path members to the visited set, tally its votes and counts the found path
                visited_in_poi = set_merge(visited_in_poi, phi);
                paths_found++;
                for (const int &vote : phi) { tally[vote] = tally[vote]+1; }
                // If we have found the minimal number of paths
                if (paths_found >= m) {
                    // If we are in FLOOD-MODE
                    if (flood) {
                        // If we have EXACTLY M paths, update the longest path for the last time
                        if (paths_found == m) {
                            longest_path = ((phi.size() > longest_path) ? (int)(phi.size()) : longest_path);
                        }
                        // If the current path is longer than the longest we ever found, stop the loop
                        if (phi.size() > longest_path) {
                            break;
                        }
                    // If we are NOT in flood-mode, we can stop the inner loop now for we have enough paths
                    } else {
                        break;
                    }
                // If we still do not have enough paths, get the longest path found
                } else {
                    longest_path = ((phi.size() > longest_path) ? (int)(phi.size()) : longest_path);
                }
            }
        }
        // If we do not have M-Connectivity (at least M paths in the POI), invalidate the results and return
        if (paths_found < m) {
            visited_sensors.clear();
            if (not quiet) { fprintf(stderr, "\nPOI %d has insufficient connectivity - %d/%d", a_poi.first, paths_found, m); }
            return total_paths;
        }
        // If we got here, we have enough paths for this POI! Add the sensors to the global set
        visited_sensors = set_merge(visited_sensors, visited_in_poi);
    }
    // Success in every single POI! Return the total number of paths found.
    return total_paths;
}
int KCMC_Instance::dinic(int m, bool flood, std::unordered_set<int> &visited_sensors, int tally[]) {
    return this->dinic(m, flood, visited_sensors, tally, true);
}
int KCMC_Instance::dinic(int m, std::unordered_set<int> &visited_sensors) {
    int tally[this->num_sensors];
    return this->dinic(m, false, visited_sensors, tally, true);
}


/** Sets the lowest distance in hops from each sensor to the sink
 * Returns the maximum level found
 */
int KCMC_Instance::level_vector(int lv[]) {

    // Prepare the FIFO queue buffer
    int head, maxlevel = 0;
    std::queue<int> q;

    // Fill the vector with the infinity value
    std::fill(lv, lv+this->num_sensors, INFTY);

    // Get the set of active neighbors of sinks. Set each neighbor's level to 0
    for (const auto &a_sink : this->sink_sensor) {
       for (const int &neighbor : a_sink.second) {
           lv[neighbor] = 0;
           q.push(neighbor);
       }
    }

    // While there are still sensors to visit, find and visit them and set their level
    while (!q.empty()) {

        // Pop the topmost sensor in the queue
        head = q.front();
        q.pop();

        // For each of its neighbors whose distance has not yet been measured
        for (const int &neigh : this->sensor_sensor[head]) {
            if (lv[neigh] == INFTY) {
                // Set the distance to the neighbor equal to the distance of its source + 1
                lv[neigh] = lv[head] + 1;
                // Enqueue the neighbor
                q.push(neigh);
                // Update the max level found
                maxlevel = ((lv[neigh] > maxlevel) ? lv[neigh] : maxlevel);
            }
        }
    }

    // Return the max level found
    return maxlevel;
}


/* Unravels the Predecessors found using get_path and other A*-derived pathfinding algorithms
 * Can be easily modified to return a sequence instead of a set
 * The no-predecessor value must be INFTY or larger
 */
int unravel_predecessors(int head, const int predecessors[], std::unordered_set<int> &members) {
    members.clear();
    do {
        members.insert(head);
        head = predecessors[head];
    } while (head < INFTY);
    return members.size();
}


int KCMC_Instance::get_path(int origin, int lv[], std::unordered_set<int> &visited, std::unordered_set<int> &phi) {

    // Prepare the buffers
    int predecessors[this->num_sensors], head, num_pops=0;
    std::priority_queue<LevelNode, std::vector<LevelNode>, CompareLevelNode> q;
    phi.clear();
    std::unordered_set<int> enqueued;
    enqueued.insert(origin);

    // Fill the predecessors vector with the infinity value
    std::fill(predecessors, predecessors+this->num_sensors, INFTY);

    // Get the set of sink neighbors. Return phi as an empty set if there are none
    std::unordered_set<int> sink_neighbors;
    for (const int &neigh : this->sink_sensor[0]) {  // HARD-CODED ASSUMPTION OF A SINGLE SINK
        if (not isin(visited, neigh)) {
            sink_neighbors.insert(neigh);
        }
    }
    if (sink_neighbors.empty()) {
        return 0;
    }

    // Return now if the origin is directly adjacent to the sink
    if (isin(sink_neighbors, origin)) {
        phi.insert(origin);
        return num_pops;  // The number of times a value was popped from the queue
    }

    // Add the non-visited neighbors of the origin vertice to the queue
    // Set their predecessors as the orign
    for (const int &neigh : this->sensor_sensor[origin]) {
        if (not isin(visited, neigh)) {
            predecessors[neigh] = origin;
            q.push({neigh, lv[neigh]});
            enqueued.insert(neigh);
        }
    }

    // Iterate while the queue is not empty, adding a new layer of neighbors to the queue
    // Since the queue is prioritized by the lowest level, it will always go in the right direction
    while (not q.empty()) {
        head = q.top().index;
        q.pop();
        num_pops++;  // The number of times a value was popped from the queue

        // If the popped head is adjacent to the sink, we arrived!
        if (isin(sink_neighbors, head)) {
            unravel_predecessors(head, predecessors, phi);
            return num_pops;
        }

        // If not, iterate each non-visited not-yet-enqueued neighbor of the popped head and add it to the queue
        // The head is set as its predecessor
        for (const int &neigh : this->sensor_sensor[head]) {
            if (not isin(visited, neigh)) {
                if (not isin(enqueued, neigh)) {
                    predecessors[neigh] = head;
                    q.push({neigh, lv[neigh]});
                    enqueued.insert(neigh);
                }
            }
        }
    }
    // If phi is empty, failure :(
    phi.clear();
    return num_pops;
}
