#include "context.h"
#ifndef __LINK_H__
#define __LINK_H__
namespace PILO {
    class Node;
    /// This is equivalent to bandwidth link in the Python version.
    class Link {
        public:
            Link(Context& context,
                 Time latency, // In seconds?
                 BPS bandwidth, // In bps
                 Node& a, // Endpoint
                 Node& b);
            void Send(Node& sender, void* packet, size_t size);
        private:
            Context& _context;
            const Time _latency;
            const BPS _bandwidth;
            Node& _a;
            Node& _b;
    };
}
#endif
