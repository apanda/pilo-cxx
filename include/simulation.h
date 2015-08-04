#include <iostream>
#include <list>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <yaml-cpp/yaml.h>
#include <memory>
#include "context.h"
#include "link.h"
#include "node.h"
#include "distributions.h"
#include "packet.h"

#ifndef __SIMULATION_H__
#define __SIMULATION_H__
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

}
namespace PILO {
    class Simulation {
        public:
            Simulation(const uint32_t seed, const std::string& configuration, const std::string& topology); 
            
            inline void run() {
                while(_context.next());
            }
            
            // Return a random link
            inline std::shared_ptr<PILO::Link> random_link() {
                return std::next(std::begin(_links), _linkRng.next())->second;
            }
            
            // Return a random node
            inline std::shared_ptr<Node> random_node() {
                auto count =  _nodeRng.next();
                auto next = std::next(std::begin(_nodes), count);
                return next->second;
            }

            inline std::shared_ptr<PILO::Node> get_node(const std::string& id) {
                return _nodes.at(id);
            }

            inline std::shared_ptr<PILO::Link> get_link(const std::string& id) {
                return _links.at(id);
            }

            void set_all_links_up();

            void set_all_links_down();

            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Node>> node_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Link>> link_map;

            PILO::Context _context;
        private:

            node_map populate_nodes();

            link_map populate_links(BPS bw);
            
            uint32_t _seed;
            boost::mt19937 _rng;

            const YAML::Node _configuration;
            const YAML::Node _topology;

            Distribution<PILO::Time> *_latency; 
            node_map _nodes;
            link_map _links;
            UniformIntDistribution _linkRng;
            UniformIntDistribution _nodeRng;
    };
}
#endif
