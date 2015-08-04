#include "switch.h"
#include "packet.h"
namespace PILO {
    Switch::Switch(Context& context, const std::string& name):
        Node(context, name),
        _linkState() {
    }

    void Switch::receive(std::shared_ptr<Packet> packet) {
        // Get the flooding out of the way
        if (packet->_type >= Packet::CONTROL &&
            packet->_destination != _name &&
            _filter.count(packet->_id) == 0) {
            flood(packet);
            _filter.emplace(packet->_id);
        }

        if (packet->_type >= Packet::CONTROL &&
             (packet->_destination == _name ||
              packet->_destination == Packet::WILDCARD)) {
            // If the packet is intended for the switch, process it.
            switch (packet->_type) {
                case Packet::CHANGE_RULES:
                    install_flow_table(packet->data.table);
                    break;
                case Packet::SWITCH_INFORMATION: {
                        auto response = Packet::make_packet(_name, packet->_source, 
                                Packet::SWITCH_INFORMATION_REQ, 
                                Packet::HEADER + (64 + 8) * _linkState.size());
                        for (auto link : _linkState) {
                            response->data.linkState[link.first] = link.second;
                        }
                        flood(response);
                    }
                    break;
                default:
                    break;
                    // Do nothing
            }
        }

        if (packet->_type == Packet::DATA) {
            auto rule = _forwardingTable.find(packet->_sig);
            if (rule != _forwardingTable.end()) {
                _links.at(rule->second)->send(this, packet);
            }
        }
    }

    void Switch::install_flow_table(const std::unordered_map<std::string, std::string>& table) {
        for (auto rules : table) {
            _forwardingTable[rules.first] = rules.second;
        }
    }

    void Switch::notify_link_existence(Link* link) {
        Node::notify_link_existence(link);
        _linkState.emplace(std::make_pair(link->name(), Link::DOWN));
    }
    
    void Switch::notify_link_up(Link* link) {
        Node::notify_link_up(link);
        if (_linkState.at(link->name()) == Link::DOWN) {
            _linkState[link->name()] = Link::UP;
            auto packet = Packet::make_packet(_name, Packet::LINK_UP, Packet::LINK_UP_SIZE);
            packet->data.link = link->name();
            flood(packet);
        }
    }

    void Switch::notify_link_down(Link* link) {
        Node::notify_link_down(link);
        if (_linkState.at(link->name()) == Link::UP) {
            _linkState[link->name()] = Link::DOWN;
            auto packet = Packet::make_packet(_name, Packet::LINK_DOWN, Packet::LINK_DOWN_SIZE);
            packet->data.link = link->name();
            flood(packet);
        }
    }

    void Switch::silent_link_up(Link* link) {
        Node::silent_link_up(link);
        std::cout << "Link up (shhhhh) " << link->name() << std::endl;
        if (_linkState.at(link->name()) == Link::DOWN) {
            _linkState[link->name()] = Link::UP;
        }
    }

    void Switch::silent_link_down(Link* link) {
        Node::silent_link_down(link);
        std::cout << "Link down (shhhhh) " << link->name() << std::endl;
        if (_linkState.at(link->name()) == Link::UP) {
            _linkState[link->name()] = Link::DOWN;
        }
    }
}
