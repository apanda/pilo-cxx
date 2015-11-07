#include "te_controller.h"
#include "packet.h"
#include <algorithm>
#include <boost/functional/hash.hpp>
// I know these are unnecessary here, but I was having some fun.
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
namespace PILO {
TeController::TeController(Context& context, const std::string& name, const Time refresh, const Time gossip,
                           const int max_load, Distribution<bool>* drop)
    : Controller(context, name, refresh, gossip, drop), _maxLoad(max_load) {
    std::cout << "Max load = " << _maxLoad << std::endl;
}

std::pair<Controller::flowtable_db, Controller::deleted_entries> TeController::compute_paths() {
    igraph_vector_t path;
    flowtable_db diffs;
    deleted_entries diffs_negative;
    flowtable_db new_table;
    igraph_t workingCopy;
    std::unordered_map<std::pair<int, int>, int, boost::hash<std::pair<int, int>>> linkUtilization;
    std::cout << _name << " Beginning computation " << std::endl;
    igraph_copy(&workingCopy, &_graph);
    igraph_to_directed(&workingCopy, IGRAPH_TO_DIRECTED_MUTUAL);
    uint64_t admissionControlRejected = 0;
    uint64_t admissionControlTried = 0;

    for (auto link : _links) {
        std::string v1, v2;
        if (is_host_link(link)) {
            continue;
        }
        std::tie(v1, v2) = split_parts(link);
        linkUtilization.emplace(std::make_pair(_vertices.at(v1), _vertices.at(v2)), 0);
        linkUtilization.emplace(std::make_pair(_vertices.at(v2), _vertices.at(v1)), 0);
    }

    for (int v0_idx = 0; v0_idx < _usedVertices; v0_idx++) {
        std::string v0 = _ivertices.at(v0_idx);

        if (_hostAtSwitch.at(v0).empty()) {
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
                            bool recomputed = false;
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

                                if (!is_host_link(link)) {
                                    int n0idx = _vertices.at(sw);
                                    int n1idx = _vertices.at(nh);
                                    auto lpair = std::make_pair(n0idx, n1idx);
                                    linkUtilization[lpair] += 1;
                                    if (linkUtilization.at(lpair) >= _maxLoad) {
                                        igraph_integer_t eid;
                                        igraph_get_eid(&workingCopy, &eid, n0idx, n1idx, IGRAPH_DIRECTED, 0);
                                        assert(eid != -1);
                                        igraph_delete_edges(&workingCopy, igraph_ess_1(eid));
                                        recomputed = true;
                                    }
                                }
                                assert(_links.find(link) != _links.end());
                                new_table[sw][psig] = link;
                                auto rule = _flowDb.at(sw).find(psig);
                                if (rule == _flowDb.at(sw).end() || rule->second != link) {
                                    _flowDb[sw][psig] = link;
                                    diffs[sw][psig] = link;
                                }
                                if (recomputed) {
                                    igraph_get_shortest_path(&workingCopy, &path, NULL, v0_idx, v1_idx, IGRAPH_OUT);
                                    if (path_len > 0 && igraph_vector_size(&path)) {
                                        std::cout << "Warning: Removal made paths infeasible" << std::endl;
                                    }
                                    path_len = igraph_vector_size(&path);
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
                            if (rule == _flowDb.at(sw).end() || rule->second != link) {
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
            if (new_table.find(sw) == new_table.end() || new_table.at(sw).find(sig) == new_table.at(sw).end()) {
                // OK, remove this signature
                diffs_negative[sw].emplace(sig);
            }
        }
    }
    return std::make_pair(diffs, diffs_negative);
}
}
