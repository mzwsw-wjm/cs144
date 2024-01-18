#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "parser.hh"
#include <cstdint>
#include <optional>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
// cppcheck-suppress uninitMemberVar
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : ethernet_address_(ethernet_address), ip_address_(ip_address)
{
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(ethernet_address_) << " and IP address "
         << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop)
{
    uint32_t ip = next_hop.ipv4_numeric();
    cout << "IP is : " << next_hop.ip() << endl;
    cout << "uint_32 IP is : " << ip << endl;
    if(arp_table_.contains(ip)) {
        //map 中存在这个地址
        EthernetFrame eth;
        eth.header.dst = arp_table_.at(ip).eth_addr;
        eth.header.src = ethernet_address_;
        eth.header.type = EthernetHeader::TYPE_IPv4;
        eth.payload = serialize(dgram);
        outbound_frames_.emplace(eth);
    }   
    else {
        //map中没有该地址
        if(!arp_requests_lifetime_.contains(ip)) {
            ARPMessage arp;
            arp.sender_ip_address = ip_address_.ipv4_numeric();
            arp.target_ip_address = ip;
            arp.sender_ethernet_address = ethernet_address_;
            arp.target_ethernet_address = {};
            arp.opcode = ARPMessage::OPCODE_REQUEST;

            EthernetFrame eth;
            eth.header.dst = ETHERNET_BROADCAST;
            eth.header.src = ethernet_address_;
            eth.header.type = EthernetHeader::TYPE_ARP;
            eth.payload = serialize(arp);
            outbound_frames_.emplace(eth);

            arp_requests_lifetime_.emplace(ip, ARP_REQUEST_DEFAULT_TTL);
        }
        arp_datagrams_waiting_list_.emplace_back(next_hop, dgram);
    }
}

//frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame)
{
    if(frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) { return nullopt; }
    
    if(frame.header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ip_data;
        if(parse(ip_data, frame.payload)) { return make_optional<InternetDatagram>(ip_data); }
        return nullopt;
    }
    if(frame.header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if(!parse(arp, frame.payload)) { return nullopt; }
        arp_table_.emplace(arp.sender_ip_address, arp_t{arp.sender_ethernet_address, ARP_DEFAULT_TTL});
        arp_requests_lifetime_.erase(arp.sender_ip_address);
        if(arp.opcode == ARPMessage::OPCODE_REQUEST) {
            if(arp.target_ip_address == ip_address_.ipv4_numeric()) {
                //寻求的ip是此ip
                ARPMessage re;
                re.sender_ip_address = ip_address_.ipv4_numeric();
                re.target_ip_address = arp.sender_ip_address;
                re.sender_ethernet_address = ethernet_address_;
                re.target_ethernet_address = arp.sender_ethernet_address;
                re.opcode = ARPMessage::OPCODE_REPLY;

                EthernetFrame eth;
                eth.header.dst = arp.sender_ethernet_address;
                eth.header.src = ethernet_address_;
                eth.header.type = EthernetHeader::TYPE_ARP;
                eth.payload = serialize(re);
                outbound_frames_.emplace(eth);
            }
        }
        if(arp.opcode == ARPMessage::OPCODE_REPLY) {
            for(auto it = arp_datagrams_waiting_list_.begin(); it != arp_datagrams_waiting_list_.end(); ) {
                if(arp_table_.contains(it->first.ipv4_numeric())) {
                    send_datagram(it->second, it->first);
                    arp_datagrams_waiting_list_.erase(it++);
                }
                else { it++; }
            }
        }
    }
    return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
    for(auto it = arp_table_.begin(); it != arp_table_.end(); ) {
        //arp_table
        if(it->second.ttl <= ms_since_last_tick) { arp_table_.erase(it++); }
        else {
            it->second.ttl -= ms_since_last_tick;
            it++;
        }
    }

    for(auto it = arp_requests_lifetime_.begin(); it != arp_requests_lifetime_.end(); ) {
        if(it->second <= ms_since_last_tick) {
            //resend
            ARPMessage arp;
            arp.opcode = ARPMessage::OPCODE_REQUEST;
            arp.sender_ip_address = ip_address_.ipv4_numeric();
            arp.target_ip_address = it->first;
            arp.sender_ethernet_address = ethernet_address_;
            arp.target_ethernet_address = {};

            EthernetFrame eth;
            eth.header.dst = ETHERNET_BROADCAST;
            eth.header.src = ethernet_address_;
            eth.header.type = EthernetHeader::TYPE_ARP;
            eth.payload = serialize(arp);
            outbound_frames_.emplace(eth);
            it->second = ARP_REQUEST_DEFAULT_TTL;
            it++;
        }
        else {
            it->second -= ms_since_last_tick;
            it++;
        }
    }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
    if(!outbound_frames_.empty()) {
        EthernetFrame eth = outbound_frames_.front();
        outbound_frames_.pop();
        return make_optional<EthernetFrame>(eth);
    }
    return nullopt;
}
