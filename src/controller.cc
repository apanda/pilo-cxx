#include "controller.h"
#include "packet.h"
#include <boost/algorithm/string.hpp>
namespace PILO {
    Controller::Controller(Context& context, const std::string& name, const Time refresh):
        Node(context, name),
        _controllers(),
        _switches(), 
        _nodes(),
        _vertices(),
        _filter(),
        _refresh(refresh) {
        // Create an empty graph
        igraph_empty(&_graph, 0, IGRAPH_UNDIRECTED);
        _usedVertices = 0;
        _context.schedule(_refresh, [&](double) {this->send_switch_info_request();});
    }

    void Controller::receive(std::shared_ptr<Packet> packet, Link* link) {
        if (_filter.find(packet->_id) != _filter.end()) {
            return;
        }
        _filter.emplace(packet->_id);
        if (packet->_type >= Packet::CONTROL &&
             (packet->_destination == _name ||
              packet->_destination == Packet::WILDCARD)) {
            // If the packet is intended for the controller, process it.
            switch (packet->_type) {
                case Packet::LINK_UP:
                    handle_link_up(packet);
                    break;
                case Packet::LINK_DOWN:
                    handle_link_down(packet);
                    break;
                case Packet::SWITCH_INFORMATION:
                    handle_switch_information(packet);
                    break;
                default:
                    break;
                    // Do nothing
            }
        }
    }

    void Controller::handle_switch_information(const std::shared_ptr<Packet>& packet) {
        //std::cout << _context.get_time() << " " << _name << " switch info received " << packet->_source << std::endl;
        bool changes = false;
        for (auto lv : packet->data.linkVersion) {
            if (lv.second > _linkVersion.at(lv.first)) {
                changes = true;
                if (packet->data.linkState.at(lv.first) == Link::UP) {
                    add_link(lv.first, lv.second);
                } else if (packet->data.linkState.at(lv.first) == Link::DOWN) {
                    remove_link(lv.first, lv.second);
                }
            }
        }
        if (changes) {
            auto patch = compute_paths();
            apply_patch(patch);
        }
    }

    void Controller::handle_link_up(const std::shared_ptr<Packet>& packet) {
        //std::cout << _context.get_time() << " " << _name << " responding to link up" << std::endl;
        auto link = packet->data.link;
        if (add_link(link, packet->data.version)) {
            auto patch = compute_paths();
            apply_patch(patch);
        }
    }

    void Controller::handle_link_down(const std::shared_ptr<Packet>& packet) {
        //std::cout << _context.get_time() << " " << _name << " responding to link down" << std::endl;
        auto link = packet->data.link;
        if (remove_link(link, packet->data.version)) {
            auto patch = compute_paths();
            apply_patch(patch);
        }
    }

