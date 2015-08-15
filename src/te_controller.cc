#include "te_controller.h"
#include "packet.h"
#include <algorithm>
#include <boost/functional/hash.hpp>
// I know these are unnecessary here, but I was having some fun.
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
namespace PILO {
    TeController::TeController(Context& context, const std::string& name, const Time refresh, const Time gossip):
        Controller(context, name, refresh, gossip) {
    }

    TeController::flowtable_db TeController::compute_paths() {
        igraph_vector_t l;
        igraph_vector_ptr_t p;
        flowtable_db diffs;
        std::cout << _name << " Beginning computation " << std::endl;

        for (int v0_idx = 0; v0_idx < _usedVertices; v0_idx++) {
            std::string v0 = _ivertices.at(v0_idx);

            if(_hostAtSwitch.at(v0).empty()) {
                continue;
            }

            igraph_vector_ptr_init(&p, 0);
            igraph_vector_init(&l, 0);
            std::cout << _name <<" Computing shortest paths for " << v0 << std::endl;
            int ret = igraph_get_all_shortest_paths(&_graph, &p, &l, v0_idx, igraph_vss_all(), IGRAPH_ALL);
            std::cout << _name <<" Done Computing shortest paths for " << v0 << std::endl;
            int base_idx = 0;

            (void)ret; // I like asserts
            assert(ret == 0);
            int len = igraph_vector_size(&l);
            //std::cout << v0_idx << " all path size " << igraph_vector_ptr_size(&p) << std::endl;

            std::cout << _name << " Generating  " << len << " paths from "  << v0 << std::endl;
            for (int v1_idx = 0; v1_idx < len; v1_idx++) {
                std::string v1 = _ivertices.at(v1_idx);
                int paths = VECTOR(l)[v1_idx];
                //std::cout << v0_idx << "  " << v1_idx << " path size " 
                          //<< paths << " base_idx " << base_idx << std::endl;
                if ((!_hostAtSwitch.at(v1).empty())) {
                    if (v0_idx != v1_idx && paths > 0) {
                        for (auto h0 : _hostAtSwitch.at(v0)) {
                            for (auto h1 : _hostAtSwitch.at(v1)) {
                                std::string psig = Packet::generate_signature(h0, h1, Packet::DATA);
                                size_t hash = _hash(psig);
                                int path_idx = hash % paths;
                                //std::cout << "\t" << bias << " " << paths << " " << path_idx << std::endl;
                                //std::cout << "\t\t" << igraph_vector_ptr_size(&p) << std::endl;
                                assert( path_idx + base_idx < igraph_vector_ptr_size(&p));
                                igraph_vector_t* path = (igraph_vector_t*)VECTOR(p)[path_idx + base_idx];
                                int path_len = igraph_vector_size(path);

                                for (int k = 0; k < path_len; k++) {
                                    std::string sw = _ivertices.at(VECTOR(*path)[k]);
                                    std::string nh;
                                    if (k + 1 < path_len) {
                                        nh = _ivertices.at(VECTOR(*path)[k + 1]);
                                    } else {
                                        nh = h1;
                                    }
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
                                    }
                                }
                            }
                        }
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
                base_idx += paths;
            }
            std::cout << _name << " Done generating  " << len << " paths from "  << v0 << std::endl;

            igraph_vector_ptr_destroy_all(&p);
            igraph_vector_destroy(&l);
            std::cout << _name << " Done destroying  vectors and paths for "  << v0 << std::endl;
        }
        std::cout << _name << " Done computing " << std::endl;
        return diffs;
    }

}
