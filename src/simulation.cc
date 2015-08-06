#include "simulation.h"
namespace {
    const std::string LINKS_KEY = "links";
    const std::string FAIL_KEY = "fail_links";
    const std::string CRIT_KEY = "crit_links";
    const std::string RUNFILE_KEY = "runfile";
    const std::string TYPE_KEY = "type";
    const std::string ARG_KEY = "args";
    const std::string HOST_TYPE = "Host";
    const std::string CONTROLLER_TYPE = "LSGossipControl";
    const std::string SWITCH_TYPE = "LinkStateSwitch";
}

namespace PILO {
    Simulation::Simulation(const uint32_t seed, const std::string& configuration,
            const std::string& topology, const Time endTime, const Time refresh, const Time gossip, const BPS bw) :
        _context(endTime),
        _seed(seed),
        _rng(_seed),
        _configuration(YAML::LoadFile(configuration)),
        _topology(YAML::LoadFile(topology)),
        _latency(Distribution<PILO::Time>::get_distribution(_configuration["data_link_latency"], _rng)),
        _vmap(),
        _ivmap(),
        _switches(),
        _controllers(),
        _others(),
        _nodes(std::move(populate_nodes(refresh, gossip))),
        _links(std::move(populate_links(bw))),
        _linkRng(0, _links.size() - 1, _rng),
        _nodeRng(0, _nodes.size() - 1, _rng) {

        // Populate controller information
        for (auto controller : _controllers) {
            auto cobj = controller.second;
            cobj->add_controllers(_controllers);
            cobj->add_switches(_switches);
            cobj->add_nodes(_others);
        }
    }