    void Controller::apply_patch(flowtable_db& diff) {
        for (auto part : diff) {
            auto dest = part.first;
            auto patch = part.second;
            size_t patch_size = patch.size();
            //std::cout << _context.get_time() << " " << _name << " sending a patch to " << dest << std::endl;
            // Each rule is header + link to go out
            size_t packet_size = Packet::HEADER + patch_size * (64 + Packet::HEADER);
            auto update = Packet::make_packet(_name, dest, Packet::CHANGE_RULES, packet_size);
            update->data.table.swap(patch);
            flood(update);
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

    bool Controller::add_link(const std::string& link, uint64_t version) {
        if (_links.find(link) == _links.end()) {
            _links.emplace(link);
            _linkVersion.emplace(std::make_pair(link, version));
        } else {
            // We have already seen this link state update (or a newer one)
            if (version <= _linkVersion.at(link)) {
                std::cout << _context.get_time() << "  " << _name << " rejected due to version " << std::endl;
                return false;
            }
        }
        _linkVersion[link] = version;
        if (_existingLinks.find(link) != _existingLinks.end()) {
            return false; 
        }
        std::vector<std::string> parts;
        _existingLinks.emplace(link);
        boost::split(parts, link, boost::is_any_of("-"));
        igraph_add_edge(&_graph, _vertices.at(parts[0]), _vertices.at(parts[1]));
        return true;
    }

    bool Controller::remove_link(const std::string& link, uint64_t version) {
        if (version <= _linkVersion.at(link)) {
            std::cout << _context.get_time() << "  " << _name << " rejected due to version " << std::endl;
            return false;
        }

        _linkVersion[link] = version;

        if (_existingLinks.find(link) == _existingLinks.end()) {
            return false;
        }

        std::vector<std::string> parts;
        _existingLinks.erase(link);
        boost::split(parts, link, boost::is_any_of("-"));
        igraph_integer_t eid;
        igraph_get_eid(&_graph, &eid, _vertices.at(parts[0]), _vertices.at(parts[1]), IGRAPH_UNDIRECTED, 0);
        assert(eid != -1);
        igraph_delete_edges(&_graph, igraph_ess_1(eid));
        return true;
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
            _flowDb.emplace(std::make_pair(sw.first, Packet::flowtable()));
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

    Controller::flowtable_db Controller::compute_paths() {
        igraph_vector_t l;
        igraph_vector_ptr_t p;
        flowtable_db diffs;
        int bias = 0;

        for (int v0_idx = 0; v0_idx < _usedVertices; v0_idx++) {
            std::string v0 = _ivertices.at(v0_idx);

            if(_nodes.count(v0) == 0) {
                continue;
            }

            igraph_vector_ptr_init(&p, 0);
            igraph_vector_init(&l, 0);
            int ret = igraph_get_all_shortest_paths(&_graph, &p, &l, _vertices.at(v0), igraph_vss_all(), IGRAPH_ALL);

            (void)ret; // I like asserts
            assert(ret == 0);
            int len = igraph_vector_size(&l);
            int base_idx = 0;

            for (int v1_idx = 0; v1_idx < len; v1_idx++) {
                std::string v1 = _ivertices.at(v1_idx);
                int paths = VECTOR(l)[v1_idx];
                if (_nodes.count(v1) > 0 && v0_idx != v1_idx) {
                    if (paths > 0) {
                        std::string psig = Packet::generate_signature(v0, v1, Packet::DATA);
                        int path_idx = bias % paths;
                        igraph_vector_t* path = (igraph_vector_t*)VECTOR(p)[path_idx + base_idx];
                        int path_len = igraph_vector_size(path);
                        for (int k = 1; k < path_len - 1; k++) {
                            std::string sw = _ivertices.at(VECTOR(*path)[k]);
                            std::string nh = _ivertices.at(VECTOR(*path)[k + 1]);
                            std::string link = sw + "-" + nh;
                            // Cannonicalize
                            if (_links.find(link) == _links.end()) {
                                link = nh + "-" + sw;
                            }
                            assert(_links.find(link) != _links.end());
                            auto rule = _flowDb.at(sw).find(psig);
                            if (rule == _flowDb.at(sw).end() ||
                                rule->second != link) {
                                _flowDb[sw][psig] = link;
                                diffs[sw][psig] = link;
                            } else {
                            }
                        }
                    } 
#if 0
                    else {
                        std::cout << "Cannot find path between " << v0 << "   " << v1 << std::endl;
                        std::cout << "\t\t" << " edge count for graph is " << igraph_ecount(&_graph) << std::endl;
                    }
#endif
                    bias ++;
                }
                base_idx += paths;
            }

            igraph_vector_ptr_destroy_all(&p);
            igraph_vector_destroy(&l);
        }
        return diffs;
    }

    void Controller::send_switch_info_request() {
        auto req = Packet::make_packet(_name, Packet::SWITCH_INFORMATION_REQ, Packet::HEADER);
        flood(req);
        _context.schedule(_refresh, [&](double) {this->send_switch_info_request();});
    }
}
