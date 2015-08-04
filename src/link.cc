#include "link.h"
#include "node.h"

namespace PILO {
    Link::Link(Context& context,
         Time latency, // In seconds?
         BPS bandwidth, // In bps
         Node& a, // Endpoint
         Node& b) :
        _context(context),
        _latency(latency),
        _bandwidth(bandwidth),
        _a(a),
        _b(b) {
    }

    void Link::Send(Node& sender, void* packet, size_t size) {
        Time end_time = ((Time)size) / (_bandwidth);
        end_time += _latency;

        if (&_a == &sender) {
            _context.schedule(end_time, [this, packet, size](float) {this->_b.receive(packet, size);});
        } else if (&_b == &sender) {
            _context.schedule(end_time, [this, packet, size](float) {this->_a.receive(packet, size);});
        }

    }
}
