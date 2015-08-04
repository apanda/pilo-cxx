#include <iostream>
#include "node.h"

namespace PILO {
    Node::Node(Context& context,
         const std::string& name) :
        _context (context),
        _name (name) {
            (void)_context;
    }
    void Node::receive(void* packet, size_t size) {
        std::cout << _name << " received packet of size " << size << std::endl;
    }
}
