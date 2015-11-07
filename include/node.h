#include <memory>
#include <unordered_map>
#include "context.h"
#include "packet.h"
#include "link.h"
#ifndef __NODE_H__
#define __NODE_H__
namespace PILO {
class Link;
/// A base node, also an end host.
class Node {
    friend class Simulation;

   protected:
    Context& _context;

   public:
    Node(Context& context, const std::string& name);

    virtual void receive(std::shared_ptr<Packet> packet, Link* link);

    virtual void notify_link_existence(Link* link);

    virtual void notify_link_up(Link* link) {
        // std::cout << _context.get_time() << " Link up " << link->name() << std::endl;
    }

    virtual void notify_link_down(Link* link) {
        // std::cout << _context.get_time() << " Link down " << link->name() << std::endl;
    }

    virtual void silent_link_up(Link*) {}

    virtual void silent_link_down(Link*) {}

    void flood(std::shared_ptr<Packet> packet);

    void flood(std::shared_ptr<Packet> packet, const std::string& l);

    const std::string _name;

   protected:
    std::unordered_map<std::string, Link*> _links;
};
}
#endif
