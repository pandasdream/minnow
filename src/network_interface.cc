#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame frame;
  if(map_.count(next_hop.ipv4_numeric()) == 0) {
    if(send_time_.count(next_hop.ipv4_numeric()) == 0 ||
    (send_time_.count(next_hop.ipv4_numeric()) && send_time_[next_hop.ipv4_numeric()] + 5000 < time_ms_)) {
      frame.header.dst = ETHERNET_BROADCAST;
      frame.header.src = ethernet_address_;
      frame.header.type = EthernetHeader::TYPE_ARP;
      ARPMessage request_arp;
      request_arp.sender_ip_address = ip_address_.ipv4_numeric();
      request_arp.sender_ethernet_address = ethernet_address_;
      request_arp.target_ip_address = next_hop.ipv4_numeric();
      // request_arp.target_ethernet_address = ETHERNET_BROADCAST;
      request_arp.opcode = ARPMessage::OPCODE_REQUEST;
      frame.payload = serialize(request_arp);
      transmit(frame);
      send_time_[next_hop.ipv4_numeric()] = time_ms_;
    }
    wait_vec_[next_hop.ipv4_numeric()].push_back(dgram);
  }
  else{
    frame.header.dst = map_[next_hop.ipv4_numeric()];
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize(dgram);
    transmit(frame);
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if(frame.header.type == EthernetHeader::TYPE_IPv4 && frame.header.dst == ethernet_address_) {
    InternetDatagram dgram;
    if(parse(dgram, frame.payload)) datagrams_received_.push(dgram);
  }
  else {
    ARPMessage arpmessage;
    if(parse(arpmessage, frame.payload)) {
      map_[arpmessage.sender_ip_address] = arpmessage.sender_ethernet_address;
      expire_time_[arpmessage.sender_ip_address] = time_ms_ + 30000;
      if(arpmessage.opcode == ARPMessage::OPCODE_REQUEST &&
      arpmessage.target_ip_address == ip_address_.ipv4_numeric()) {
          ARPMessage reply_arp_message;
          reply_arp_message.opcode = ARPMessage::OPCODE_REPLY;
          reply_arp_message.target_ip_address = arpmessage.sender_ip_address;
          reply_arp_message.target_ethernet_address = arpmessage.sender_ethernet_address;
          reply_arp_message.sender_ethernet_address = ethernet_address_;
          reply_arp_message.sender_ip_address = ip_address_.ipv4_numeric();
          EthernetFrame send_frame;
          send_frame.payload = serialize(reply_arp_message);
          send_frame.header.dst = arpmessage.sender_ethernet_address;
          send_frame.header.src = ethernet_address_;
          send_frame.header.type = EthernetHeader::TYPE_ARP;
          transmit(send_frame);
      }
      for(auto& dgram : wait_vec_[arpmessage.sender_ip_address]) {
        send_datagram(dgram, Address::from_ipv4_numeric(arpmessage.sender_ip_address));
        // dgram = dgram;
      }
      wait_vec_[arpmessage.sender_ip_address].clear();
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time_ms_ += ms_since_last_tick;
  std::vector<uint32_t> tmp;
  for(auto& p : expire_time_) {
    if(expire_time_[p.first] < time_ms_) {
      tmp.push_back(p.first);
    }
  }
  for(auto t : tmp) {
    map_.erase(t);
    expire_time_.erase(t);
  }
}
