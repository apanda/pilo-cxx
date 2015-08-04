#include "simulation.h"
namespace PILO {    
    Simulation::Simulation(const uint32_t seed, const std::string& configuration, const std::string& topology) :
        _seed(seed),
        _rng(_seed),
        _configuration(YAML::LoadFile(configuration)),
        _topology(YAML::LoadFile(topology)),
        _latency(Distribution<PILO::Time>::get_distribution(_configuration["data_link_latency"], _rng)),
        _nodes(std::move(populate_nodes())),
        _links(std::move(populate_links(10000000000))),
        _linkRng(0, _links.size(), _rng),
        _nodeRng(0, _nodes.size(), _rng) {
    }

    Simulation::node_map Simulation::populate_nodes() {
        node_map nodeMap;
        for (auto& node : _topology) {
            std::string node_str = node.first.as<std::string>();
            if (node_str == LINKS_KEY ||
                node_str == FAIL_KEY ||
                node_str == RUNFILE_KEY || 
                node_str == CRIT_KEY) {
                continue; // Not a node we want
            }
            nodeMap.emplace(std::make_pair(node_str, std::make_shared<Node>(_context, node_str)));
        }
        return nodeMap;
    }

    Simulation::link_map Simulation::populate_links(BPS bw) {
        link_map linkMap;
        assert(_latency);
        for (auto link : _topology[LINKS_KEY]) {
            std::string link_str = link.as<std::string>();
            std::vector<std::string> parts;
            boost::split(parts, link_str, boost::is_any_of("-"));
            linkMap.emplace(std::make_pair(link_str, 
                        std::make_shared<Link>(_context, _latency, bw, _nodes.at(parts[0]), _nodes.at(parts[1]))));
        }
        return linkMap;
    }

    void Simulation::set_all_links_up() {
        for (auto link : _links) {
            link.second->set_up();
        }
    }

    void Simulation::set_all_links_down() {
        for (auto link : _links) {
            link.second->set_down();
        }
    }
}
