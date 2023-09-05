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
  auto ptr = router_;
  for ( int i = 0; i < prefix_length; i++ ) {
    auto bit = ( route_prefix >> ( 31 - i ) ) & 1;
    if ( bit ) {
      if ( ptr->one == nullptr )
        ptr->one.reset( new TrieNode );
      ptr = ptr->one;
    } else {
      if ( ptr->zero == nullptr )
        ptr->zero.reset( new TrieNode );
      ptr = ptr->zero;
    }
  }
  if ( next_hop )
    ptr->next_hop = *next_hop;
  ptr->interface_num = interface_num;
}

void Router::route()
{
  for ( unsigned i = 0; i < interfaces_.size(); i++ ) {
    std::optional<InternetDatagram> dgram;
    while ( dgram = interfaces_[i].maybe_receive() ) {
      if ( dgram->header.ttl <= 1 )
        continue;
      dgram->header.ttl--;
      auto match = router_, ptr = router_;
      for ( int j = 0; j < 32; j++ ) {
        auto bit = ( dgram->header.dst >> ( 31 - j ) ) & 1;
        ptr = bit ? ptr->one : ptr->zero;
        if ( ptr == nullptr )
          break;
        if ( ptr->interface_num )
          match = ptr;
      }
      if ( !match->interface_num )
        continue;
      Address next_hop = match->next_hop ? *( match->next_hop ) : Address::from_ipv4_numeric( dgram->header.dst );
      dgram->header.compute_checksum();
      interface( *( match->interface_num ) ).send_datagram( *dgram, next_hop );
    }
  }
}
