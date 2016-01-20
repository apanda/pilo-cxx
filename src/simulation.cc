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
Simulation::Simulation(const uint32_t seed, const std::string& configuration, const std::string& topology, bool version,
                       const Time endTime, const Time refresh, const Time gossip, const BPS bw, const int limit,
                       std::unique_ptr<Distribution<bool>>&& drop, std::unique_ptr<Distribution<bool>>&& cdrop)
    : _context(endTime),
      _flowLimit(limit),
      _seed(seed),
      _rng(_seed),
      _dropRng(std::move(drop)),
      _cdropRng(std::move(cdrop)),
      _configuration(YAML::LoadFile(configuration)),
      _topology(YAML::LoadFile(topology)),
      _latency(Distribution<PILO::Time>::get_distribution(_configuration["data_link_latency"], _rng)),
      _hlatency(Distribution<PILO::Time>::get_distribution(_configuration["data_link_hlatency"]
                                                               ? _configuration["data_link_hlatency"]
                                                               : _configuration["data_link_latency"],
                                                           _rng)),
      _vmap(),
      _ivmap(),
      _nsmap(),
      _switches(),
      _controllers(),
      _others(),
      _nodes(std::move(populate_nodes(refresh, gossip, version))),
      _switchLinks(),
      _controllerLinks(),
      _swControllerLinks(),
      _liveLinks(),
      _links(std::move(populate_links(bw))),
      _cLinkRng(0, _controllerLinks.size() - 1, _rng),
      _swLinkRng(0, _switchLinks.size() - 1, _rng),
      _swCLinkRng(0, _swControllerLinks.size() - 1, _rng),
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

