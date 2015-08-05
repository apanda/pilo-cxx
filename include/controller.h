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
                 const std::string& name);
            
            virtual void receive(std::shared_ptr<Packet> packet); 

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
            
            void add_link(const std::string& link);
            void remove_link(const std::string& link);

            flowtable_db compute_paths ();

            virtual ~Controller() {
                igraph_destroy(&_graph);
            }

        private:
            std::unordered_set<std::string> _controllers;
            std::unordered_set<std::string> _switches;
            std::unordered_set<std::string> _nodes;
            vertex_map _vertices;
            inv_vertex_map _ivertices;
            igraph_t _graph;
            igraph_integer_t _usedVertices;
            flowtable_db _flowDb;
    };
}
#endif
