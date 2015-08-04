#include <memory>
#include "context.h"
#include "distributions.h"
#include "packet.h"
#ifndef __LINK_H__
#define __LINK_H__
namespace PILO {
    class Node;
    /// This is equivalent to bandwidth link in the Python version.
    class Link {
        private:
            Context& _context;
            Distribution<Time>* _latency;
            const BPS _bandwidth;
        public:
            Link(Context& context,
                 Distribution<Time>* latency, // In seconds?
                 BPS bandwidth, // In bps
                 std::shared_ptr<Node> a, // Endpoint
                 std::shared_ptr<Node> b);

            void send(Node* sender, std::shared_ptr<Packet> packet);
            
            void set_up();

            void set_down();

            std::shared_ptr<Node> _a;
            std::shared_ptr<Node> _b;
        private:
            Time _nextSchedulable;
            enum State {
                DOWN = 0,
                UP
            };
            State _state;
    };
}
#endif
