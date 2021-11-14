/** KCMC_INSTANCE.h
 * Header of the KCMC instance object
 * Jose F. R. Fonseca
 */


// STDLib dependencies
#include <vector>         // vector object
#include <unordered_set>  // unordered_set object
#include <unordered_map>  // unordered_map HashMap object
#include <cmath>          // sqrt, pow


#ifndef KCMC_INSTANCE_H
#define KCMC_INSTANCE_H

#define tPOI 0
#define tSENSOR 1
#define tSINK 2


/** NODE
 * Basic building block of the KCMC Instance. Contains its type (poi, sensor, sink), index (in array) and dinic level
 * Placement is a buffer for positioning nodes when generating a random instance
 */


struct Node {
    int nodetype;
    int index;
    int level;
};

struct Placement {
    Node *node;
    int x, y;
};


/* SET MERGE
 * Returns the set that is the sum of the given sets
 */
template<class T>
T set_merge (T a, T b) {
  T t(a); t.insert(b.begin(),b.end()); return t;
}


/** KCMC Instance
 * Contains node vectors for pois, sensors, sinks.
 * Vector of unordered sets listing the neighbors of each poi, sensor and sink
 */


class KCMC_Instance {

    public:
        /* Descriptor INTEGER constants
         * Quantity of POIs, sensors, sinks.
         * Dispersion area of the instance
         * Sensor coverage and communication radius
         * Random seed for component placement
         */
        int num_pois, num_sensors, num_sinks, area_side, sensor_coverage_radius, sensor_communication_radius;
        long long random_seed;

        /* Component buffers
         * Separate Node Vectors for pois, sensors and sinks
         * Each vector contains a node of index to its position in the vector
         */
        std::vector<Node> poi, sensor, sink;

        /* Graph edges sparse matrix
         * Separate HashMaps of UnorderedSet of indexes of Nodes for poi-sensor, sensor-sensor and sensor-sink edges.
         * Each HashMap contains an UnorderedSet of indexes of Nodes. The index in the HashMap is relative to the
         *     index of the base Node it represents. The content of the HashMap is an unordered set of the indexes of
         *     the Nodes that the index node is neighbor of.
         * There are no Poi-Poi, Poi-Sink nor Sink-Sink edges
         */
        std::unordered_map<int, std::unordered_set<int>> poi_sensor, sensor_sensor, sensor_sink, sink_sensor;

        /* Random-instance generator constructor
         * Receives the instance descriptive constants and makes an instance of randomly-placed Nodes.
         */
        KCMC_Instance(int num_pois, int num_sensors, int num_sinks,
                      int area_side, int coverage_radius, int communication_radius,
                      long long random_seed);

        /* Instance de-serializes constructor
         * Receives a serialized instance and constructs an KCMC_Instance object from it.
         */
        explicit KCMC_Instance(const std::string& serialized_kcmc_instance);

        /* Instance basic services
         * Get the KEY of the current instance
         * Serialize the current instance as an string
         */
        std::string key() const;
        std::string serialize();

        /* Instance payload services
         * Validates k-coverage in the instance considering the given set of inactive sensors
         * Validates m-connectivity in the instance considering the given set of inactive sensors
         */
        std::string k_coverage(int k, std::unordered_set<int> &inactive_sensors);
        std::string m_connectivity(int m, std::unordered_set<int> &inactive_sensors);

        /* STATIC SERVICES
         *
         */
        static bool isin(const std::unordered_map<int, std::unordered_set<int>> &ref, int item);
        static bool isin(const std::unordered_set<int> &ref, int item);
        static bool isin(std::unordered_set<std::string> ref, const std::string &item);
        static std::unordered_set<int> set_diff(const std::unordered_set<int> &left, const std::unordered_set<int> &right);
        static void push(std::unordered_map<int, std::unordered_set<int>> &buffer, int source, int target);

    private:
        int parse_edge(int stage, const std::string& token);
        std::unordered_set<int> bfs(std::unordered_set<int> &seed_sensors, std::unordered_set<int> &inactive_sensors);
        int level_graph(int level_graph[], std::unordered_set<int> &inactive_sensors);
};

#endif
