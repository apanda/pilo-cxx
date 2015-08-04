#include <memory>
#include <vector>
#include "context.h"
#include "packet.h"
#include "link.h"
#ifndef __NODE_H__
#define __NODE_H__
namespace PILO {
    class Link;
    /// This is equivalent to bandwidth link in the Python version.
    class Node {
        private:

            Context& _context;
        public:
            Node(Context& context,
                 const std::string& name);
            
            void receive(std::shared_ptr<Packet> packet); 

            virtual void notify_link_existence(Link* link);
            
            virtual void notify_link_up(Link*) {}

            virtual void notify_link_down(Link*) {}
            
            void flood(std::shared_ptr<Packet> packet);
            
            const std::string _name;

        private:
            std::vector<Link*> _links;
    };
}
#endif
