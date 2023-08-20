#include "tcp_sender.hh"
#include "random.hh"
#include "tcp_config.hh"
#include <iostream>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn ) : initial_RTO_ms_( initial_RTO_ms )
{
  auto rd = get_random_engine();
  if ( fixed_isn )
    isn_ = *fixed_isn;
  else
    isn_ = Wrap32( rd() );
  RTO_ms_ = initial_RTO_ms_;
  seqno_ = isn_;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seqno_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retrans_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( queue_wait_retran_ != MAX_QUEUE_SIZE_ ) {
    uint64_t temp = queue_wait_retran_;
    queue_wait_retran_ = MAX_QUEUE_SIZE_;
    return std::get<0>( queue_[temp] );
  }
  if ( queue_wait_send_ == queue_back_ )
    return nullopt;
  if ( queue_wait_send_ == queue_front_ )
    timer_ = 0;
  TCPSenderMessage re = std::get<0>( queue_[queue_wait_send_] );
  bytes_sended_ += std::get<0>( queue_[queue_wait_send_] ).sequence_length();

  queue_wait_send_ = ( queue_wait_send_ + 1 ) % MAX_QUEUE_SIZE_;
  return re;
}

void TCPSender::push( Reader& outbound_stream )
{
  while ( available_window_ && !is_fin_
          && ( !is_syn_ || !outbound_stream.is_empty() || outbound_stream.is_finished() ) ) {
    std::get<0>( queue_[queue_back_] ) = TCPSenderMessage { seqno_, false, {}, false };
    if ( !is_syn_ ) {
      is_syn_ = true;
      std::get<0>( queue_[queue_back_] ).SYN = true;
    }

    read( outbound_stream,
          std::min( available_window_ - ( std::get<0>( queue_[queue_back_] ).SYN ? 1 : 0 ),
                    TCPConfig::MAX_PAYLOAD_SIZE ),
          std::get<1>( queue_[queue_back_] ) );
    std::get<0>( queue_[queue_back_] ).payload = std::get<1>( queue_[queue_back_] );
    available_window_ -= std::get<0>( queue_[queue_back_] ).sequence_length();
    if ( available_window_ && outbound_stream.is_finished() ) {
      is_fin_ = true;
      std::get<0>( queue_[queue_back_] ).FIN = true;
      available_window_--;
    }
    seqno_ = seqno_ + std::get<0>( queue_[queue_back_] ).sequence_length();
    seqno_in_flight_ += std::get<0>( queue_[queue_back_] ).sequence_length();

    queue_back_ = ( queue_back_ + 1 ) % MAX_QUEUE_SIZE_;
    if ( queue_back_ == queue_front_ ) {
      std::cout << "TCPSender: queue overflow" << std::endl;
      throw "TCPSender: queue overflow";
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage { seqno_, false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.ackno && ( *msg.ackno ).unwrap( isn_, bytes_sended_ ) > seqno_.unwrap( isn_, bytes_sended_ ) )
    return;

  window_ = msg.ackno ? msg.window_size + ( *msg.ackno ).unwrap( isn_, bytes_sended_ )
                          - seqno_.unwrap( isn_, bytes_sended_ )
                      : msg.window_size;
  available_window_ = msg.window_size ? window_ : 1;

  if ( !msg.ackno )
    return;

  while ( queue_front_ != queue_wait_send_
          && std::get<0>( queue_[queue_front_] ).seqno.unwrap( isn_, bytes_sended_ )
                 + std::get<0>( queue_[queue_front_] ).sequence_length()
               <= ( *msg.ackno ).unwrap( isn_, bytes_sended_ ) ) {

    seqno_in_flight_ -= std::get<0>( queue_[queue_front_] ).sequence_length();
    queue_front_ = ( queue_front_ + 1 ) % MAX_QUEUE_SIZE_;

    RTO_ms_ = initial_RTO_ms_;
    timer_ = 0;
    consecutive_retrans_ = 0;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timer_ += ms_since_last_tick;
  if ( timer_ < RTO_ms_ || queue_front_ == queue_wait_send_ )
    return;

  queue_wait_retran_ = queue_front_;
  if ( window_ ) {
    consecutive_retrans_++;
    RTO_ms_ <<= 1;
  }
  timer_ = 0;
}
