#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  uint64_t last_index = now_index_;
  for ( auto e = buffer_.begin(); e != buffer_.end(); last_index = ( *e ).first + ( *e ).second.length(), e++ ) {
    if ( first_index >= ( *e ).first )
      continue;
    if ( first_index + data.length() <= last_index )
      break;

    uint64_t index = std::max( first_index, last_index );
    uint64_t start = index - first_index;
    uint64_t length = std::min( data.length() - start, ( *e ).first - index );

    std::string str = data.substr( start, length );
    buffer_.insert( e, std::make_pair( index, str ) );
    bytes_pending_ += str.length();
  }
  if ( first_index + data.length() > last_index && first_index < now_index_ + output.available_capacity() ) {
    uint64_t index = std::max( first_index, last_index );
    uint64_t start = index - first_index;
    uint64_t length = std::min( data.length() - start, output.available_capacity() - ( index - now_index_ ) );

    std::string str = data.substr( start, length );
    buffer_.emplace_back( make_pair( index, str ) );
    bytes_pending_ += str.length();
  }

  for ( auto e = buffer_.begin(); e != buffer_.end(); ) {
    if ( ( *e ).first == now_index_ ) {
      output.push( ( *e ).second );
      bytes_pending_ -= ( *e ).second.length();
      now_index_ += ( *e ).second.length();
      e++;
      buffer_.pop_front();
    } else {
      break;
    }
  }

  if ( is_last_substring )
    received_last_ = true;

  if ( received_last_ && buffer_.empty() ) {
    output.close();
    now_index_ = 0;
    bytes_pending_ = 0;
    received_last_ = false;
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}
