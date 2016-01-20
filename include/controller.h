#include "node.h"
#include "link.h"
#include "packet.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <forward_list>
#include <memory>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include <igraph/igraph.h>  // Graph processing (for the masses).
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

namespace PILO {
class Switch;

// A class to keep logs. Currently this is very inefficient in terms of memory, however
// the memory inefficiency is lower than my laziness.
class Log {
   public:
    // Can optimize these if needed.
    typedef std::unordered_map<std::string, std::vector<Link::State>> LinkLog;
    typedef std::unordered_map<std::string, std::vector<bool>> LogCommit;
    typedef std::unordered_map<std::string, uint64_t> LogMarked;
    typedef std::unordered_map<std::string, size_t> LogSizes;

    Log();

    // Tell the log that a link exists
    void open_log_link(const std::string& link);

    // Add a link event
    void add_link_event(const std::string& link, uint64_t version, Link::State state);

    // Record what things we might be missing.
    void compute_gaps(const std::shared_ptr<Packet>& packet);

    // Given a gossip packet, compute the response.
    std::vector<Packet::GossipLog> compute_response(const std::shared_ptr<Packet>& packet);

    // Given a gossip response, merge things together.
    void merge_logs(const std::shared_ptr<Packet>& packet);

   private:
    std::vector<uint64_t> compute_link_gap(const std::string& link, size_t&);
    LinkLog _log;
    LogCommit _commit;
    LogMarked _marked;
    LogSizes _sizes;
    LogMarked _max;

    const size_t INITIAL_SIZE = 1024;
    const size_t HWM = 8192;
    const size_t GROW = 128;
};

// Equivalent to LSController in Python
class Controller : public Node {
    // The simulation is our friend
    friend class Simulation;

   public:
    Controller(Context& context, const std::string& name, const Time referesh, const Time gossip,
               Distribution<bool>* drop);

    virtual void receive(std::shared_ptr<Packet> packet, Link* link);

    virtual void notify_link_existence(Link* link);

    virtual void notify_link_up(Link*);

    virtual void notify_link_down(Link*);

    virtual void silent_link_up(Link*);

    virtual void silent_link_down(Link*);

    static size_t compute_hash(const Packet::flowtable&);

    typedef std::unordered_map<std::string, std::shared_ptr<PILO::Node>> node_map;
    typedef std::unordered_map<std::string, std::shared_ptr<PILO::Switch>> switch_map;
    typedef std::unordered_map<std::string, std::shared_ptr<Controller>> controller_map;
    typedef std::unordered_map<std::string, igraph_integer_t> vertex_map;
    typedef std::unordered_map<igraph_integer_t, std::string> inv_vertex_map;
    typedef std::unordered_map<std::string, Packet::flowtable> flowtable_db;
    typedef std::unordered_map<std::string, uint64_t> flowtable_version;
    typedef std::unordered_map<std::string, std::unordered_set<std::string>> deleted_entries;

    // igraph is not C++, and allocates memory. So be nice and remove things.
    virtual ~Controller() { igraph_destroy(&_graph); }

   protected:
    // Some calls that can be used by the simulation to set up the controller.
    void add_controllers(controller_map controllers);
    void add_switches(switch_map switches);
    void add_nodes(node_map nodes);

    virtual bool add_link(const std::string& link, uint64_t version);
    virtual bool remove_link(const std::string& link, uint64_t version);

    // Compute paths, return a diff of what needs to be fixed.
    virtual std::pair<flowtable_db, deleted_entries> compute_paths();

    // Respond to various control messages.
    virtual void handle_link_up(const std::shared_ptr<Packet>& packet);
    virtual void handle_link_down(const std::shared_ptr<Packet>& packet);
    virtual void handle_switch_information(const std::shared_ptr<Packet>& packet);
    virtual void handle_gossip(const std::shared_ptr<Packet>& packet);
    virtual void handle_gossip_rep(const std::shared_ptr<Packet>& packet);
    virtual void handle_routing_resp(const std::shared_ptr<Packet>& packet);

    // Given a diff, packetize things and send rule updates to switches.
    virtual void apply_patch(std::pair<flowtable_db, deleted_entries>& diff);

    // Periodically query switches for information.
    virtual void send_switch_info_request();

    // Periodically (potentially) query switches for routing table
    virtual void send_routing_request();

    // Periodically gossip with controllers
    virtual void send_gossip_request();


    void add_new_link(const std::string&, uint64_t);
    inline bool is_host_link(const std::string&);
    inline bool add_host_link(const std::string&);
    inline bool remove_host_link(const std::string&);
    inline std::pair<std::string, std::string> split_parts(const std::string&);
    Distribution<bool>* _drop;
    std::unordered_set<std::string> _controllers;
    std::unordered_set<std::string> _switches;
    std::unordered_set<std::string> _nodes;
    std::unordered_set<std::string> _links;
    std::unordered_map<std::string, uint64_t> _linkVersion;
    std::unordered_map<std::string, std::forward_list<std::string>> _hostAtSwitch;
    std::unordered_map<std::string, size_t> _hostAtSwitchCount;
    vertex_map _vertices;
    inv_vertex_map _ivertices;
    igraph_t _graph;
    igraph_integer_t _usedVertices;
    flowtable_db _flowDb;
    std::unordered_set<uint64_t> _filter;
    std::unordered_set<std::string> _existingLinks;
    Time _refresh;
    Time _gossip;
    Log _log;
    boost::hash<std::string> _hash;
    flowtable_version _flow_version;
};

inline bool Controller::is_host_link(const std::string& link) {
    std::string v0, v1;
    std::tie(v0, v1) = split_parts(link);
    return ((_nodes.find(v0) != _nodes.end()) || (_nodes.find(v1) != _nodes.end()));
}

inline std::pair<std::string, std::string> Controller::split_parts(const std::string& link) {
    std::vector<std::string> parts;
    boost::split(parts, link, boost::is_any_of("-"));
    return std::make_pair(parts[0], parts[1]);
}
}
#endif
