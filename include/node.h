#include <memory.h>
#include "context.h"
#include "packet.h"
#ifndef __NODE_H__
#define __NODE_H__
namespace PILO {
    class Link;
    /// This is equivalent to bandwidth link in the Python version.
    class Node {
        private:
            Context& _context;
        public:
            Node(Context& context,
                 const std::string& name);
            void receive(std::shared_ptr<Packet> packet); 
            const std::string _name;
    };
}
#endif
