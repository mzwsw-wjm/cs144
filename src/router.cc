#include "router.hh"

#include <iostream>
#include <limits>

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

    routing_table_.emplace_back(route_t {route_prefix, prefix_length, next_hop, interface_num});
}

void Router::route()
{
    // scan interfaces to receive InternetDatagram
    for (auto &net_interface : interfaces_) {
        while (std::optional<InternetDatagram> datagram = net_interface.maybe_receive()) {
            if (datagram) {
                InternetDatagram dgram = datagram.value();
                const uint32_t dst_ipaddr_numeric = dgram.header.dst;
                auto largest_matched_iter = routing_table_.end();
                for (auto route = routing_table_.begin(); route != routing_table_.end(); route++) {
                    // zero prefix_length means match all
                    if (route->prefix_length == 0
                        || ((route->route_prefix ^ dst_ipaddr_numeric)
                            >> (static_cast<uint8_t>(32) - route->prefix_length)) == 0) {
                        // update longest prefix matched route
                        if (largest_matched_iter == routing_table_.end()
                            || route->prefix_length > largest_matched_iter->prefix_length) {
                            largest_matched_iter = route;
                        }
                    }
                }

                // check the legitimacy of the incoming datagram
                uint8_t &ttl = dgram.header.ttl;
                if (largest_matched_iter != routing_table_.end() && ttl-- > 1) {
                    // We have changed the dgram content. Checksum needs to be recomputed.
                    dgram.header.compute_checksum();
                    AsyncNetworkInterface &outbound_interface = interface(largest_matched_iter->interface_id);
                    const Address next_addr = largest_matched_iter->next_hop.has_value()
                                                  ? largest_matched_iter->next_hop.value()
                                                  : Address::from_ipv4_numeric(dst_ipaddr_numeric);
                    outbound_interface.send_datagram(dgram, next_addr);
                }; // no route matched (increase code readability)
            }
        }
    }
}
