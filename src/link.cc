#include "link.h"
#include "node.h"
#include <iostream>

namespace PILO {
    Link::Link(Context& context,
         Distribution<Time>* latency, // In seconds?
         BPS bandwidth, // In bps
         Node& a, // Endpoint
         Node& b) :
        _context(context),
        _latency(latency),
        _bandwidth(bandwidth),
        _a(a),
        _b(b) {
    }

    void Link::send(Node& sender, std::shared_ptr<Packet> packet) {
        Time end_time = ((Time)packet->_size) / (_bandwidth);
        end_time += _latency->next();

        if (&_a == &sender) {
            std::cout << "Send from a" << std::endl;
            _context.schedule(end_time, [this, packet] (float) mutable {this->_b.receive(std::move(packet));});
        } else if (&_b == &sender) {
            std::cout << "Send from b" << std::endl;
            _context.schedule(end_time, [this, packet] (float) mutable {this->_a.receive(std::move(packet));});
        }

    }
}
