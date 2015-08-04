#include "link.h"
#include "node.h"
#include <iostream>
#include <algorithm>

namespace PILO {
    Link::Link(Context& context,
         Distribution<Time>* latency, // In seconds?
         BPS bandwidth, // In bps
         std::shared_ptr<Node> a, // Endpoint
         std::shared_ptr<Node> b) :
        _context(context),
        _latency(latency),
        _bandwidth(bandwidth),
        _a(std::move(a)),
        _b(std::move(b)),
        _nextSchedulable(0),
        _state (DOWN) {
            _a->notify_link_existence(this);
            _b->notify_link_existence(this);
    }

    void Link::send(Node* sender, std::shared_ptr<Packet> packet) {
        // This link fails "atomically". No packets scheduled for delivery after failure are delivered.
        if (_state == DOWN) {
            return;
        }
        Time end_delay = ((Time)packet->_size) / (_bandwidth);
        end_delay += _latency->next();
        Time start_time = std::max(_nextSchedulable, _context.get_time());
        Time end_time = start_time + end_delay;
        _nextSchedulable = end_time;
        if (_a.get() == sender) {
            _context.scheduleAbsolute(end_time, [this, packet] (float) mutable {
                    if (_state == UP) {
                        this->_b->receive(std::move(packet));
                    }
            });
        } else if (_b.get() == sender) {
            _context.scheduleAbsolute(end_time, [this, packet] (float) mutable {
                    if (_state == UP) {
                        this->_a->receive(std::move(packet));
                    }
            });
        }
    }

    void Link::set_up() {
        _state = UP;
        _a->notify_link_up(this);
        _b->notify_link_up(this);
    }

    void Link::set_down() {
        _state = DOWN;
        _a->notify_link_down(this);
        _b->notify_link_down(this);
    }
}
