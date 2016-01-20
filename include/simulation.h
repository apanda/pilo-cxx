#include <iostream>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <yaml-cpp/yaml.h>
#include <igraph/igraph.h>  // Graph processing
#include "context.h"
#include "link.h"
#include "node.h"
#include "distributions.h"
#include "packet.h"
#include "switch.h"
#include "controller.h"
#include "te_controller.h"
#include "coord_controller.h"

#ifndef __SIMULATION_H__
#define __SIMULATION_H__
namespace PILO {

// The simulation itself. Where all the sausage is made.
class Simulation {
   public:
    Simulation(const uint32_t seed, const std::string& configuration, const std::string& topology, bool version,
            const Time endTime, const Time refresh, const Time gossip, const BPS bw, const int limit,
               std::unique_ptr<Distribution<bool>>&& drop, std::unique_ptr<Distribution<bool>>&& cdrop);

    // Run to completion
    inline void run() {
        while (!_stopped && _context.next())
            ;
    }

    // Return a random link
    inline std::shared_ptr<PILO::Link> random_link() { return std::next(std::begin(_links), _linkRng.next())->second; }

    // Return a random switch link
    inline std::shared_ptr<PILO::Link> random_switch_link() {
        return _links.at(*std::next(std::begin(_switchLinks), _swLinkRng.next()));
    }

    inline std::shared_ptr<PILO::Link> random_switch_ctrl_link() {
        return _links.at(*std::next(std::begin(_swControllerLinks), _swCLinkRng.next()));
    }

    // Return a random node
    inline std::shared_ptr<Node> random_node() {
        auto count = _nodeRng.next();
        auto next = std::next(std::begin(_nodes), count);
        return next->second;
    }

    inline std::shared_ptr<PILO::Link> random_controller_link() {
        return _links.at(*std::next(std::begin(_controllerLinks), _cLinkRng.next()));
    }

    inline std::shared_ptr<PILO::Node> get_node(const std::string& id) const { return _nodes.at(id); }

    inline std::shared_ptr<PILO::Link> get_link(const std::string& id) const { return _links.at(id); }

    void stop() { _stopped = true; }

    void set_all_links_up();

    void set_all_links_down();

    void set_all_links_up_silent();

    void set_all_links_down_silent();

    void set_all_controller_links_down();

    void set_all_controller_links_down(const std::shared_ptr<PILO::Link>& ex);

    void set_link_up(const std::string&);
    void set_link_up(const std::shared_ptr<Link>&);

    void set_link_down(const std::string&);
    void set_link_down(const std::shared_ptr<Link>&);

    void compute_all_paths();

    void install_all_routes();

    double check_routes(double& global_distance, double& net_distance, double& difference) const;

    uint32_t max_link_usage() const;

    void dump_link_usage() const;

    void dump_bw_used() const;
    
    void dump_table_changes() const;

    void reset_links();

    typedef std::unordered_map<std::string, std::shared_ptr<PILO::Node>> node_map;
    typedef std::unordered_map<std::string, std::shared_ptr<PILO::Link>> link_map;
    typedef std::unordered_map<std::string, std::shared_ptr<PILO::Switch>> switch_map;
    typedef std::unordered_map<std::string, std::shared_ptr<PILO::Controller>> controller_map;
    typedef std::unordered_map<std::string, std::string> node_switch_map;
    typedef std::unordered_set<std::string> link_set;

    virtual ~Simulation() { igraph_destroy(&_graph); }

    inline boost::mt19937& rng() { return _rng; }

    PILO::Context _context;

   private:
    void add_graph_link(const std::shared_ptr<PILO::Link>& link);
    void remove_graph_link(const std::shared_ptr<PILO::Link>& link);
    inline bool add_host_graph_link(const std::shared_ptr<PILO::Link>& link);
    inline bool remove_host_graph_link(const std::shared_ptr<PILO::Link>& link);
    double compute_controller_diameter();

    node_map populate_nodes(const Time refresh, const Time gossip, const bool version);

    link_map populate_links(BPS bw);

    std::pair<std::string, std::shared_ptr<Link>> populate_link(const std::string& link, BPS bw,
                                                                Distribution<Time>* latency);

    int _flowLimit;
    uint32_t _seed;
    boost::mt19937 _rng;

    std::unique_ptr<Distribution<bool>> _dropRng;
    std::unique_ptr<Distribution<bool>> _cdropRng;

    igraph_t _graph;

    const YAML::Node _configuration;
    const YAML::Node _topology;

    Distribution<PILO::Time>* _latency;
    Distribution<PILO::Time>* _hlatency;
    Controller::vertex_map _vmap;
    Controller::inv_vertex_map _ivmap;
    node_switch_map _nsmap;
    switch_map _switches;
    controller_map _controllers;
    node_map _others;
    node_map _nodes;
    link_set _switchLinks;
    link_set _controllerLinks;
    link_set _swControllerLinks;
    link_set _switchAndControllerLinks;
    link_set _liveLinks;
    link_map _links;
    UniformIntDistribution _cLinkRng;
    UniformIntDistribution _swLinkRng;
    UniformIntDistribution _swCLinkRng;
    UniformIntDistribution _linkRng;
    UniformIntDistribution _nodeRng;
    bool _stopped;
};
}
#endif
