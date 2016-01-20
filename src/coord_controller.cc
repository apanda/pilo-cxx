#include "coord_controller.h"
#include "packet.h"
#include <algorithm>
#include <boost/functional/hash.hpp>
// I know these are unnecessary here, but I was having some fun.
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
namespace PILO {
std::shared_ptr<Coordinator> Coordinator::_instance;
void Coordinator::RegisterController(CoordinationController* controller) { _controllers.emplace_back(controller); }

void Coordinator::set_context(Context* context) {
    _context = context;
    _lastTime = _context->now();
}

void Coordinator::receive(CoordinationController* controller, std::shared_ptr<Packet> packet, Link* link) {
    if (_lastSeen[packet->_destination] < packet->_id) {
        // New information.
        _lastSeen[packet->_destination] = packet->_id;
        if (_lastTime > _context->now()) _lastTime = 0.0;
        // Need to compute the largest controller to controller path
        Time t = std::max(_context->now() + 2. * _rtt, _lastTime + 2. * _rtt);
        //std::cout << _context->now() << " coordination at " << t << " " << _rtt << std::endl;
        _lastTime = t;
        //std::cout << "Coordinator scheduling for " << t << std::endl;
        _context->scheduleAbsolute(t, [this, packet, link](Time) mutable { send_to_controller(packet, link); });
    }
}

void Coordinator::send_to_controller(std::shared_ptr<Packet> packet, Link* link) {
    for (auto controller : _controllers) {
        CoordinationController* c = controller;
        _context->schedule(0.0, [c, packet, link](Time) mutable { c->receive_coordinator(packet, link); });
    }
}

CoordinationController::CoordinationController(Context& context, const std::string& name, const Time refresh,
                                               const Time gossip, Distribution<bool>* drop)
    : Controller(context, name, refresh, gossip, drop), _coordinator(Coordinator::GetInstance()) {
    _coordinator->RegisterController(this);
    std::cout << "Creating coordination" << std::endl;
}

void CoordinationController::receive(std::shared_ptr<Packet> packet, Link* link) {
    _coordinator->receive(this, packet, link);
}

void CoordinationController::receive_coordinator(std::shared_ptr<Packet> packet, Link* link) {
    std::cout << _context.now() << " " << this->_name << " coordinator sent information " << std::endl;
    Controller::receive(packet, link);
    std::cout << _context.now() << " " << this->_name << " done processing coordinator information " << std::endl;
}
}
