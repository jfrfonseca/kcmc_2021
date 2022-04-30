/** RANDOM PLACEMENT PRINTER
 * Uses the same method to generate random placements from a seed, and print it to STDOUT
 */

#include <random>     // mt19937, uniform_real_distribution
#include <iostream>  // cin, cout, endl


void help(int argc, char* const argv[]) {
    std::cout << "RECEIVED LINE (" << argc << "): ";
    for (int i=0; i<argc; i++) {std::cout << argv[i] << " ";}
    std::cout << std::endl;
    std::cout << "Please, use the correct input for the KCMC instance node placements visualizer:" << std::endl << std::endl;
    std::cout << "./placements_visualizer <p> <s> <k> <area_s> <seed>" << std::endl;
    std::cout << "  where:" << std::endl << std::endl;
    std::cout << "p > 0 is the number of POIs to be randomly generated" << std::endl;
    std::cout << "s > 0 is the number of Sensors to be generated" << std::endl;
    std::cout << "k > 0 is the number of Sinks to be generated. If n=1, the sink will be placed at the center of the area" << std::endl;
    std::cout << "seed is an integer number that is uded as seed of the PRNG." << std::endl;
    std::cout << "p{x} identifies POI x" << std::endl;
    std::cout << "i{y} identifies SENSOR y" << std::endl;
    std::cout << "s{z} identifies SINK z" << std::endl;
    exit(0);
}


int main(int argc, char* const argv[]) {
    if (argc < 5) { help(argc, argv); }

    /* Parse CMD SETTINGS */
    long long random_seed;
    int i, num_pois, num_sensors, num_sinks, area_side;
    num_pois    = atoi(argv[1]);
    num_sensors = atoi(argv[2]);
    num_sinks   = atoi(argv[3]);
    area_side   = atoi(argv[4]);
    random_seed = atoll(argv[5]);

    // Print POIs and sensors
    std::mt19937 gen(random_seed);
    std::uniform_real_distribution<> point(0, area_side);
    printf("id,x,y\n");
    for (i=0; i<num_pois; i++) {printf("p%d,%d,%d\n", i, (int)(point(gen)), (int)(point(gen)));}
    for (i=0; i<num_sensors; i++) {printf("i%d,%d,%d\n", i, (int)(point(gen)), (int)(point(gen)));}

    // Set the SINKs buffers (if there is a single sink, it will be at the center of the area)
    if (num_sinks == 1) {printf("s0,%d,%d\n", (int)(area_side / 2.0), (int)(area_side / 2.0));}
    else {for (i=0; i<num_sinks; i++) {printf("s%d,%d,%d\n", i, (int)(point(gen)), (int)(point(gen)));}}
}
