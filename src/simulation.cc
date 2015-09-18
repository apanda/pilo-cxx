#include "simulation.h"
namespace {
    const std::string LINKS_KEY = "links";
    const std::string HLAT_LINKS_KEY = "high_latency_links";
    const std::string FAIL_KEY = "fail_links";
    const std::string CRIT_KEY = "crit_links";
    const std::string RUNFILE_KEY = "runfile";
    const std::string TYPE_KEY = "type";
    const std::string ARG_KEY = "args";
    const std::string HOST_TYPE = "Host";
    const std::string CONTROLLER_TYPE = "LSGossipControl";
    const std::string TE_CONTROLLER_TYPE = "LSTEControl";
    const std::string COORD_CONTROLLER_TYPE = "CoordinationOracleControl";
    const std::string SWITCH_TYPE = "LinkStateSwitch";
}

namespace PILO {
    Simulation::Simulation(const uint32_t seed, const std::string& configuration,
            const std::string& topology, const Time endTime, const Time refresh,
            const Time gossip, const BPS bw, const int limit, std::unique_ptr<Distribution<bool>>&& drop,
            std::unique_ptr<Distribution<bool>>&& cdrop) :
        _context(endTime),
        _flowLimit(limit),
        _seed(seed),
        _rng(_seed),
        _dropRng(std::move(drop)),
        _cdropRng(std::move(cdrop)),
        _configuration(YAML::LoadFile(configuration)),
        _topology(YAML::LoadFile(topology)),
        _latency(Distribution<PILO::Time>::get_distribution(_configuration["data_link_latency"], _rng)),
        _hlatency(Distribution<PILO::Time>::get_distribution(_configuration["data_link_hlatency"] ? 
							    _configuration["data_link_hlatency"] :
							    _configuration["data_link_latency"], _rng)),
        _vmap(),
        _ivmap(),
        _nsmap(),
        _switches(),
        _controllers(),
        _others(),
        _nodes(std::move(populate_nodes(refresh, gossip))),
        _switchLinks(),
        _links(std::move(populate_links(bw))),
        _swLinkRng(0, _switchLinks.size() - 1, _rng),
        _linkRng(0, _links.size() - 1, _rng),
        _nodeRng(0, _nodes.size() - 1, _rng),
        _stopped(false) {
        // Do not print igraph warnings
        igraph_set_warning_handler(igraph_warning_handler_ignore);
        std::cout << "PILO simulation set limit = " << _flowLimit << "    " << limit << std::endl;
        // Populate controller information
        Coordinator::GetInstance()->set_context(&_context);
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
                node_str == CRIT_KEY ||
                node_str == HLAT_LINKS_KEY) {
                continue; // Not a node we want
            }
            std::string type_str = node.second[TYPE_KEY].as<std::string>();

