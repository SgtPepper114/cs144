#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <array>
#include <string>
#include <tuple>

class TCPSender
{
  static const uint64_t MAX_QUEUE_SIZE_ = 2048;
  uint64_t timer_ { 0 };
  uint64_t window_ { 1 };
  uint64_t available_window_ { 1 };
  std::array<std::tuple<TCPSenderMessage, std::string>, MAX_QUEUE_SIZE_> queue_ {};
  uint64_t queue_front_ { 0 };
  uint64_t queue_back_ { 0 };
  uint64_t queue_wait_send_ { 0 };
  uint64_t queue_wait_retran_ { MAX_QUEUE_SIZE_ };
  Wrap32 isn_ { 0 };
  Wrap32 seqno_ { 0 };
  uint64_t initial_RTO_ms_;
  uint64_t RTO_ms_ { 0 };
  bool is_syn_ { false };
  bool is_fin_ { false };
  uint64_t bytes_sended_ { 0 };
  uint64_t seqno_in_flight_ { 0 };
  uint64_t consecutive_retrans_ { 0 };

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
