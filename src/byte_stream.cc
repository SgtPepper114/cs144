#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( f_close_ || f_error_ )
    return;
  for ( auto c : data ) {
    if ( buffer_.size() == capacity_ )
      break;
    if ( buffer_.empty() ) {
      first_char_.clear();
      first_char_ += c;
    }
    buffer_.push( c );
    total_pushed_++;
  }
}

void Writer::close()
{
  f_close_ = true;
}

void Writer::set_error()
{
  f_error_ = true;
}

bool Writer::is_closed() const
{
  return f_close_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

string_view Reader::peek() const
{
  return string_view( first_char_ );
}

bool Reader::is_finished() const
{
  return f_close_ && buffer_.empty();
}

bool Reader::has_error() const
{
  return f_error_;
}

void Reader::pop( uint64_t len )
{
  if ( buffer_.size() < len ) {
    f_error_ = true;
    return;
  }
  while ( len-- ) {
    buffer_.pop();
    total_poped_++;
  }
  first_char_.clear();
  if ( !buffer_.empty() )
    first_char_ += buffer_.front();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return total_poped_;
}
