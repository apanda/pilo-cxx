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
#include "switch.h"
#include "controller.h"

#ifndef __SIMULATION_H__
#define __SIMULATION_H__
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

            void set_all_links_up_silent();

            void set_all_links_down_silent();
            
            void compute_all_paths();

            void install_all_routes();

            double check_routes();

            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Node>> node_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Link>> link_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Switch>> switch_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Controller>> controller_map;

            PILO::Context _context;
        private:

            node_map populate_nodes();

            link_map populate_links(BPS bw);
            
            uint32_t _seed;
            boost::mt19937 _rng;

            const YAML::Node _configuration;
            const YAML::Node _topology;

            Distribution<PILO::Time> *_latency; 
            switch_map _switches;
            controller_map _controllers;
            node_map _others;
            node_map _nodes;
            link_map _links;
            UniformIntDistribution _linkRng;
            UniformIntDistribution _nodeRng;
    };
}
#endif
