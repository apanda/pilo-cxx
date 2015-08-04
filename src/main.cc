#include <iostream>
#include <list>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <boost/random/uniform_int.hpp>
#include <yaml-cpp/yaml.h>
#include "context.h"
#include "link.h"
#include "node.h"
#include "distributions.h"
#include "packet.h"

namespace po = boost::program_options;
typedef std::unordered_map<std::string, PILO::Node> node_map;
typedef std::unordered_map<std::string, PILO::Link> link_map;

namespace {
    using namespace PILO;
    const std::string LINKS_KEY = "links";
    const std::string FAIL_KEY = "fail_links";
    const std::string CRIT_KEY = "crit_links";
    const std::string RUNFILE_KEY = "runfile";
    const std::string TYPE_KEY = "type";
    const std::string ARG_KEY = "args";
    const std::string HOST_TYPE = "Host";
    const std::string CONTROLLER_TYPE = "Control";
    const std::string SWITCH_TYPE = "Switch";

    node_map populate_nodes(Context& context, const YAML::Node& topology) {
        node_map nodeMap;
        for (auto& node : topology) {
            std::string node_str = node.first.as<std::string>();
            if (node_str == LINKS_KEY ||
                node_str == FAIL_KEY ||
                node_str == RUNFILE_KEY || 
                node_str == CRIT_KEY) {
                continue; // Not a node we want
            }
            nodeMap.emplace(std::make_pair(node_str, Node(context, node_str)));
        }
        return nodeMap;
    }

    link_map populate_links(Context& context, node_map& nodes, 
            Distribution<Time>* latency, BPS bw, const YAML::Node& topology) {
        link_map linkMap;
        for (auto link : topology[LINKS_KEY]) {
            std::string link_str = link.as<std::string>();
            std::vector<std::string> parts;
            boost::split(parts, link_str, boost::is_any_of("-"));
            linkMap.emplace(std::make_pair(link_str, 
                        Link(context, latency, bw, nodes.at(parts[0]), nodes.at(parts[1]))));
        }
        return linkMap;
    }

    void run(Context& context) {
        while(context.next());
    }
}

int main(int argc, char* argv[]) {
    std::string topology;
    std::string configuration;
    uint32_t seed;
    boost::mt19937 rng;

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

    rng.seed(seed);

    PILO::Context context;

    YAML::Node yaml_configuration = YAML::LoadFile(configuration);
    YAML::Node yaml_topology = YAML::LoadFile(topology);

    Distribution<PILO::Time> *latency = 
        Distribution<PILO::Time>::get_distribution(yaml_configuration["data_link_latency"], rng); 

    auto node_map = populate_nodes(context, yaml_topology);
    auto link_map = populate_links(context, node_map, latency, 10000000000., yaml_topology);

    boost::variate_generator<boost::mt19937&, boost::uniform_int<>> var(rng, boost::uniform_int<>(0, link_map.size())); 

    // Random send, delete when not testing
    auto random_it = std::next(std::begin(link_map), var());
    std::cout<< "Selected link " <<  random_it->first << std::endl;
    auto link = random_it->second;
    std::cout<< "Selected node " <<  link._a._name << std::endl;
    std::shared_ptr<PILO::Packet> packet(new PILO::Packet(link._a._name, link._b._name, PILO::Packet::NOP, 100000000000));
    link.send(node_map.at(link._a._name), std::move(packet));

    int count = 0;

    PILO::Task task;
    task = [&](PILO::Time time) {
        if (count < 5) {
            std::cout << "Time is " << time << std::endl;
            context.schedule(1, task);
            count++;
        } else {
            std::cout << "Time is " << time << std::endl;
        }
    };
    context.schedule(1, task);
    run(context);
    return 1;
}
