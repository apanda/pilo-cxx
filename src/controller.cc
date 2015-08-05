#include "controller.h"
#include "packet.h"
#include <boost/algorithm/string.hpp>
namespace PILO {
    Controller::Controller(Context& context, const std::string& name):
        Node(context, name),
        _controllers(),
        _switches(), 
        _nodes(),
        _vertices() {
        // Create an empty graph
        igraph_empty(&_graph, 0, IGRAPH_UNDIRECTED);
        _usedVertices = 0;
    }

    void Controller::receive(std::shared_ptr<Packet> packet) {
        if (packet->_type >= Packet::CONTROL &&
             (packet->_destination == _name ||
              packet->_destination == Packet::WILDCARD)) {
            // If the packet is intended for the switch, process it.
            switch (packet->_type) {
                case Packet::CHANGE_RULES:
                    break;
                case Packet::SWITCH_INFORMATION: {
                    }
                    break;
                default:
                    break;
                    // Do nothing
            }
        }
    }

    void Controller::notify_link_existence(Link* link) {
        Node::notify_link_existence(link);
    }
    
    void Controller::notify_link_up(Link* link) {
        Node::notify_link_up(link);
    }

    void Controller::notify_link_down(Link* link) {
        Node::notify_link_down(link);
    }

    void Controller::silent_link_up(Link* link) {
        Node::silent_link_up(link);
    }

    void Controller::silent_link_down(Link* link) {
        Node::silent_link_down(link);
    }

    void Controller::add_link(const std::string& link) {
        std::vector<std::string> parts;
        boost::split(parts, link, boost::is_any_of("-"));
        //std::cout << _name << "  adding link " << link << " (" << _vertices.at(parts[0]) << "<-->" 
            //<< _vertices.at(parts[1]) << ")" <<  std::endl;
        igraph_add_edge(&_graph, _vertices.at(parts[0]), _vertices.at(parts[1]));
    }

    void Controller::remove_link(const std::string& link) {
        std::vector<std::string> parts;
        boost::split(parts, link, boost::is_any_of("-"));
        igraph_integer_t eid;
        igraph_get_eid(&_graph, &eid, _vertices.at(parts[0]), _vertices.at(parts[1]), IGRAPH_UNDIRECTED, 0);
        if (eid != -1) {
            igraph_delete_edges(&_graph, igraph_ess_1(eid));
        }
    }

    void Controller::add_controllers(controller_map controllers) {
        int count = 0;
        for (auto controller : controllers) {
            _controllers.emplace(controller.first);
            _nodes.emplace(controller.first);
            igraph_integer_t idx = count + _usedVertices;
            _vertices[controller.first] = idx;
            _ivertices[idx] = controller.first;
            count++;
        }
        igraph_add_vertices(&_graph, count, NULL);
        _usedVertices += count;
    }

    void Controller::add_switches(switch_map switches) {
        int count = 0;
        for (auto sw : switches) {
            _controllers.emplace(sw.first);
            igraph_integer_t idx = count + _usedVertices;
            _vertices[sw.first] = idx;
            _ivertices[idx] = sw.first;
            count++;
        }
        igraph_add_vertices(&_graph, count, NULL);
        _usedVertices += count;
    }

    void Controller::add_nodes(node_map nodes) {
        int count = 0;
        for (auto node : nodes) {
            _nodes.emplace(node.first);
            igraph_integer_t idx = count + _usedVertices;
            _vertices[node.first] = idx;
            _ivertices[idx] = node.first;
            count++;
        }
        igraph_add_vertices(&_graph, count, NULL);
        _usedVertices += count;
    }

    void Controller::compute_paths() {
        igraph_vector_t l;
        igraph_vector_ptr_t p;
        for (auto v0 : _nodes) {
            int v0_idx = _vertices.at(v0);
            igraph_vector_ptr_init(&p, 0);
            igraph_vector_init(&l, 0);
            int ret = igraph_get_all_shortest_paths(&_graph, &p, &l, _vertices.at(v0), igraph_vss_all(), IGRAPH_ALL);
            assert(ret == 0);
            int len = igraph_vector_size(&l);
            std::cout << _name << " Return len for " << v0 << " is " << len << std::endl;
            int base_idx = 0;

            for (int i = 0; i < len; i++) {
                int paths = VECTOR(l)[i];
                std::string v1 = _ivertices.at(i);
                if (_nodes.count(v1) > 0 && v0_idx != i) {
                    std::cout << "\t " << v0 << "   " << v1 << std::endl;
                    for (int j = 0; j < paths; j++) {
                        igraph_vector_t* path = (igraph_vector_t*)VECTOR(p)[j + base_idx];
                        int path_len = igraph_vector_size(path);
                        std::cout << "\t\t";
                        for (int k = 0; k < path_len; k++) {
                            std::cout << _ivertices.at(VECTOR(*path)[k]) << " -- ";
                        }
                        std::cout << std::endl;
                    }
                }
                base_idx += paths;
            }

            igraph_vector_ptr_destroy_all(&p);
            igraph_vector_destroy(&l);
        }
    }
}
