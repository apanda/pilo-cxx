#include <iostream>
#include <list>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <boost/random/uniform_int.hpp>
#include <yaml-cpp/yaml.h>
#include "context.h"
#include "simulation.h"
#include "link.h"
#include "node.h"
#include "distributions.h"
#include "packet.h"

namespace po = boost::program_options;
typedef std::unordered_map<std::string, PILO::Node> node_map;
typedef std::unordered_map<std::string, PILO::Link> link_map;

int main(int argc, char* argv[]) {
    std::string topology;
    std::string configuration;
    uint32_t seed;
    
    // Argument parsing
    po::options_description args("PILO simulation");
    args.add_options()
        ("help,h", "Display help")
        ("topology,t", po::value<std::string>(&topology),
           "Simulation topology")
        ("seed,s", po::value<uint32_t>(&seed)->default_value(42),
            "Random seed")
        ("configuration,c", po::value<std::string>(&configuration),
           "Simulation parameters");
    po::variables_map vmap;
    po::store(po::command_line_parser(argc, argv).options(args).run(), vmap);
    po::notify(vmap);
    if (vmap.count("help")) {
        std::cerr << args << std::endl;
        return 0;
    }

    if (!vmap.count("topology")) {
        std::cerr << "Topology not specified" << std::endl;
        return 0;
    }

    if (!vmap.count("configuration")) {
        std::cerr << "Configuration not specified" << std::endl;
        return 0;
    }

    PILO::Simulation simulation(seed, configuration, topology);
    simulation.set_all_links_up_silent();
    simulation.install_all_routes();
    std::cout << "Pre run check = " << simulation.check_routes() << std::endl;
    simulation.run();
    return 1;
}
