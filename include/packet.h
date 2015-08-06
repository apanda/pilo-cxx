#include <string>
#include <memory>
#include <unordered_map>
#include "link.h"
#ifndef __PACKET_H__
#define __PACKET_H__
namespace PILO {
    class Node;
    class Packet {
        public:
            typedef std::unordered_map<std::string, std::string> flowtable;
            static const std::string WILDCARD;
            static uint64_t pid;
            enum Type {
                DATA = 0,
                CONTROL = 1, // Below this everything is control
                NOP = 1,
                ECHO = 2,
                LINK_UP = 3,
                LINK_DOWN = 4,
                CHANGE_RULES = 5,
                SWITCH_INFORMATION = 6,
                SWITCH_INFORMATION_REQ=7,
                END
            };

            static const std::string IType[];

            enum {
                HEADER = 14 * 8,
                LINK_UP_SIZE = HEADER + 64 + 64, // Header + 64 bit link ID + 64 bit version
                LINK_DOWN_SIZE = HEADER + 64 + 64// Header + 64 bit link ID + 64 bit version
            };

            std::string _source;
            std::string _destination;
            Type _type;
            std::string _sig;
            size_t _size;
            uint64_t _id;
            
            // All the data we would ever possibly need, since I am lazy
            struct {
                std::string link;
                uint64_t version;
                flowtable table;
                std::unordered_map<std::string, Link::State> linkState;
                std::unordered_map<std::string, uint64_t> linkVersion;
            } data;
            // Let us revisit this at some point
            //void* data;

            Packet(std::string source, std::string destination, Type type, size_t size) :
                _source(source),
                _destination(destination),
                _type(type),
                _size(size) {
                    _sig =  generate_signature(_source, _destination, _type);
                    _id = pid;
                    pid++;
#if 0
                    std::cout << "packet_obj " << _id << " created " << std::endl;
#endif
            }

            virtual ~Packet() {
#if 0
                std::cout << "packet_obj " << _id << " destroyed " << std::endl;
#endif
            }

            static std::string generate_signature(const std::string& src, const std::string& dest, Type type) {
                return src + ":" + dest + ":" + std::to_string(type);
            }

            static std::shared_ptr<Packet> make_packet(std::shared_ptr<Node> src, std::shared_ptr<Node> dest,
                    Type type, size_t size);

            static std::shared_ptr<Packet> make_packet(std::shared_ptr<Node> src, Type type, size_t size);

            static std::shared_ptr<Packet> make_packet(std::string src, Packet::Type type, size_t size);

            static std::shared_ptr<Packet> make_packet(std::string src, std::string dest, Type type, size_t size);
    };
}
#endif
