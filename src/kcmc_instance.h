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
#define INSPECTION_FREQUENCY 100
#define WORST_FITNESS 9999999999


/* NODE
 * Basic building block of the KCMC Instance. Contains its type (poi, sensor, sink), index (in array) and dinic level
 * LevelNodes can be compared in function of their level.
 * Placement is a buffer for positioning nodes when generating a random instance.
 * The euclidean distance between two placements can also be computed in function of its X and Y coordinates.
 */


struct Node {
    int nodetype;
    int index;
};

struct LevelNode {
    int index;
    int level;
};

struct CompareLevelNode {
    bool operator()(LevelNode const& a, LevelNode const& b) {
        // return "true" if "a" is ordered before "b"
        if (a.level == b.level){return (a.index <= b.index);}  // INCREASING order, if at the same level
        else {return (a.level > b.level);}  // DECREASING order
    }
};


struct Placement {
    Node *node;
    int x, y;
};

double distance(Placement source, Placement target);


/* ISIN
 * Many-types-of-input verification if a given item is in the reference set.
 * If the reference set is a mapping, the search is in its keys.
 */


bool isin(std::unordered_map<int, std::unordered_set<int>> &ref, int item);
bool isin(std::unordered_map<int, int> &ref, int item);
bool isin(std::unordered_set<int> &ref, int item);
bool isin(std::unordered_set<std::string> &ref, const std::string &item);
bool isin(std::vector<int> &ref, int item);
bool isin(std::vector<int> *ref, int item);


/* PUSH AND VOTE
 * PUSH: Adds a new pair in a mapping, from the source to a set containing only the target.
 *       If the source is already in the set, the target is added to its mapped data.
 * VOTE: Adds an element to a map, pointing to the value 1.
 *       It the source element is alreary in the map, increase the value of its mapped data by 1.
 */
void push(std::unordered_map<int, std::unordered_set<int>> &buffer, int source, int target);
void vote(std::unordered_map<int, int> &buffer, int target, int value);
void vote(std::unordered_map<int, int> &buffer, int target);


/* SET MERGE & DIFF
 * Returns the set that is the sum (or difference) of the given sets
 */
template<class T>
T set_merge (T a, T b) {T t(a); t.insert(b.begin(),b.end()); return t;}
std::unordered_set<int> set_diff(const std::unordered_set<int> &left, const std::unordered_set<int> &right);


/* SETIFY
 * Returns a set from other data structure
 */
void setify(std::unordered_set<int> &target, int size, int source[], int reference);
void setify(std::unordered_set<int> &target, std::unordered_map<int, int> *reference);


// #####################################################################################################################


/** KCMC Instance Object
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
        std::unordered_map<int, std::unordered_set<int>> poi_sensor, sensor_poi, sensor_sensor, sensor_sink, sink_sensor;

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
         * Serialize the current instance as a string
         * Invert a set of sensors (get every sensor in the instance not in the set)
         * Validate the instance, raising errors if invalid. Some arguments are optional
         */
        std::string key() const;
        std::string serialize();
        int invert_set(std::unordered_set<int> &source_set, std::unordered_set<int> *target_set);
        bool validate(bool raise, int k, int m);
        bool validate(bool raise, int k, int m, std::unordered_set<int> &inactive_sensors);
        bool validate(bool raise, int k, int m, std::unordered_set<int> &inactive_sensors,
                      std::unordered_set<int> *k_used_sensors,
                      std::unordered_set<int> *m_used_sensors);

        /* Instance problem-specific methods
         * Get the Degree of each Sensor in the instance
         * Get the Coverage of each POI in the instance
         * Get the Connectivity of each POI in the instance
         */
        int get_degree(int buffer[], std::unordered_set<int> &inactive_sensors);
        int get_coverage(int buffer[], std::unordered_set<int> &inactive_sensors);
        int get_connectivity(int buffer[], std::unordered_set<int> &inactive_sensors, int target);
        int get_connectivity(int buffer[], std::unordered_set<int> &inactive_sensors);

        /* Instance payload services
         * Validates k-coverage in the instance considering the given set of inactive sensors
         * Validates m-connectivity in the instance considering the given set of inactive sensors
         */
        int fast_k_coverage(int k, std::unordered_set<int> &inactive_sensors);
        int fast_k_coverage(int k, std::unordered_set<int> &inactive_sensors, std::unordered_set<int> *all_used_sensors);
        std::string k_coverage(int k, std::unordered_set<int> &inactive_sensors);
        int fast_m_connectivity(int m, std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *all_used_sensors);
        int fast_m_connectivity(int m, std::unordered_set<int> &inactive_sensors, std::unordered_set<int> *all_used_sensors);
        std::string m_connectivity(int m, std::unordered_set<int> &inactive_sensors);

        /* Instance Preprocessors
         * Local Optima yelds ony the sensors required to validate the instance using Dinic's algorithm (limited)
         * Flood finds all parallel paths from the dinic paths required in the instance.
         *   The Minimal flood does it only for the required dinic paths. Full flood keeps on adding paths to the
         *     minimal requirements until paths start to increase, so it has way more sensors.
         * Reuse uses the full-flood to get paths. Each path votes on all its composing sensors. Then, new paths are
         *   created preferring the most voted sensors in each dinic level.
         */
        int local_optima(int k, int m, std::unordered_set<int> &inactive_sensors, std::unordered_set<int> *all_used_sensors);
        int flood(int k, int m, bool full, std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *visited_sensors);
        int reuse(int k, int m, int flood_level, std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *visited_sensors);
        int reuse(int k, int m, std::unordered_set<int> &inactive_sensors, std::unordered_map<int, int> *visited_sensors);

    private:
        void regenerate();
        int parse_edge(int stage, const std::string& token);
        int level_graph(int level_graph[], std::unordered_set<int> &inactive_sensors);
        int find_path(int poi_number, std::unordered_set<int> &used_sensors,
                      int level_graph[], int predecessors[]);
};

#endif
