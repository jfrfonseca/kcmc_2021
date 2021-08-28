

#include <unordered_map>
#include <unordered_set>

#ifndef KCMC_COMPONENTS_H
#define KCMC_COMPONENTS_H

#define MAX_POIS 1000
#define MAX_SENSORS 1000
#define MAX_SINKS 100

#define NULL_SENSOR 3*MAX_SENSORS


struct COMPONENT {
    int x;
    int y;
};


class KCMC_Instance {

    public:
        bool splitted;
        int num_pois, num_sensors, num_sinks, area_side, sensor_coverage_radius, sensor_communication_radius;
        long long random_seed;
        std::unordered_map<int, std::unordered_set<int>> poi_sensor, sensor_poi, sensor_sensor, sink_sensor, sensor_sink;
        std::unordered_map<int, std::unordered_set<int>> split_sensors;

        KCMC_Instance(int num_pois, int num_sensors, int num_sinks,
                      int area_side, int coverage_radius, int communication_radius,
                      long long random_seed);
        explicit KCMC_Instance(const std::string& serialized_kcmc_instance);
    
        std::string key() const;
        std::string serialize();
    
        std::string k_coverage(int k);
        std::string m_connectivity(int m);
    
    private:
        static void push(std::unordered_map<int, std::unordered_set<int>> &buffer, int source, int target);
        int parse_edge(int stage, const std::string& token);
        void split_all_sensors();
};

#endif