            if (type_str == SWITCH_TYPE) {
                auto sw = std::make_shared<Switch>(_context, node_str);
                nodeMap.emplace(std::make_pair(node_str, sw));
                _switches.emplace(std::make_pair(node_str, sw));
                _vmap.emplace(std::make_pair(node_str, count));
                _ivmap.emplace(std::make_pair(count, node_str));
                count++;
            } else if (type_str == TE_CONTROLLER_TYPE) {
                std::cout << "PILO simulation set limit = " << _flowLimit << std::endl;
                auto c = std::make_shared<TeController>(_context, node_str, refresh, gossip, _flowLimit, _cdropRng.get());
                std::cout << "TE Controller " << node_str << std::endl;
                nodeMap.emplace(std::make_pair(node_str, c));
                _controllers.emplace(std::make_pair(node_str, c));
            } else if (type_str == CONTROLLER_TYPE) {
                auto c = std::make_shared<Controller>(_context, node_str, refresh, gossip, _cdropRng.get());
                std::cout << "Controller " << node_str << std::endl;
                nodeMap.emplace(std::make_pair(node_str, c));
                _controllers.emplace(std::make_pair(node_str, c));
            } else if (type_str == COORD_CONTROLLER_TYPE) {
                auto c = std::make_shared<CoordinationController>(_context, 
                        node_str, 
                        refresh, 
                        gossip, 
                        _cdropRng.get());
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

    std::pair<std::string, std::shared_ptr<Link>> 
    	    Simulation::populate_link(const std::string& link,
				   BPS bw,
				   Distribution<Time>* latency) {
	    std::vector<std::string> parts;
	    boost::split(parts, link, boost::is_any_of("-"));
	    if (_switches.find(parts[0]) != _switches.end() &&
		_switches.find(parts[1]) != _switches.end()) {
		_switchLinks.emplace(link);
	    }
	    return std::make_pair(link,
			std::make_shared<Link>(_context, link,
			latency, bw, _nodes.at(parts[0]), _nodes.at(parts[1]), _dropRng.get()));
    }

    Simulation::link_map Simulation::populate_links(BPS bw) {
        link_map linkMap;
        assert(_latency);
        for (auto link : _topology[LINKS_KEY]) {
            std::string link_str = link.as<std::string>();
            linkMap.emplace(populate_link(link_str, bw, _latency));
        }

        if (_topology[HLAT_LINKS_KEY]) {
        	for (auto link : _topology[HLAT_LINKS_KEY]) {
		    std::string link_str = link.as<std::string>();
		    linkMap.emplace(populate_link(link_str, bw, _hlatency));
		}
	}
        return linkMap;
    }

    bool Simulation::add_host_graph_link(const std::shared_ptr<PILO::Link>& link) {
        if (_switches.find(link->_a->_name) == _switches.end()) {
            _nsmap.emplace(std::make_pair(link->_a->_name, link->_b->_name));
            return true;
        } else if (_switches.find(link->_b->_name) == _switches.end()) {
            _nsmap.emplace(std::make_pair(link->_b->_name, link->_a->_name));
            return true;
        }
        return false;
    }

    bool Simulation::remove_host_graph_link(const std::shared_ptr<PILO::Link>& link) {
        if (_switches.find(link->_a->_name) == _switches.end()) {
            _nsmap.erase(link->_a->_name);
            return true;
        } else if (_switches.find(link->_b->_name) == _switches.end()) {
            _nsmap.erase(link->_b->_name);
            return true;
        }
        return false;
    }

    void Simulation::add_graph_link(const std::shared_ptr<PILO::Link>& link) {
        if (add_host_graph_link(link)) return;
        uint32_t e0 = _vmap.at(link->_a->_name);
        uint32_t e1 = _vmap.at(link->_b->_name);
        igraph_add_edge(&_graph, e0, e1);
    }

    void Simulation::remove_graph_link(const std::shared_ptr<PILO::Link>& link) {
        if (remove_host_graph_link(link)) return;
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
        //std::cout << "Controller Diameter " << compute_controller_diameter() << std::endl;
        auto diameter = compute_controller_diameter();
        std::cout << "Controller Diameter " << diameter << " rtt = " << diameter * 2.0 << std::endl;
        Coordinator::GetInstance()->set_rtt(diameter * 2.0);
    }

    void Simulation::set_all_links_down_silent() {
        for (auto link : _links) {

            link.second->silent_set_down();
            remove_graph_link(link.second);

            for (auto controller : _controllers) {
                controller.second->remove_link(link.first, link.second->version());
            }
        }
        std::cout << "Controller Diameter " << compute_controller_diameter() << std::endl;
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
            std::tie(diffs, std::ignore) = c.second->compute_paths();
        }
        size_t min = 1ull<<33, max = 0, count = 0, total = 0;
        for (auto diff : diffs) {
            count++;
            size_t sz = diff.second.size();
            if (sz < min) {
                min = sz;
            }
            if (sz > max) {
                max = sz;
            }
            total += sz;
            auto sw = _switches.at(diff.first);
            sw->install_flow_table(diff.second);
        }
        std::cout << "Rule sizes: min " << min << " max " << max << " count " << count << " total " << total << std::endl;
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
                if (_nsmap.find(h1.first) == _nsmap.end() || _nsmap.find(h2.first) == _nsmap.end()) {
                    continue;
                }
                igraph_integer_t v0 = _vmap.at(_nsmap.at(h1.first));
                igraph_integer_t v1 = _vmap.at(_nsmap.at(h2.first));
                if (MATRIX(distances, v0, v1) == IGRAPH_INFINITY) {
                    std::cout << "skip (!connected)" << std::endl;
                    continue;
                }

                checked += 1;
                std::string sig = Packet::generate_signature(h1.first, h2.first, Packet::DATA);
                std::unordered_set<std::string> visited;
                for (auto begin_link : h1.second->_links) {
                    std::string link = begin_link.first;
                    auto current = h1.second;
                    visited.emplace(current->_name);
                    while (current.get() != h2.second.get()) {
                        if (_links.at(link)->is_up()) {
                            current = _links.at(link)->get_other(current);

                            if (visited.find(current->_name) != visited.end()) {
                                std::cout << "WARNING: LOOP DETECTED" << std::endl;
                                break;
                            }
                            visited.emplace(current->_name);
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
        std::cout << "\tChecked " << checked << "    passed   " << passed << std::endl;
        igraph_matrix_destroy(&distances);

        return ((double)passed)/((double)checked);
    }


    double Simulation::compute_controller_diameter() {
        igraph_vector_t path;
        double longest = 0.0;
        for (auto c0 : _controllers) {
            for (auto c1 : _controllers) {
                if (c0.first == c1.first) {
                    continue;
                }
                igraph_integer_t c0_idx = _vmap.at(_nsmap.at(c0.first));
                igraph_integer_t c1_idx = _vmap.at(_nsmap.at(c1.first));
                igraph_vector_init(&path, 0);
                igraph_get_shortest_path(&_graph, &path, NULL, c0_idx, c1_idx, IGRAPH_OUT);
                int path_len = igraph_vector_size(&path);
                assert(path_len > 0);
                double lat_len = 0.0;
                for (int k = 0; k < path_len - 1; k++) {
                    auto l0 = _ivmap.at(VECTOR(path)[k]);
                    auto l1 = _ivmap.at(VECTOR(path)[k + 1]);
                    auto link = l0 + "-" + l1;
                    if (_links.find(link) == _links.end()) {
                        link = l1 + "-" + l0;
                    }
                    lat_len += _links.at(link)->_latency->mean();
                }
                if (lat_len > longest) {
                    longest = lat_len;
                }
            }
        }
        return longest;
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

    uint32_t Simulation::max_link_usage() const {
        auto controller = std::begin(_controllers)->second;
        int32_t max = 0;
        for (auto sw_pair : _switches) {
            auto sw = sw_pair.second;
            for (auto l_pair : sw->_linkStats) {
                if (!controller->is_host_link(l_pair.first) && max < l_pair.second) {
                    max = l_pair.second;
                }
            }
        }
        return max;
    }

    void Simulation::dump_link_usage() const {
        auto controller = std::begin(_controllers)->second;
        size_t checked = 0;
        size_t tight = 0;
        for (auto sw_pair : _switches) {
            auto name = sw_pair.first;
            auto sw = sw_pair.second;
            for (auto l_pair : sw->_linkStats) {
                if (!controller->is_host_link(l_pair.first)) {
                    std::cout << "\t\t" << name << " " << l_pair.first << " "  << l_pair.second << std::endl;
                    checked++;
                    if (l_pair.second >= _flowLimit) {
                        tight++;
                    }
                }
            }
        }
        std::cout << ">>>> Checked " << checked << " Tight " << tight << std::endl;
    }
}
