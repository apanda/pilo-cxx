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
            Switch(Context& context,
                 const std::string& name);
            
            virtual void receive(std::shared_ptr<Packet> packet); 

            virtual void notify_link_existence(Link* link);
            
            virtual void notify_link_up(Link*);

            virtual void notify_link_down(Link*);

            virtual void silent_link_up(Link*);

            virtual void silent_link_down(Link*);

            void install_flow_table(const Packet::flowtable& table);

        private:
            std::unordered_map<std::string, Link::State> _linkState;
            std::unordered_set<uint64_t> _filter;
            Packet::flowtable _forwardingTable;
    };
}
#endif
