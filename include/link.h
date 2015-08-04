#include <memory.h>
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
                 Node& a, // Endpoint
                 Node& b);
            void send(Node& sender, std::shared_ptr<Packet> packet);
            Node& _a;
            Node& _b;
    };
}
#endif
