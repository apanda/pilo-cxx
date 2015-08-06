#include <memory>
#include <unordered_map>
#include "context.h"
#include "distributions.h"
#ifndef __LINK_H__
#define __LINK_H__
namespace PILO {
    class Node;
    class Packet;
    /// This is equivalent to bandwidth link in the Python version.
    class Link {
        friend class Simulation;
        private:
            Context& _context;
            std::string _name;
            Distribution<Time>* _latency;
            const BPS _bandwidth;
        public:
            enum State {
                DOWN = 0,
                UP
            };
            Link(Context& context,
                 const std::string& name,
                 Distribution<Time>* latency, // In seconds?
                 BPS bandwidth, // In bps
                 std::shared_ptr<Node> a, // Endpoint
                 std::shared_ptr<Node> b);

            void send(Node* sender, std::shared_ptr<Packet> packet);
            
            inline bool is_up() const { return _state == UP; }

            inline std::shared_ptr<Node> get_other(std::shared_ptr<Node> n) const  { 
                return (n.get() == _a.get() ? _b : _a); }

            inline const std::string& name() const {
                return _name;
            }

            inline uint64_t version() const {
                return _version;
            }

            std::shared_ptr<Node> _a;
            std::shared_ptr<Node> _b;

        private:

            void set_up();

            void set_down();

            void silent_set_up();

            void silent_set_down();

            uint64_t _version;
            Time _nextSchedulable;
            State _state;
            size_t _totalBits;
            std::unordered_map<int32_t, size_t> _bitByType;
    };
}
#endif
