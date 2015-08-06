#include <iostream>
#include <list>
#include <unordered_map>
#include <memory>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <yaml-cpp/yaml.h>
#include <igraph/igraph.h> // Graph processing
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

    // The simulation itself. Where all the sausage is made.
    class Simulation {
        public:
            Simulation(const uint32_t seed, const std::string& configuration, const std::string& topology,
                    const Time endTime, const Time refresh, const Time gossip, const BPS bw);

            // Run to completion
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

            inline std::shared_ptr<PILO::Node> get_node(const std::string& id) const {
                return _nodes.at(id);
            }

            inline std::shared_ptr<PILO::Link> get_link(const std::string& id) const {
                return _links.at(id);
            }

            void set_all_links_up();

            void set_all_links_down();

            void set_all_links_up_silent();

            void set_all_links_down_silent();

            void set_link_up(const std::string&);
            void set_link_up(const std::shared_ptr<Link>&);

            void set_link_down(const std::string&);
            void set_link_down(const std::shared_ptr<Link>&);

            void compute_all_paths();

            void install_all_routes();

            double check_routes() const;

            void dump_bw_used() const;

            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Node>> node_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Link>> link_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Switch>> switch_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Controller>> controller_map;

            virtual ~Simulation() {
                igraph_destroy(&_graph);
            }

            inline boost::mt19937& rng() { return _rng; }

            PILO::Context _context;
        private:

            void add_graph_link(const std::shared_ptr<PILO::Link>& link);
            void remove_graph_link(const std::shared_ptr<PILO::Link>& link);

            node_map populate_nodes(const Time refresh, const Time gossip);

            link_map populate_links(BPS bw);

            uint32_t _seed;
            boost::mt19937 _rng;

            igraph_t _graph;

            const YAML::Node _configuration;
            const YAML::Node _topology;

            Distribution<PILO::Time> *_latency;
            Controller::vertex_map _vmap;
            Controller::inv_vertex_map _ivmap;
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
