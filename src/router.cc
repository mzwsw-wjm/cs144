#include "address.hh"
#include "router.hh"

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix, const uint8_t prefix_length, const optional<Address> next_hop,
                       const size_t interface_num)
{
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/"
         << static_cast<int>(prefix_length) << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)")
         << " on interface " << interface_num << "\n";
    routing_table_.emplace_back(route_t{route_prefix, prefix_length, next_hop, interface_num});
}

void Router::route()
{
    for(auto &inter : interfaces_) {
        auto data = inter.maybe_receive();
        while(data.has_value()) {
            uint32_t des_addr = data.value().header.dst;
            auto max_p = routing_table_.end();
            for(auto it = routing_table_.begin(); it != routing_table_.end(); it++) {
                bool judge = (it->prefix_length == 0) || (((it->route_prefix ^ des_addr) >> (static_cast<uint8_t>(32) - it->prefix_length)) == 0);
                if(judge && (max_p == routing_table_.end() || (max_p != routing_table_.end() && max_p->prefix_length < it->prefix_length))) {
                    max_p = it;
                }
            }
            if(max_p != routing_table_.end() && data.value().header.ttl-- > 1) {
                data.value().header.compute_checksum();
                if(max_p->next_hop.has_value()) {
                    interface(max_p->interface_id).send_datagram(data.value(), max_p->next_hop.value());
                }
                else {
                    interface(max_p->interface_id).send_datagram(data.value(), Address::from_ipv4_numeric(des_addr));
                }
            }
            data = inter.maybe_receive();
        }
    }
}