    Simulation::node_map Simulation::populate_nodes(const Time refresh, const Time gossip) {
        igraph_empty(&_graph, 0, IGRAPH_UNDIRECTED);
        node_map nodeMap;
        igraph_integer_t count = 0;
        for (auto& node : _topology) {
            std::string node_str = node.first.as<std::string>();
            if (node_str == LINKS_KEY ||
                node_str == FAIL_KEY ||
                node_str == RUNFILE_KEY ||
                node_str == CRIT_KEY) {
                continue; // Not a node we want
            }
            std::string type_str = node.second[TYPE_KEY].as<std::string>();

            _vmap.emplace(std::make_pair(node_str, count));
            _ivmap.emplace(std::make_pair(count, node_str));
            count++;

            if (type_str == SWITCH_TYPE) {
                auto sw = std::make_shared<Switch>(_context, node_str);
                nodeMap.emplace(std::make_pair(node_str, sw));
                _switches.emplace(std::make_pair(node_str, sw));
            } else if (type_str == CONTROLLER_TYPE) {
                auto c = std::make_shared<Controller>(_context, node_str, refresh, gossip);
                std::cout << "Controller " << node_str << std::endl;
                nodeMap.emplace(std::make_pair(node_str, c));
                _controllers.emplace(std::make_pair(node_str, c));
            } else {
                auto n = std::make_shared<Node>(_context, node_str);
                nodeMap.emplace(std::make_pair(node_str, n));
                _others.emplace(std::make_pair(node_str, n));
            }
        }
        igraph_add_vertices(&_graph, count, NULL);
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
                        std::make_shared<Link>(_context, link_str,
                            _latency, bw, _nodes.at(parts[0]), _nodes.at(parts[1]))));
        }
        return linkMap;
    }

    void Simulation::add_graph_link(const std::shared_ptr<PILO::Link>& link) {
        uint32_t e0 = _vmap.at(link->_a->_name);
        uint32_t e1 = _vmap.at(link->_b->_name);
        igraph_add_edge(&_graph, e0, e1);
    }

    void Simulation::remove_graph_link(const std::shared_ptr<PILO::Link>& link) {
        uint32_t e0 = _vmap.at(link->_a->_name);
        uint32_t e1 = _vmap.at(link->_b->_name);
        igraph_integer_t eid;
        igraph_get_eid(&_graph, &eid, e0, e1, IGRAPH_UNDIRECTED, 0);
        if (eid != -1) {
            igraph_delete_edges(&_graph, igraph_ess_1(eid));
        }

    }

    void Simulation::set_all_links_up() {
        for (auto link : _links) {
            link.second->set_up();
            add_graph_link(link.second);
        }
    }

    void Simulation::set_all_links_down() {
        for (auto link : _links) {
            link.second->set_down();
            remove_graph_link(link.second);
        }
    }

    void Simulation::set_all_links_up_silent() {
        for (auto link : _links) {
            link.second->silent_set_up();
            add_graph_link(link.second);
            for (auto controller : _controllers) {
                controller.second->add_link(link.first, link.second->version());
            }
        }
    }

    void Simulation::set_all_links_down_silent() {
        for (auto link : _links) {

            link.second->silent_set_down();
            remove_graph_link(link.second);

            for (auto controller : _controllers) {
                controller.second->remove_link(link.first, link.second->version());
            }
        }
    }

    void Simulation::set_link_up(const std::string& link) {
        set_link_up(_links.at(link));
    }

    void Simulation::set_link_up(const std::shared_ptr<Link>& link) {
        link->set_up();
        add_graph_link(link);
    }

    void Simulation::set_link_down(const std::string& link) {
        set_link_down(_links.at(link));
    }

    void Simulation::set_link_down(const std::shared_ptr<Link>& link) {
        link->set_down();
        remove_graph_link(link);
    }

    void Simulation::compute_all_paths() {
        for (auto c : _controllers) {
            c.second->compute_paths();
        }
    }

    void Simulation::install_all_routes() {
        Controller::flowtable_db diffs;
        for (auto c : _controllers) {
            diffs = std::move(c.second->compute_paths());
        }

        for (auto diff : diffs) {
            auto sw = _switches.at(diff.first);
            sw->install_flow_table(diff.second);
        }
    }

    double Simulation::check_routes() const {
        uint64_t checked = 0;
        uint64_t passed = 0;
        //std::cout << _context.get_time() <<
            //"  Checking \t\t" << " edge count for graph is " << igraph_ecount(&_graph) << std::endl;
        igraph_matrix_t distances;
        igraph_matrix_init(&distances, 1, 1);
        igraph_shortest_paths(&_graph, &distances, igraph_vss_all(), igraph_vss_all(), IGRAPH_ALL);

        for (auto h1 : _others) {
            for (auto h2 : _others) {
#if 0
                std::cout << "Going to check" << std::endl;
                std::cout << "\t" << h1.first << "   " << h2.first << "    ";
#endif
                if (h1.first == h2.first) {
                    //std::cout << "skip (same)" << std::endl;
                    continue;
                }
                // Skip if the underlying topology is disconnected
                if (MATRIX(distances, _vmap.at(h1.first), _vmap.at(h2.first)) == IGRAPH_INFINITY) {
                    //std::cout << "skip (!connected)" << std::endl;
                    continue;
                }

                checked += 1;
                std::string sig = Packet::generate_signature(h1.first, h2.first, Packet::DATA);
                for (auto begin_link : h1.second->_links) {
                    std::string link = begin_link.first;
                    auto current = h1.second;
                    while (current.get() != h2.second.get()) {
                        if (_links.at(link)->is_up()) {
                            current = _links.at(link)->get_other(current);
                            auto as_switch = std::dynamic_pointer_cast<Switch>(current);
                            if (!as_switch) {
                                // Maybe we have reached the end, maybe not. But this is not a switch.
                                break;
                            }
                            auto entry = as_switch->_forwardingTable.find(sig);
                            if (entry != as_switch->_forwardingTable.end()) {
                                link = entry->second;
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                    if (current.get() == h2.second.get()) {
                        passed++;
                        break;
                    }
                }
            }
        }
        //std::cout << "\tChecked " << checked << "    passed   " << passed << std::endl;
        igraph_matrix_destroy(&distances);

        return ((double)passed)/((double)checked);
    }

    void Simulation::dump_bw_used() const {
        size_t total_data = 0;
        std::unordered_map<int, size_t> data_by_type;

        for (int i = 0; i < Packet::END; i++) {
            data_by_type.emplace(std::make_pair(i, 0));
        }

        for (auto link : _links) {
            total_data += link.second->_totalBits;
            for (auto bw : link.second->_bitByType) {
                data_by_type[bw.first] += bw.second;
            }
        }

        BPS overall_bw = ((BPS)total_data) / _context.now();
        BPS bw_per_link = overall_bw / _links.size();
        std::cout << _context.now() << " bw  total " << std::fixed << overall_bw << " link "
            <<  std::fixed << bw_per_link << std::endl;
        std::cout << "\t By type:" << std::endl;
        for (auto per_type : data_by_type) {
            BPS overall = ((per_type.second) / _context.now());
            BPS per_link = overall / _links.size();
            std::cout << "\t\t " << Packet::IType[per_type.first] << " bw total "
                << std::fixed << overall << " link " << std::fixed <<  per_link << std::endl;
        }
    }
}
