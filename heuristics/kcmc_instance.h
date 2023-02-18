/** KCMC_INSTANCE.h
 * Header of the KCMC instance object
 * Jose F. R. Fonseca
 */


// STDLib dependencies
#include <vector>         // vector object
#include <unordered_set>  // unordered_set object
#include <unordered_map>  // unordered_map HashMap object
#include <cmath>          // sqrt, pow
#include <iostream>       // cin, cout, endl
#include <iomanip>        // std::setprecision


#ifndef KCMC_INSTANCE_H
#define KCMC_INSTANCE_H

#define tPOI 0
#define tSENSOR 1
#define tSINK 2
#define INFTY 999999
#define SEP_BAND 10000


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

        /* Set of sensors that were active at the moment the instance was created
         */
        std::unordered_set<int> active_sensors;

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
                      long long random_seed, std::unordered_set<int> active_sensors);
        KCMC_Instance(int num_pois, int num_sensors, int num_sinks,
                      int area_side, int coverage_radius, int communication_radius,
                      long long random_seed);

        /* Instance Serializer
         * key() returns the seed information to generete the instance (arguments for the constructor)
         * serialize() returns a string mapping every edge in the instance.
         */
        std::string key() const;
        std::string serialize();

        /* Instance de-serialize constructor
         * Receives a serialized instance and constructs an KCMC_Instance object from it.
         * May receive a limited set of active sensors. If not provided, all sensors are considered active
         */
        explicit KCMC_Instance(const std::string& serialized_kcmc_instance, std::unordered_set<int> active_sensors);
        explicit KCMC_Instance(const std::string& serialized_kcmc_instance);

        /* Instance component placement getter
         * Gets the placements of every POI, sensor and sink in the instance.
         * Used in the contructor and by visualizers
         */
        void get_placements(Placement *pl_pois, Placement *pl_sensors, Placement *pl_sinks);

        /* COVERAGE VALIDATION
         * Sums the coverage in each POI
         * If any POI has coverage less than K, the total result will be negative
         * Prints to SDERR the POI with insufficient coverage
         * Returns the total coverage, inverted if any POI has coverage less than K
         */
        int has_coverage(int k, std::unordered_set<int> &inactive_sensors);
        int has_coverage(int k, std::unordered_set<int> &inactive_sensors, bool quiet);

        /* CONNECTIVITY VALIDATION
         *
         */
        int dinic(int m, std::unordered_set<int> &visited_sensors);
        int dinic(int m, bool flood, std::unordered_set<int> &visited_sensors, int tally[]);
        int dinic(int m, bool flood, std::unordered_set<int> &visited_sensors, int tally[], bool quiet);

        /** OPTIMIZATION
         * KCOV-DINIC
         * REUSE-DINIC
         */
         long long kcov_dinic(int k, int m, std::unordered_set<int> &solution);
         long long reuse_dinic(int k, int m, std::unordered_set<int> &solution);
         long long flood_dinic(int k, int m, std::unordered_set<int> &solution);
         long long best_dinic(int k, int m, std::unordered_set<int> &solution);

    private:

        /* Internal support methods for the get_placements method
         * get_placements is a private version of a public method of the same name. This one updates the intance
         * regenerate computes every edge in the instance based on its placements.
         * Ignore sensors not in the set of active sensors
         */
        void get_placements(Placement *pl_pois, Placement *pl_sensors, Placement *pl_sinks, bool push);
        void regenerate();

        /* Internal support methods for the de-serializer constructor
         * parse_edge reads a serialized edge into the contructor ignoring edges with sensors not in the set of active sensors
         */
        int parse_edge(int stage, const std::string& token);

        /* Internal support methods for the dinic method
         * level_vector fills a vector the distance in hops from each sensor to the sink
         * get_path finds a path between two vertices
         */
        int level_vector(int level_vector[]);
        int get_path(int origin, int lv[], std::unordered_set<int> &visited, std::unordered_set<int> &phi);

        /* Internal support methods for the optimization methods
         * invert_sensor_set gets a set of sensors and prepares a set of all sensors not in it
         * add_k_cov adds sensors to a set until the set has K-Coverage
         * strongest_flow_first_search (or "San Francisco First" search) is like DINIC, but prioritizes sensors that are part of many paths
         */
        void invert_sensor_set(std::unordered_set<int> &source_set, std::unordered_set<int> &target_set);
        int add_k_cov(int k, std::unordered_set<int> &included_sensors);
        int strongest_flow_first_search(int m, bool flood, std::unordered_set<int> &all_visited);

};

void print_tikz(KCMC_Instance *instance, double width, bool only_active);
void print_tikz(KCMC_Instance *instance, double width);

bool validate_kcmc_instance(KCMC_Instance *instance, int k, int m, std::unordered_set<int> active_sensors);

#endif
