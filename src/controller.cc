#include "controller.h"
#include "packet.h"
#include <algorithm>
#include <boost/functional/hash.hpp>
// I know these are unnecessary here, but I was having some fun.
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
namespace PILO {
    Controller::Controller(Context& context, const std::string& name, const Time refresh, const Time gossip):
        Node(context, name),
        _controllers(),
        _switches(),
        _nodes(),
        _links(),
        _linkVersion(),
        _hostAtSwitch(),
        _hostAtSwitchCount(),
        _vertices(),
        _filter(),
        _refresh(refresh),
        _gossip(gossip),
        _log() {
        // Create an empty graph
        igraph_empty(&_graph, 0, IGRAPH_UNDIRECTED);
        _usedVertices = 0;
        _context.schedule(_refresh, [&](double) {this->send_switch_info_request();});
        _context.schedule(_gossip, [&](double) {this->send_gossip_request();});
    }

    void Controller::receive(std::shared_ptr<Packet> packet, Link* link) {
        // Make sure we have not already received this packet.
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
                case Packet::GOSSIP:
                    handle_gossip(packet);
                    break;
                case Packet::GOSSIP_REP:
                    handle_gossip_rep(packet);
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

    void Controller::handle_gossip(const std::shared_ptr<Packet>& packet) {
        auto response = _log.compute_response(packet);
        if (response.size() > 0) {
            //std::cout << _context.now() << " " << _name << " sending gossip response " << std::endl;
            auto rpacket = Packet::make_packet(_name, packet->_source, Packet::GOSSIP_REP,
                                                Packet::HEADER + response.size() * (64 + 64 + 8));
            rpacket->data.gossipResponse = std::move(response);
            flood(rpacket);
        }
    }

    void Controller::handle_gossip_rep(const std::shared_ptr<Packet>& packet) {
        //std::cout << _context.get_time() << " " << _name << " handle_gossip_rep" << std::endl;
        _log.merge_logs(packet);
    }

    void Controller::apply_patch(std::pair<flowtable_db, deleted_entries>& patch) {
        flowtable_db diff;
        deleted_entries remove;
        std::tie(diff, remove) = patch;
        size_t rule_updates = 0;
        for (auto part : diff) {
            auto dest = part.first;
            auto patch = part.second;
            size_t patch_size = patch.size();

            // Add the size of negation
            if (remove.find(dest) != remove.end()) {
                patch_size += remove.at(dest).size();
            }

            rule_updates += patch_size;
            //std::cout << _context.get_time() << " " << _name << " sending a patch to " << dest << std::endl;
            // Each rule is header + link to go out
            size_t packet_size = Packet::HEADER + patch_size * (64 + Packet::HEADER);
            auto update = Packet::make_packet(_name, dest, Packet::CHANGE_RULES, packet_size);
            update->data.table.swap(patch);
            if (remove.find(dest) != remove.end()) {
                update->data.deleteEntries.swap(remove.at(dest));
            }
            flood(std::move(update));
        }
        std::cout << _context.get_time() << "  " << _name << " patch_size " << rule_updates << std::endl;
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

    void Controller::add_new_link(const std::string& link, uint64_t version) {
        _links.emplace(link);
        _linkVersion.emplace(std::make_pair(link, version));
        _log.open_log_link(link);
    }

    inline bool Controller::add_host_link(const std::string& link) {
        std::string v0, v1;
        std::tie(v0, v1) = split_parts(link);
        if (_nodes.find(v0) != _nodes.end()) {
            assert(_nodes.find(v1) == _nodes.end());
            _hostAtSwitch.at(v1).push_front(v0);
            _hostAtSwitchCount[v1]+=1;
            return true;
        } else if (_nodes.find(v1) != _nodes.end()) {
            _hostAtSwitch.at(v0).push_front(v1);
            _hostAtSwitchCount[v0]+=1;
            return true;
        }
        return false;
    }

    inline bool Controller::remove_host_link(const std::string& link) {
        std::string v0, v1;
        std::tie(v0, v1) = split_parts(link);
        if (_nodes.find(v0) != _nodes.end()) {
            assert(_nodes.find(v1) == _nodes.end());
            _hostAtSwitch.at(v1).remove(v0);
            _hostAtSwitchCount[v1]--;
            return true;
        } else if (_nodes.find(v1) != _nodes.end()) {
            _hostAtSwitch.at(v0).remove(v1);
            _hostAtSwitchCount[v0]--;
            return true;
        }
        return false;
    }

    bool Controller::add_link(const std::string& link, uint64_t version) {

        if (_links.find(link) == _links.end()) {
            add_new_link(link, version);
        } else {
            // We have already seen this link state update (or a newer one)
            if (version <= _linkVersion.at(link)) {
                //std::cout << _context.get_time() << "  " << _name << " rejected due to version " << std::endl;
                return false;
            }
        }

        // Update the version.
        _linkVersion[link] = version;
        _log.add_link_event(link, version, Link::UP);
        // This is mostly to prevent adding a single link many times.
        if (_existingLinks.find(link) != _existingLinks.end()) {
            return false;
        }
        _existingLinks.emplace(link);

        if (!add_host_link(link)) {
            std::string v0, v1;
            std::tie(v0, v1) = split_parts(link);
            igraph_add_edge(&_graph, _vertices.at(v0), _vertices.at(v1));
        }
        return true;
    }

    bool Controller::remove_link(const std::string& link, uint64_t version) {
        if (version <= _linkVersion.at(link)) {
            //std::cout << _context.get_time() << "  " << _name << " rejected due to version " << std::endl;
            return false;
        }

        _linkVersion[link] = version;
        _log.add_link_event(link, version, Link::DOWN);

        if (_existingLinks.find(link) == _existingLinks.end()) {
            return false;
        }

        _existingLinks.erase(link);
        if (!remove_host_link(link)) {
            std::string v0, v1;
            std::tie(v0, v1) = split_parts(link);

            igraph_integer_t eid;
            igraph_get_eid(&_graph, &eid, _vertices.at(v0), _vertices.at(v1), IGRAPH_UNDIRECTED, 0);
            assert(eid != -1);
            igraph_delete_edges(&_graph, igraph_ess_1(eid));
        }
        return true;
    }

    void Controller::add_controllers(controller_map controllers) {
        //int count = 0;
        for (auto controller : controllers) {
            _controllers.emplace(controller.first);
            _nodes.emplace(controller.first);
            //igraph_integer_t idx = count + _usedVertices;
            //_vertices[controller.first] = idx;
            //_ivertices[idx] = controller.first;
            //count++;
        }
        //igraph_add_vertices(&_graph, count, NULL);
        //_usedVertices += count;
    }

    void Controller::add_switches(switch_map switches) {
        int count = 0;
        for (auto sw : switches) {
            _switches.emplace(sw.first);
            _hostAtSwitch.emplace(std::make_pair(sw.first, std::forward_list<std::string>()));
            _hostAtSwitchCount.emplace(std::make_pair(sw.first, 0ll));
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
        //int count = 0;
        for (auto node : nodes) {
            _nodes.emplace(node.first);
            //igraph_integer_t idx = count + _usedVertices;
            //_vertices[node.first] = idx;
            //_ivertices[idx] = node.first;
            //count++;
        }
        //igraph_add_vertices(&_graph, count, NULL);
        //_usedVertices += count;
    }

    std::pair<Controller::flowtable_db, Controller::deleted_entries> Controller::compute_paths() {
        igraph_vector_t path;
        flowtable_db diffs;
        deleted_entries diffs_negative;
        flowtable_db new_table;
        igraph_t workingCopy;
        std::cout << _name << " Beginning computation " << std::endl;
        igraph_copy(&workingCopy, &_graph);
        igraph_to_directed(&workingCopy, IGRAPH_TO_DIRECTED_MUTUAL);
        uint64_t admissionControlRejected = 0;
        uint64_t admissionControlTried = 0;

        for (int v0_idx = 0; v0_idx < _usedVertices; v0_idx++) {
            std::string v0 = _ivertices.at(v0_idx);

            if(_hostAtSwitch.at(v0).empty()) {
                continue;
            }

            for (int v1_idx = 0; v1_idx < _usedVertices; v1_idx++) {
                std::string v1 = _ivertices.at(v1_idx);
                if ((!_hostAtSwitch.at(v1).empty())) {
                    if (v0_idx != v1_idx) {
                        igraph_vector_init(&path, 0);
                        igraph_get_shortest_path(&workingCopy, &path, NULL, v0_idx, v1_idx, IGRAPH_OUT);
                        for (auto h0 : _hostAtSwitch.at(v0)) {
                            for (auto h1 : _hostAtSwitch.at(v1)) {
                                std::string psig = Packet::generate_signature(h0, h1, Packet::DATA);
                                int path_len = igraph_vector_size(&path);

                                admissionControlTried++;

                                if (path_len == 0) {
                                    admissionControlRejected++;
                                }

                                for (int k = 0; k < path_len; k++) {
                                    std::string sw = _ivertices.at(VECTOR(path)[k]);
                                    std::string nh;
                                    if (k + 1 < path_len) {
                                        nh = _ivertices.at(VECTOR(path)[k + 1]);
                                    } else {
                                        nh = h1;
                                    }
                                    std::string link = sw + "-" + nh;

                                    // Cannonicalize
                                    if (_links.find(link) == _links.end()) {
                                        link = nh + "-" + sw;
                                    }
                                    assert(_links.find(link) != _links.end());
                                    new_table[sw][psig] = link;
                                    auto rule = _flowDb.at(sw).find(psig);
                                    if (rule == _flowDb.at(sw).end() ||
                                        rule->second != link) {
                                        _flowDb[sw][psig] = link;
                                        diffs[sw][psig] = link;
                                    }
                                }
                            }
                        }
                        igraph_vector_destroy(&path);
                    } else if (v0_idx == v1_idx) {
                        // Set up paths between things connected to the same switch.
                        for (auto h0 : _hostAtSwitch.at(v0)) {
                            for (auto h1 : _hostAtSwitch.at(v1)) {
                                if (h0 == h1) {
                                    continue;
                                }
                                std::string psig = Packet::generate_signature(h0, h1, Packet::DATA);
                                std::string link = v0 + "-" + h1;
                                auto sw = v0;
                                // Cannonicalize
                                if (_links.find(link) == _links.end()) {
                                    link = h1 + "-" + v0;
                                }
                                assert(_links.find(link) != _links.end());
                                new_table[sw][psig] = link;
                                auto rule = _flowDb.at(sw).find(psig);
                                if (rule == _flowDb.at(sw).end() ||
                                    rule->second != link) {
                                    _flowDb[sw][psig] = link;
                                    diffs[sw][psig] = link;
                                }
                            }
                        }
                    }
                }
            }
        }
        igraph_destroy(&workingCopy);
        std::cout << _name << " Done computing " << admissionControlTried << "   " << admissionControlRejected << std::endl;
        for (auto swtable : _flowDb) {
            auto sw = swtable.first;
            auto table = swtable.second;
            for (auto match_action : table) {
                auto sig = match_action.first;
                if (new_table.find(sw) == new_table.end() ||
                    new_table.at(sw).find(sig) == new_table.at(sw).end()) {
                    // OK, remove this signature
                    diffs_negative[sw].emplace(sig);
                }
            }
        }
        return std::make_pair(diffs, diffs_negative);
    }

    void Controller::send_switch_info_request() {
        //std::cout << _context.get_time() << " " << _name << " switch info request starting " << std::endl;
        auto req = Packet::make_packet(_name, Packet::SWITCH_INFORMATION_REQ, Packet::HEADER);
        flood(std::move(req));
        _context.schedule(_refresh, [&](double) {this->send_switch_info_request();});
    }

    void Controller::send_gossip_request() {
        auto req = Packet::make_packet(_name, Packet::GOSSIP, Packet::HEADER);
        _log.compute_gaps(req);
        flood(std::move(req));
        _context.schedule(_gossip, [&](double) {this->send_gossip_request();});
    }

    Log::Log() :
        _log(),
        _commit(),
        _marked(),
        _sizes(),
        _max() {
    }

    void Log::open_log_link(const std::string& link) {
        _log.emplace(std::make_pair(link, std::vector<Link::State>(INITIAL_SIZE)));
        _commit.emplace(std::make_pair(link, std::vector<bool>(INITIAL_SIZE)));
        // Nothing has been marked yet
        _max.emplace(std::make_pair(link, 0));
        _marked.emplace(std::make_pair(link, 1));
        _sizes.emplace(std::make_pair(link, INITIAL_SIZE));

        _log.at(link).emplace(_log[link].begin(), Link::DOWN);
        _commit.at(link).emplace(_commit[link].begin(), true);
    }

    void Log::add_link_event(const std::string& link, uint64_t version, Link::State state) {
        assert(_log.find(link) != _log.end());
        while (unlikely(version >= _sizes.at(link))) {
            //std::cout << "Growing log" << std::endl;
            size_t new_size;
            size_t size = _sizes.at(link);
            if (likely(size < HWM)) {
                new_size = size * 2;
            } else {
                // Be cowardly and prevent unbounded growth.
                new_size = version + GROW;
            }
            _log.at(link).resize(new_size);
            _commit.at(link).resize(new_size);
            _sizes[link] = new_size;
        }
        _log[link].emplace(_log[link].begin() + version, state);
        _commit[link].emplace(_commit[link].begin() + version, true);
        if (_max.at(link) < version) {
            _max[link] = version;
        }
    }

    void Log::compute_gaps(const std::shared_ptr<Packet>& packet) {
        size_t packet_size = 0;
        for (auto l : _max) {
            packet_size += (64 + 64); // 64 bit for link ID, 64 bit for version
            packet->data.logMax.emplace(l.first, l.second);
            size_t gap_size;
            packet->data.gaps.emplace(l.first, std::move(compute_link_gap(l.first, gap_size)));
            packet_size += (gap_size * 2 * 64); // 64 bit for each side of the gap
        }
        packet->_size += packet_size;

    }

    std::vector<uint64_t> Log::compute_link_gap(const std::string& link, size_t& gaps_found) {
        std::vector<uint64_t> gaps;
        uint64_t i = _marked.at(link);
        bool first = true;
        gaps_found = 0;
        while (i < _max.at(link)) {
            // Increment i until we find a missing piece.
            while (_commit.at(link).at(i)) i++;
            if (first) {
                _marked[link] = i;
                first = false;
            }
            if (i < _max.at(link)) {
                //std::cout << "Found gap" << std::endl;
                // Found a gap
                gaps.push_back(i);
                // Figure out where gap ends
                while (!_commit.at(link).at(i) && i < _max.at(link)) i++;
                assert(i <= _max.at(link));
                gaps.push_back(i);
                gaps_found += 1;
            }
        }
        return gaps;
    }

    std::vector<Packet::GossipLog> Log::compute_response(const std::shared_ptr<Packet>& packet) {
        assert(packet->_type == Packet::GOSSIP);
        std::vector<Packet::GossipLog> log;
        for (auto lv : packet->data.logMax) {
            // Newer entries than are known
            auto link = lv.first;
            if (lv.second < _max.at(link)) {
                for (uint64_t i = lv.second; i <= _max.at(link); i++) {
                    if (_commit.at(link).at(i)) {
                        log.emplace_back(Packet::GossipLog{.link=lv.first,
                                                           .state=_log.at(link).at(i),
                                                           .version=i});
                    }
                }
            }

            // See if we can fill any gaps
            for (size_t idx = 0; idx < packet->data.gaps.at(link).size(); idx+=2) {
                uint64_t begin = packet->data.gaps.at(link).at(idx);
                uint64_t end = std::min(packet->data.gaps.at(link).at(idx + 1), _max.at(link));
                for (uint64_t i = begin; i < end; i++) {
                    if (_commit.at(link).at(i)) {
                        log.emplace_back(Packet::GossipLog{.link=lv.first,
                                                           .state=_log.at(link).at(i),
                                                           .version=i});
                    }
                }
            }
        }
        return log;
    }

    void Log::merge_logs(const std::shared_ptr<Packet>& packet) {
        assert(packet->_type == Packet::GOSSIP_REP);
        for (auto log : packet->data.gossipResponse) {
            if (!_commit.at(log.link).at(log.version)) {
                _commit.at(log.link)[log.version] = true;
                _log.at(log.link)[log.version] = log.state;
                if (_max.at(log.link) < log.version) {
                    _max[log.link] = log.version;
                }
            }
        }
    }
}
