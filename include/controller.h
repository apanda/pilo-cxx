#include "node.h"
#include "link.h"
#include "packet.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <igraph/igraph.h> // Graph processing
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__
// Equivalent to LSController in Python
namespace PILO {
    class Switch;
    class Controller : public Node {
        friend class Simulation;
        public:
            Controller(Context& context,
                 const std::string& name,
                 const Time referesh,
                 const Time gossip);
            
            virtual void receive(std::shared_ptr<Packet> packet, Link* link); 

            virtual void notify_link_existence(Link* link);
            
            virtual void notify_link_up(Link*);

            virtual void notify_link_down(Link*);

            virtual void silent_link_up(Link*);

            virtual void silent_link_down(Link*);

            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Node>> node_map;
            typedef std::unordered_map<std::string, std::shared_ptr<PILO::Switch>> switch_map;
            typedef std::unordered_map<std::string, std::shared_ptr<Controller>> controller_map;
            typedef std::unordered_map<std::string, igraph_integer_t> vertex_map;
            typedef std::unordered_map<igraph_integer_t, std::string> inv_vertex_map;
            typedef std::unordered_map<std::string, Packet::flowtable> flowtable_db;

            void add_controllers(controller_map controllers);
            void add_switches(switch_map switches);
            void add_nodes(node_map nodes);
            
            bool add_link(const std::string& link, uint64_t version);
            bool remove_link(const std::string& link, uint64_t version);

            flowtable_db compute_paths ();

            virtual ~Controller() {
                igraph_destroy(&_graph);
            }

        protected:
            virtual void handle_link_up(const std::shared_ptr<Packet>& packet);
            virtual void handle_link_down(const std::shared_ptr<Packet>& packet);
            virtual void handle_switch_information(const std::shared_ptr<Packet>& packet);
            virtual void handle_gossip(const std::shared_ptr<Packet>& packet);
            virtual void handle_gossip_rep(const std::shared_ptr<Packet>& packet);
            void apply_patch(flowtable_db& diff);
            void send_switch_info_request();
            void send_gossip_request();

        private:
            std::unordered_set<std::string> _controllers;
            std::unordered_set<std::string> _switches;
            std::unordered_set<std::string> _nodes;
            std::unordered_set<std::string> _links;
            std::unordered_map<std::string, uint64_t> _linkVersion;
            vertex_map _vertices;
            inv_vertex_map _ivertices;
            igraph_t _graph;
            igraph_integer_t _usedVertices;
            flowtable_db _flowDb;
            std::unordered_set<uint64_t> _filter;
            std::unordered_set<std::string> _existingLinks;
            Time _refresh;
            Time _gossip;
    };
}
#endif
