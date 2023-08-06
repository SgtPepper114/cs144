#include "tcp_receiver.hh"
#include <cmath>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    ISN_ = message.seqno;
    message.seqno = message.seqno + 1;
  }
  if ( ISN_ )
    reassembler.insert( message.seqno.unwrap( *ISN_ + 1, inbound_stream.bytes_pushed() ),
                        message.payload,
                        message.FIN,
                        inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage ack;

  if ( ISN_ )
    ack.ackno = Wrap32::wrap( inbound_stream.bytes_pushed() + inbound_stream.is_closed(), *ISN_ + 1 );
  else
    ack.ackno = nullopt;

  ack.window_size = min( inbound_stream.available_capacity(), 0xffffUL );

  return ack;
}
