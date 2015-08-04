#include <string>
#ifndef __PACKET_H__
#define __PACKET_H__
namespace PILO {
    class Packet {
        public:
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

            Packet(std::string source, std::string destination, Type type, size_t size) :
                _source(source),
                _destination(destination),
                _type(type),
                _size(size) {
                    _sig = source + ":" + destination + ":" + std::to_string(type);
            }
    };
}
#endif
