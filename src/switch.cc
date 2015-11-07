#include "switch.h"
#include "packet.h"
namespace PILO {
Switch::Switch(Context& context, const std::string& name)
    : Node(context, name), _linkState(), _filter(), _forwardingTable() {}

void Switch::receive(std::shared_ptr<Packet> packet, Link* link) {
    // Get the flooding out of the way
    if (packet->_type >= Packet::CONTROL && packet->_destination != _name &&
        _filter.find(packet->_id) == _filter.end()) {
        // std::cout << _name << " Flooding " << packet->_type << "   " << packet->_destination << std::endl;
        flood(packet, link->name());
        _filter.emplace(packet->_id);
    } /*else {*/
    // std::cout << _name << " Not flooding " << packet->_type << "   " << packet->_destination << std::endl;
    // if (!(packet->_type >= Packet::CONTROL)) {
    // std::cout << "\t\t because not control" << std::endl;
    //}
    // if (!(packet->_destination != _name)) {
    // std::cout << "\t\t because not control" << std::endl;
    //}
    // if (!(_filter.find(packet->_id) == _filter.end())) {
    // std::cout << "\t\t because filtered" << std::endl;
    //}
    /*}*/

    if (packet->_type >= Packet::CONTROL &&
        (packet->_destination == _name || packet->_destination == Packet::WILDCARD)) {
        // If the packet is intended for the switch, process it.
        switch (packet->_type) {
        case Packet::CHANGE_RULES:
            install_flow_table(packet->data.table, packet->data.deleteEntries);
            break;
        case Packet::SWITCH_INFORMATION_REQ: {
            auto response = Packet::make_packet(_name, packet->_source, Packet::SWITCH_INFORMATION,
                                                Packet::HEADER + (64 + 64 + 8) * _linkState.size());
            for (auto link : _linkState) {
                response->data.linkState[link.first] = link.second;
                response->data.linkVersion[link.first] = _links.at(link.first)->version();
            }
            flood(response);
        } break;
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

void Switch::install_flow_table(const Packet::flowtable& table) {
    for (auto rules : table) {
        if (_forwardingTable.find(rules.first) != _forwardingTable.end()) {
            if (_forwardingTable.at(rules.first) != rules.second) {
                // Decrement since we are about to change to some other link
                _linkStats.at(_forwardingTable.at(rules.first))--;
                _linkStats.at(rules.second)++;
                _forwardingTable[rules.first] = rules.second;
            }
        } else {
            _linkStats.at(rules.second)++;
            _forwardingTable[rules.first] = rules.second;
        }
    }
}
void Switch::install_flow_table(const Packet::flowtable& table, const std::unordered_set<std::string>& remove) {
    // std::cout << _context.get_time() << " " << _name << " installing rules" << std::endl;
    install_flow_table(table);
    for (auto rule : remove) {
        if (_forwardingTable.find(rule) != _forwardingTable.end()) {
            _linkStats.at(_forwardingTable.at(rule))--;
            _forwardingTable.erase(rule);
        }
    }
}

void Switch::notify_link_existence(Link* link) {
    Node::notify_link_existence(link);
    _linkState.emplace(std::make_pair(link->name(), Link::DOWN));
    _linkStats.emplace(std::make_pair(link->name(), 0));
}

void Switch::notify_link_up(Link* link) {
    Node::notify_link_up(link);
    if (_linkState.at(link->name()) == Link::DOWN) {
        _linkState[link->name()] = Link::UP;
        auto packet = Packet::make_packet(_name, Packet::LINK_UP, Packet::LINK_UP_SIZE);
        packet->data.link = link->name();
        packet->data.version = link->version();
        flood(packet);
    }
}

void Switch::notify_link_down(Link* link) {
    Node::notify_link_down(link);
    if (_linkState.at(link->name()) == Link::UP) {
        _linkState[link->name()] = Link::DOWN;
        auto packet = Packet::make_packet(_name, Packet::LINK_DOWN, Packet::LINK_DOWN_SIZE);
        packet->data.link = link->name();
        packet->data.version = link->version();
        flood(packet);
    }
}

void Switch::silent_link_up(Link* link) {
    Node::silent_link_up(link);
    if (_linkState.at(link->name()) == Link::DOWN) {
        _linkState[link->name()] = Link::UP;
    }
}

void Switch::silent_link_down(Link* link) {
    Node::silent_link_down(link);
    if (_linkState.at(link->name()) == Link::UP) {
        _linkState[link->name()] = Link::DOWN;
    }
}
}
