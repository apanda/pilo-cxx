#include "packet.h"
#include "node.h"
namespace PILO {
    const std::string Packet::WILDCARD = "ALL";
    std::shared_ptr<Packet> Packet::make_packet(std::shared_ptr<Node> src, std::shared_ptr<Node> dest,
                        Packet::Type type, size_t size) {
        return Packet::make_packet(src->_name, dest->_name, type, size);
    }

    std::shared_ptr<Packet> Packet::make_packet(std::shared_ptr<Node> src, Packet::Type type, size_t size) {
        return Packet::make_packet(src->_name, WILDCARD, type, size);
    }

    std::shared_ptr<Packet> Packet::make_packet(std::string src, std::string dest, Packet::Type type, size_t size) {
        return std::make_shared<Packet>(src, dest, type, size);
    }
}
