
// STDLib dependencies
#include <sstream> // ostringstream
#include <queue>   // queue
#include <cstring> // memset
#include <list>    // list
#include <utility>


// Dependencies from this package
#include "kcmc_components.h"


/* #####################################################################################################################
 * BINOMIAL HEAP
 * Data structure appropriate for the Priority Queue used in Dinic's Algorithm.
 * Fibonacci heaps may be canonically faster, but only if too many decrease-key operations are performed.
 */


struct BHNode {
    int data, degree;
    BHNode *follower, *colleague, *previous;
};


BHNode* new_BHNode(int key) {
    auto *spawn = new BHNode;
    spawn->data = key;
    spawn->degree = 0;
    spawn->follower = spawn->previous = spawn->colleague = nullptr;
    return spawn;
}


BHNode* merge_BHTree(BHNode *small_tree, BHNode *large_tree) {

    // Make sure the smaller tree is really so
    if (small_tree->data > large_tree->data) {std::swap(small_tree, large_tree);}

    // We basically make larger valued tree
    // a follower of smaller valued tree
    large_tree->previous = small_tree;
    large_tree->colleague = small_tree->follower;
    small_tree->follower = large_tree;
    small_tree->degree++;

    return small_tree;
}


std::list<BHNode*> union_BHeap(std::list<BHNode*> BHeap_a, std::list<BHNode*> BHeap_b) {

    // Prepare buffers
    std::list<BHNode*> united;

    // Prepare two iterators to transverse the heaps
    auto it_a = BHeap_a.begin();
    auto it_b = BHeap_b.begin();

    // While not done transversing both heaps
    while ((it_a != BHeap_a.end()) && (it_b != BHeap_b.end())) {

        // Push the smallest element at the end of the new BHeap and advance its iterator
        if (((*it_a)->degree) <= ((*it_b)->degree)) {
            united.push_back(*it_a);
            it_a++;
        } else {
            united.push_back(*it_b);
            it_b++;
        }
    }

    // Add the (possible) remaining elements of (one of the two) BHeap at the end of the united one
    while (it_a != BHeap_a.end()) {
        united.push_back(*it_a);
        it_a++;
    }
    while (it_b != BHeap_b.end()) {
        united.push_back(*it_b);
        it_b++;
    }

    // Return the united heap
    return united;
}


std::list<BHNode*> heapify(std::list<BHNode*> BHeap) {

    // If the heap has only one element, return the "entire" heap
    if (BHeap.size() <= 1) return BHeap;

    // Prepare buffers
    std::list<BHNode*> new_heap;
    std::list<BHNode*>::iterator it_current, it_next, next_next;
    it_current = it_next = next_next = BHeap.begin();

    // Set the iterators regarding the size of the heap
    if (BHeap.size() == 2) {
        it_next = it_current;
        it_next++;
        next_next = BHeap.end();  // This iterator is already done
    } else {
        it_next++;
        next_next = it_next;
        next_next++;
    }

    // While the heap was not entirely transversed
    while (it_current != BHeap.end()) {

        // If we're at the end of the heap, simply increase the current-position iterator
        if (it_next == BHeap.end()) {it_current++;}

        // if the current has a smaller value than the next, then it's already ordered, and we can update the iterators
        else if ((*it_current)->degree < (*it_next)->degree) {
            it_current++;
            it_next++;
            if(next_next != BHeap.end()) {next_next++;}
        }

        // If the current has an equal value to the next
        else if ((*it_current)->degree == (*it_next)->degree) {

            // If there is an next next and the current is also equal to it, just update the pointers for the BHeap is
            // already ordered
            if ((next_next != BHeap.end()) && ((*it_current)->degree == (*next_next)->degree)) {
                it_current++;
                it_next++;
                next_next++;

            // If not, the BHeap is not ordered. We must merge the current with the next and make it the new current
            } else {
                *it_current = merge_BHTree(*it_current, *it_next);
                it_next = BHeap.erase(it_next);
                if(next_next != BHeap.end()) {next_next++;}
            }
        }
    }

    // Return the now-ordered BHeap
    return BHeap;
}


std::list<BHNode*> insert_BHTree(std::list<BHNode*> BHeap, BHNode *BHTree) {
    std::list<BHNode*> new_BHeap;
    new_BHeap.push_back(BHTree);
    return heapify(union_BHeap(std::move(BHeap), new_BHeap));
}


std::list<BHNode*> remove_minimal_from_BHtree(BHNode *BHTree) {

    // Get the current pointer as the next-to-the-first element in the BHTree
    BHNode *current = BHTree->follower, *next;

    // Insert all following nodes into a new heap, and return it
    std::list<BHNode*> new_heap;
    while (current) {
        next = current;
        current = current->colleague;
        next->colleague = nullptr;
        new_heap.push_front(next);
    }
    return new_heap;
}


std::list<BHNode*> insert(std::list<BHNode*> BHeap, int key) {return insert_BHTree(std::move(BHeap), new_BHNode(key));}


BHNode* get_minimal_element(std::list<BHNode*> BHeap) {
    // Transverse the original heap, storing returning its minimal element
    auto it = BHeap.begin();
    BHNode *minimal = *it;
    while (it != BHeap.end()) {
        if ((*it)->data < minimal->data) {minimal = *it;}
        it++;
    }
    return minimal;
}


std::list<BHNode*> extract_min(std::list<BHNode*> BHeap) {

    // Get the minimal element from the BHeap. IT MIGHT BE AN ENTIRE TREE!
    BHNode *minimal = get_minimal_element(BHeap);

    // Create a new Bheap and transverse the old BHeap, inserting in the new all but the minimal value
    std::list<BHNode*> new_heap;
    auto it = BHeap.begin();
    while (it != BHeap.end()) {
        if (*it != minimal) {new_heap.push_back(*it);}
        it++;
    }

    // Remove the minimal element from the tree and union it with the new, minimal-missing heap. Sort the results
    return heapify(union_BHeap(new_heap, remove_minimal_from_BHtree(minimal)));
}


/* #####################################################################################################################
 * M-CONNECTIVITY VALIDATOR USING DINIC'S ALGORITHM
 */


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



/* #####################################################################################################################
 * M-CONNECTIVITY VALIDATOR USING THE FORD-FULKERSON ALGORITHM
 */


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


std::string KCMC_Instance::m_connectivity_ford_fulkerson(const int m) {
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