Simulation::node_map Simulation::populate_nodes(const Time refresh, const Time gossip, const bool version) {
    igraph_empty(&_graph, 0, IGRAPH_UNDIRECTED);
    node_map nodeMap;
    igraph_integer_t count = 0;
    for (auto& node : _topology) {
        std::string node_str = node.first.as<std::string>();
        if (node_str == LINKS_KEY || node_str == FAIL_KEY || node_str == RUNFILE_KEY || node_str == CRIT_KEY ||
            node_str == HLAT_LINKS_KEY) {
            continue;  // Not a node we want
        }
        std::string type_str = node.second[TYPE_KEY].as<std::string>();

        if (type_str == SWITCH_TYPE) {
            auto sw = std::make_shared<Switch>(_context, node_str, version);
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
            auto c = std::make_shared<CoordinationController>(_context, node_str, refresh, gossip, _cdropRng.get());
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

std::pair<std::string, std::shared_ptr<Link>> Simulation::populate_link(const std::string& link, BPS bw,
                                                                        Distribution<Time>* latency) {
    std::vector<std::string> parts;
    boost::split(parts, link, boost::is_any_of("-"));
    if (_switches.find(parts[0]) != _switches.end() && _switches.find(parts[1]) != _switches.end()) {
        _switchLinks.emplace(link);
        _swControllerLinks.emplace(link);
    } else if ((_controllers.find(parts[0]) != _controllers.end()) ||
               (_controllers.find(parts[1]) != _controllers.end())) {
        _controllerLinks.emplace(link);
        _swControllerLinks.emplace(link);
    }
    return std::make_pair(link, std::make_shared<Link>(_context, link, latency, bw, _nodes.at(parts[0]),
                                                       _nodes.at(parts[1]), _dropRng.get()));
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
        _liveLinks.emplace(link->name());
        return true;
    } else if (_switches.find(link->_b->_name) == _switches.end()) {
        _nsmap.emplace(std::make_pair(link->_b->_name, link->_a->_name));
        _liveLinks.emplace(link->name());
        return true;
    }
    return false;
}

bool Simulation::remove_host_graph_link(const std::shared_ptr<PILO::Link>& link) {
    if (_switches.find(link->_a->_name) == _switches.end()) {
        _nsmap.erase(link->_a->_name);
        _liveLinks.erase(link->name());
        return true;
    } else if (_switches.find(link->_b->_name) == _switches.end()) {
        _nsmap.erase(link->_b->_name);
        _liveLinks.erase(link->name());
        return true;
    }
    return false;
}

void Simulation::add_graph_link(const std::shared_ptr<PILO::Link>& link) {
    if (add_host_graph_link(link))
        return;
    uint32_t e0 = _vmap.at(link->_a->_name);
    uint32_t e1 = _vmap.at(link->_b->_name);
    igraph_integer_t eid;
    igraph_get_eid(&_graph, &eid, e0, e1, IGRAPH_UNDIRECTED, 0);
    if (eid == -1) {
        igraph_add_edge(&_graph, e0, e1);
        _liveLinks.emplace(link->name());
    }
}

void Simulation::remove_graph_link(const std::shared_ptr<PILO::Link>& link) {
    if (remove_host_graph_link(link))
        return;
    uint32_t e0 = _vmap.at(link->_a->_name);
    uint32_t e1 = _vmap.at(link->_b->_name);
    igraph_integer_t eid;
    igraph_get_eid(&_graph, &eid, e0, e1, IGRAPH_UNDIRECTED, 0);
    if (eid != -1) {
        igraph_delete_edges(&_graph, igraph_ess_1(eid));
        _liveLinks.erase(link->name());
    }
}

void Simulation::set_all_controller_links_down() {
    for (auto link : _controllerLinks) {
        set_link_down(link);
    }
}

void Simulation::set_all_controller_links_down(const std::shared_ptr<PILO::Link>& ex) {
    for (auto link : _controllerLinks) {
        if (ex != _links.at(link)) {
            set_link_down(link);
        }
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
    // std::cout << "Controller Diameter " << compute_controller_diameter() << std::endl;
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

void Simulation::set_link_up(const std::string& link) { set_link_up(_links.at(link)); }

void Simulation::set_link_up(const std::shared_ptr<Link>& link) {
    link->set_up();
    add_graph_link(link);
}

void Simulation::set_link_down(const std::string& link) { set_link_down(_links.at(link)); }

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
    size_t min = 1ull << 33, max = 0, count = 0, total = 0;
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
        // Update controller version
        if (sz > 0) {
            for (auto c : _controllers) {
                c.second->_flow_version[diff.first]++;
            }
        }
    }
    std::cout << "Rule sizes: min " << min << " max " << max << " count " << count << " total " << total << std::endl;
}

double Simulation::check_routes(double& global_distance, double& net_distance, double& difference) const {
    uint64_t checked = 0;
    uint64_t passed = 0;
    igraph_matrix_t distances;
    igraph_matrix_init(&distances, 1, 1);
    igraph_shortest_paths(&_graph, &distances, igraph_vss_all(), igraph_vss_all(), IGRAPH_ALL);
    global_distance = 0.0;
    net_distance = 0.0;
    difference = 0.0;
    std::unordered_map<double, int64_t> distance_cdf;

    for (auto h1 : _others) {
        for (auto h2 : _others) {
#if 0
                std::cout << "Going to check" << std::endl;
                std::cout << "\t" << h1.first << "   " << h2.first << "    ";
#endif
            if (h1.first == h2.first) {
                continue;
            }
            // Skip if the underlying topology is disconnected
            if (_nsmap.find(h1.first) == _nsmap.end() || _nsmap.find(h2.first) == _nsmap.end()) {
                continue;
            }
            igraph_integer_t v0 = _vmap.at(_nsmap.at(h1.first));
            igraph_integer_t v1 = _vmap.at(_nsmap.at(h2.first));
            if (MATRIX(distances, v0, v1) == IGRAPH_INFINITY) {
                continue;
            }

            checked += 1;
            std::string sig = Packet::generate_signature(h1.first, h2.first, Packet::DATA);
            std::unordered_set<std::string> visited;
            for (auto begin_link : h1.second->_links) {
                std::string link = begin_link.first;
                auto current = h1.second;
                igraph_real_t measured_distance = 0.0;
                visited.emplace(current->_name);
                while (current.get() != h2.second.get()) {
                    if (_links.at(link)->is_up()) {
                        current = _links.at(link)->get_other(current);
                        measured_distance += 1.0;

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
                        // std::cout << "Broken link " << link << std::endl;
                        break;
                    }
                }
                if (current.get() == h2.second.get()) {
                    igraph_real_t distance = MATRIX(distances, v0, v1);
                    distance += 2.0;  // Tget to switch and back
                    if (measured_distance >= distance) {
                        if ((measured_distance - distance) >= difference) {
                            difference = measured_distance - distance;
                            net_distance = measured_distance;
                            global_distance = distance;
                        }
                        if (distance_cdf.find(difference) == distance_cdf.end()) {
                            distance_cdf.emplace(std::make_pair(difference, 1));
                        } else {
                            distance_cdf[difference] += 1;
                        }
                    } else {
                        std::cout << "WARNING " << measured_distance << " " << distance << std::endl;
                    }
                    passed++;
                    break;
                }
            }
        }
    }
    for (auto cdf : distance_cdf) {
        std::cout << _context.now() << " IDISTANCE " << cdf.first << " " << cdf.second << std::endl;
    }
    for (auto c : _controllers) {
        auto links = &c.second->_existingLinks;
        int64_t count = 0;
        std::cout << _context.now() << " CTRL_LINK_DIFF " << c.first;
        for (auto l : _liveLinks) {
            if (links->find(l) == links->end()) {
                std::cout << " " << l;
                count++;
            }
        }
        std::cout << " " << count << std::endl;
        count = 0;
        std::cout << _context.now() << " CTRL_LINK_EXTRA " << c.first;
        for (auto l : *links) {
            if (_liveLinks.find(l) == _liveLinks.end()) {
                std::cout << " " << l;
                count++;
            }
        }
        std::cout << " " << count << std::endl;
    }
    std::cout << "\t" << _context.now() << " Checked " << checked << "    passed   " << passed << std::endl;
    igraph_matrix_destroy(&distances);

    return ((double)passed) / ((double)checked);
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
    std::cout << _context.now() << " bw  total " << std::fixed << overall_bw << " link " << std::fixed << bw_per_link
              << std::endl;
    std::cout << "\t By type:" << std::endl;
    for (auto per_type : data_by_type) {
        BPS overall = ((per_type.second) / _context.now());
        BPS per_link = overall / _links.size();
        std::cout << "\t\t " << Packet::IType[per_type.first] << " bw total " << std::fixed << overall << " link "
                  << std::fixed << per_link << std::endl;
    }
}

void Simulation::dump_table_changes() const {
    uint64_t overall_changes = 0;
    uint64_t entries = 0;
    for (auto sw_pair : _switches) {
        auto sw = sw_pair.second;
        overall_changes += sw->_version;
        entries += sw->_forwardingTable.size();
        ;
    }
    std::cout << _context.now() << " rule changes " << overall_changes << std::endl;
    std::cout << _context.now() << " entries " << entries << std::endl;
    for (auto c : _controllers) {
        auto ctrl = c.second;
        uint64_t entries = 0;
        for (auto fdb : ctrl->_flowDb) {
            entries += fdb.second.size();
        }
        std::cout << _context.now() << " " << c.first << " thinks there are " << entries << std::endl;
    }
    for (auto c : _controllers) {
        int64_t differences_s = 0;
        int64_t differences_c = 0;
        int64_t differences_h = 0;
        int64_t differences_by_switch = 0;
        auto ctrl = c.second;
        for (auto fdb : ctrl->_flowDb) {
            bool sw_d = false;
            auto sw = _switches.at(fdb.first);
            if (Controller::compute_hash(fdb.second) != Controller::compute_hash(sw->_forwardingTable)) {
                differences_h++;
                sw_d = true;
            }
            for (auto le : fdb.second) {
                if (sw->_forwardingTable.find(le.first) == sw->_forwardingTable.end() ||
                    le.second != sw->_forwardingTable.at(le.first)) {
                    differences_s++;
                    sw_d = true;
                }
            }
            for (auto fe : sw->_forwardingTable) {
                if (fdb.second.find(fe.first) == fdb.second.end()) {
                    differences_c++;
                    sw_d = true;
                }
            }
            if (sw_d) {
                differences_by_switch++;
            }
        }
        std::cout << _context.now() << " " << c.first << " FLOW_DIFF " << differences_s << " " << differences_c << " "
                  << differences_h << " " << differences_by_switch << std::endl;
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
                std::cout << "\t\t" << name << " " << l_pair.first << " " << l_pair.second << std::endl;
                checked++;
                if (l_pair.second >= _flowLimit) {
                    tight++;
                }
            }
        }
    }
    std::cout << ">>>> Checked " << checked << " Tight " << tight << std::endl;
}

void Simulation::reset_links() {
    for (auto l : _links) {
        l.second->reset();
    }
}
}
