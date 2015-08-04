#include <iostream>
#include "node.h"

namespace PILO {
    Node::Node(Context& context,
         const std::string& name) :
        _context (context),
        _name (name),
        _links() {
            (void)_context;
    }
    
    void Node::receive(std::shared_ptr<Packet> packet) {
        std::cout << _context.now() << "   " <<  _name << " received packet " 
            << packet->_sig << " of size " << packet->_size << " (" << packet.use_count() << ")" << std::endl;
    }
    
    void Node::notify_link_existence(Link* link) {
        // Add link
        _links.emplace(std::make_pair(link->name(), link));
    }

    void Node::flood(std::shared_ptr<Packet> packet) {
        for (auto link : _links) {
            link.second->send(this, packet);
        }
    }
}
