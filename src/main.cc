#include <iostream>
#include <list>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <yaml-cpp/yaml.h>
#include "context.h"
#include "link.h"
#include "node.h"

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
            Time latency, BPS bw, const YAML::Node& topology) {
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

    po::options_description args("PILO simulation");
    args.add_options()
        ("help,h", "Display help")
        ("topology,t", po::value<std::string>(&topology),
           "Simulation topology")
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

    PILO::Context context;

    YAML::Node yaml_configuration = YAML::LoadFile(configuration);
    PILO::Time latency = yaml_configuration["data_link_latency"]["mean"].as<PILO::Time>();

    YAML::Node yaml_topology = YAML::LoadFile(topology);
    std::cout << "Latency = " << latency << std::endl;

    auto node_map = populate_nodes(context, yaml_topology);
    auto link_map = populate_links(context, node_map, latency, 10000000000., yaml_topology);

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
