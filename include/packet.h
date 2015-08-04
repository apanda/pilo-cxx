#include <string>
#include <memory>
#ifndef __PACKET_H__
#define __PACKET_H__
namespace PILO {
    class Node;
    class Packet {
        public:
            static const std::string WILDCARD;
            enum Type {
                DATA = 0,
                CONTROL = 1, // Below this everything is control
                NOP = 1,
                ECHO,
                LINK_UP,
                LINK_DOWN,
                SWITCH_UP
            };

            std::string _source;
            std::string _destination;
            Type _type;
            std::string _sig;
            size_t _size;
            // Let us revisit this at some point
            void* data;

            Packet(std::string source, std::string destination, Type type, size_t size) :
                _source(source),
                _destination(destination),
                _type(type),
                _size(size) {
                    _sig = source + ":" + destination + ":" + std::to_string(type);
            }

            static std::shared_ptr<Packet> make_packet(std::shared_ptr<Node> src, std::shared_ptr<Node> dest,
                    Type type, size_t size);

            static std::shared_ptr<Packet> make_packet(std::shared_ptr<Node> src, Type type, size_t size);

            static std::shared_ptr<Packet> make_packet(std::string src, std::string dest, Type type, size_t size);
    };
}
#endif
