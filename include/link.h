#include <memory>
#include "context.h"
#include "distributions.h"
#ifndef __LINK_H__
#define __LINK_H__
namespace PILO {
    class Node;
    class Packet;
    /// This is equivalent to bandwidth link in the Python version.
    class Link {
        friend class Simulation;
        private:
            Context& _context;
            std::string _name;
            Distribution<Time>* _latency;
            const BPS _bandwidth;
        public:
            enum State {
                DOWN = 0,
                UP
            };
            Link(Context& context,
                 const std::string& name,
                 Distribution<Time>* latency, // In seconds?
                 BPS bandwidth, // In bps
                 std::shared_ptr<Node> a, // Endpoint
                 std::shared_ptr<Node> b);

            void send(Node* sender, std::shared_ptr<Packet> packet);
            
            inline bool is_up() { return _state == UP; }

            inline std::shared_ptr<Node> get_other(std::shared_ptr<Node> n) { return (n.get() == _a.get() ? _b : _a); }

            inline const std::string& name() {
                return _name;
            }

            std::shared_ptr<Node> _a;
            std::shared_ptr<Node> _b;
        private:

            void set_up();

            void set_down();

            void silent_set_up();

            void silent_set_down();

            Time _nextSchedulable;
            State _state;
    };
}
#endif
