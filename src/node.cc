#include <iostream>
#include "node.h"

namespace PILO {
    Node::Node(Context& context,
         const std::string& name) :
        _context (context),
        _name (name) {
            (void)_context;
    }
    void Node::receive(std::shared_ptr<Packet> packet) {
        std::cout << _context.now() << "   " <<  _name << " received packet " 
            << packet->_sig << " of size " << packet->_size << " (" << packet.use_count() << ")" << std::endl;
    }
}
