#include "context.h"
#ifndef __NODE_H__
#define __NODE_H__
namespace PILO {
    class Link;
    /// This is equivalent to bandwidth link in the Python version.
    class Node {
        public:
            Node(Context& context,
                 const std::string& name);
            void receive(void* packet, size_t size); 
        private:
            Context& _context;
            const std::string& _name;
    };
}
#endif
