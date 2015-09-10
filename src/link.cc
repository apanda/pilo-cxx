#include "link.h"
#include "packet.h"
#include "node.h"
#include <iostream>
#include <algorithm>

namespace PILO {
    Link::Link(Context& context,
         const std::string& name,
         Distribution<Time>* latency, // In seconds?
         BPS bandwidth, // In bps
         std::shared_ptr<Node> a, // Endpoint
         std::shared_ptr<Node> b,
         Distribution<bool>* drop) :
        _context(context),
        _name(name),
        _latency(latency),
        _bandwidth(bandwidth),
        _drop(drop),
        _a(std::move(a)),
        _b(std::move(b)),
        _version(0),
        _nextSchedulable(0),
        _state (DOWN),
        _totalBits(0),
        _bitByType() {
            _a->notify_link_existence(this);
            _b->notify_link_existence(this);
            for (int i = 0; i < Packet::END; i++) {
                _bitByType.emplace(std::make_pair(i, 0));
            }
    }

    void Link::send(Node* sender, std::shared_ptr<Packet> packet) {
        // This link fails "atomically". No packets scheduled for delivery after failure are delivered.
        if (_state == DOWN) {
            return;
        }

        if (!(_drop->next())) {
            std::cout << "VVV dropping" << std::endl;
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
                        this->_totalBits += packet->_size;
                        this->_bitByType[packet->_type] += packet->_size;
                        this->_b->receive(std::move(packet), this);
                    }
            });
        } else if (_b.get() == sender) {
            _context.scheduleAbsolute(end_time, [this, packet] (float) mutable {
                    if (_state == UP) {
                        this->_totalBits += packet->_size;
                        this->_bitByType[packet->_type] += packet->_size;
                        this->_a->receive(std::move(packet), this);
                    }
            });
        }
    }

    void Link::set_up() {
        _state = UP;
        _version++;
        _a->notify_link_up(this);
        _b->notify_link_up(this);
    }

    void Link::set_down() {
        _state = DOWN;
        _version++;
        _a->notify_link_down(this);
        _b->notify_link_down(this);
    }

    void Link::silent_set_up() {
        _state = UP;
        _version++;
        _a->silent_link_up(this);
        _b->silent_link_up(this);
    }

    void Link::silent_set_down() {
        _state = DOWN;
        _version++;
        _a->silent_link_down(this);
        _b->silent_link_down(this);
    }
}
