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
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  rtable.push_back(make_pair(make_pair(route_prefix, prefix_length), make_pair(next_hop, interface_num)));
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(auto itf : _interfaces) {
    while(!(itf->datagrams_received().empty())) {
      auto dgram = itf->datagrams_received().front();
      itf->datagrams_received().pop();
      if (dgram.header.ttl == 0 || --dgram.header.ttl == 0) return;
      dgram.header.compute_checksum();
      uint32_t dst {dgram.header.dst};
      int j {-1};
      for(size_t i = 0; i < rtable.size(); i++) {
        auto r = rtable[i];
        uint32_t route_prefix = r.first.first;
        uint8_t prefix_length = r.first.second;
        if(prefix_length == 0 && route_prefix == 0) {
          if(j == -1) j = i;
        }
        else if((dst >> (32 - prefix_length) << (32 - prefix_length)) == route_prefix) {
          if(j < 0 || rtable[j].first.second < prefix_length) j = static_cast<int>(i);
        }
      }
      if(j >= 0) {
        if(rtable[j].second.first.has_value())
          interface(rtable[j].second.second)->send_datagram(dgram, rtable[j].second.first.value());
        else
          interface(rtable[j].second.second)->send_datagram(dgram, Address::from_ipv4_numeric(dgram.header.dst));
      }
    }
  }
}
