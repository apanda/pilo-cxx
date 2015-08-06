#include "packet.h"
#include "node.h"
namespace PILO {
    const std::string Packet::WILDCARD = "ALL";
    uint64_t Packet::pid = 0;
    const std::string Packet::IType[] = {
        "DATA",
        "NOP",
        "ECHO",
        "LINK_UP",
        "LINK_DOWN",
        "CHANGE_RULES",
        "SWITCH_INFORMATION",
        "SWITCH_INFORMATION_REQ",
        "END"
    };
    std::shared_ptr<Packet> Packet::make_packet(std::shared_ptr<Node> src, std::shared_ptr<Node> dest,
                        Packet::Type type, size_t size) {
        return Packet::make_packet(src->_name, dest->_name, type, size);
    }

    std::shared_ptr<Packet> Packet::make_packet(std::shared_ptr<Node> src, Packet::Type type, size_t size) {
        return Packet::make_packet(src->_name, WILDCARD, type, size);
    }

    std::shared_ptr<Packet> Packet::make_packet(std::string src, Packet::Type type, size_t size) {
        return std::make_shared<Packet>(src, WILDCARD, type, size);
    }

    std::shared_ptr<Packet> Packet::make_packet(std::string src, std::string dest, Packet::Type type, size_t size) {
        return std::make_shared<Packet>(src, dest, type, size);
    }
}
