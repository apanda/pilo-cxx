#include <string>
#include <memory>
#include <map>
#include <unordered_set>
#include <vector>
#include "link.h"
#ifndef __PACKET_H__
#define __PACKET_H__
namespace PILO {
class Node;

// Packets.
class Packet {
   public:
    typedef std::map<std::string, std::string> flowtable;

    // This packet is for all.
    static const std::string WILDCARD;

    // Record packet ID.
    static uint64_t pid;

    // Packet type.
    enum Type {
        DATA = 0,
        CONTROL = 1,  // Below this everything is control
        NOP = 1,
        ECHO = 2,
        LINK_UP = 3,
        LINK_DOWN = 4,
        CHANGE_RULES = 5,
        SWITCH_INFORMATION = 6,
        SWITCH_INFORMATION_REQ = 7,
        GOSSIP = 8,
        GOSSIP_REP = 9,
        SWITCH_TABLE_REQ = 10,
        SWITCH_TABLE_RESP = 11,
        END
    };

    // Pretty print support
    static const std::string IType[];

    enum {
        HEADER = 14 * 8,
        LINK_UP_SIZE = HEADER + 64 + 64,   // Header + 64 bit link ID + 64 bit version
        LINK_DOWN_SIZE = HEADER + 64 + 64  // Header + 64 bit link ID + 64 bit version
    };

    std::string _source;
    std::string _destination;
    Type _type;
    std::string _sig;
    size_t _size;
    uint64_t _id;

    // What to send in gossip responses.
    struct GossipLog {
        std::string link;
        Link::State state;
        uint64_t version;
    };

    // All the data we would ever possibly need, since I am lazy
    struct {
        std::string link;
        size_t version;
        flowtable table;
        std::unordered_set<std::string> deleteEntries;
        std::unordered_map<std::string, Link::State> linkState;
        std::unordered_map<std::string, uint64_t> linkVersion;
        std::unordered_map<std::string, std::vector<uint64_t>> gaps;
        std::unordered_map<std::string, uint64_t> logMax;
        std::vector<GossipLog> gossipResponse;
    } data;

    Packet(std::string source, std::string destination, Type type, size_t size)
        : _source(source), _destination(destination), _type(type), _size(size) {
        _sig = generate_signature(_source, _destination, _type);
        _id = pid;
        pid++;
        data.version = 0;
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

    static std::shared_ptr<Packet> make_packet(std::shared_ptr<Node> src, std::shared_ptr<Node> dest, Type type,
                                               size_t size);

    static std::shared_ptr<Packet> make_packet(std::shared_ptr<Node> src, Type type, size_t size);

    static std::shared_ptr<Packet> make_packet(std::string src, Packet::Type type, size_t size);

    static std::shared_ptr<Packet> make_packet(std::string src, std::string dest, Type type, size_t size);
};
}
#endif
