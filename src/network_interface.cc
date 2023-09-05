#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t raw_address = next_hop.ipv4_numeric();
  if ( ip2eth_.count( raw_address ) && ip2eth_[raw_address].second + 30000 > timer_ ) {
    wait2send_.push_back(
      EthernetFrame { EthernetHeader { ip2eth_[raw_address].first, ethernet_address_, EthernetHeader::TYPE_IPv4 },
                      serialize( dgram ) } );
  } else {
    if ( wait4arp_[raw_address].first.empty() || wait4arp_[raw_address].second + 5000 <= timer_ ) {
      wait4arp_[raw_address].second = timer_;

      ARPMessage arp;
      arp.opcode = ARPMessage::OPCODE_REQUEST;
      arp.sender_ethernet_address = ethernet_address_;
      arp.sender_ip_address = ip_address_.ipv4_numeric();
      arp.target_ip_address = raw_address;

      wait2send_.push_front( EthernetFrame {
        EthernetHeader { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( arp ) } );
    }
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = ethernet_address_;
    arp.sender_ip_address = ip_address_.ipv4_numeric();
    arp.target_ip_address = raw_address;
    wait4arp_[raw_address].first.push(
      EthernetFrame { EthernetHeader { {}, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( dgram ) } );
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ )
    return nullopt;
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) )
      return dgram;
    else
      return nullopt;
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage msg;
    if ( !parse( msg, frame.payload ) )
      return nullopt;
    ip2eth_[msg.sender_ip_address].first = msg.sender_ethernet_address;
    ip2eth_[msg.sender_ip_address].second = timer_;
    if ( msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      ARPMessage rep;
      rep.opcode = ARPMessage::OPCODE_REPLY;
      rep.sender_ethernet_address = ethernet_address_;
      rep.sender_ip_address = ip_address_.ipv4_numeric();
      rep.target_ethernet_address = msg.sender_ethernet_address;
      rep.target_ip_address = msg.sender_ip_address;
      wait2send_.push_front( EthernetFrame {
        EthernetHeader { frame.header.src, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( rep ) } );
    } else if ( msg.opcode == ARPMessage::OPCODE_REPLY ) {
      while ( !wait4arp_[msg.sender_ip_address].first.empty() ) {
        auto frame2send = wait4arp_[msg.sender_ip_address].first.front();
        wait4arp_[msg.sender_ip_address].first.pop();

        frame2send.header.dst = msg.sender_ethernet_address;
        wait2send_.push_back( frame2send );
      }
    }
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timer_ += ms_since_last_tick;
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( wait2send_.empty() )
    return nullopt;
  auto frame = wait2send_.front();
  wait2send_.pop_front();
  return frame;
}
