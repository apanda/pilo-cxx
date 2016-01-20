#include "node.h"
#include "link.h"
#include <unordered_map>
#include <unordered_set>
#ifndef __SWITCH_H__
#define __SWITCH_H__
// Equivalent to LSSwitch in Python
namespace PILO {
class Switch : public Node {
    friend class Simulation;

   public:
    Switch(Context& context, const std::string& name, const bool version);

    virtual void receive(std::shared_ptr<Packet> packet, Link* link);

    virtual void notify_link_existence(Link* link);

    virtual void notify_link_up(Link*);

    virtual void notify_link_down(Link*);

    virtual void silent_link_up(Link*);

    virtual void silent_link_down(Link*);

    void install_flow_table(const Packet::flowtable& table);

    void install_flow_table(const Packet::flowtable& table, const std::unordered_set<std::string>& remove);

   private:
    bool install_flow_table_internal(const Packet::flowtable& table);
    std::unordered_map<std::string, Link::State> _linkState;
    std::unordered_map<std::string, int32_t> _linkStats;  // Assume < 2^31 paths through a link.
    std::unordered_set<uint64_t> _filter;
    Packet::flowtable _forwardingTable;
    uint64_t _version;  // A way to track the number of routing table changes.
    uint64_t _entries;  // Number of routing table entries
    bool _filter_version;
};
}
#endif
