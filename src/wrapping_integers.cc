#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32( n + zero_point.raw_value_ );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t u64 = raw_value_ - zero_point.raw_value_ + ( checkpoint & ( ~( ( 1UL << 32 ) - 1 ) ) );
  if ( u64 <= checkpoint )
    return u64 + ( 1UL << 32 ) - checkpoint < checkpoint - u64 ? u64 + ( 1UL << 32 ) : u64;
  else if ( u64 >= 1UL << 32 )
    return checkpoint - ( u64 - ( 1UL << 32 ) ) < u64 - checkpoint ? u64 - ( 1UL << 32 ) : u64;
  else
    return u64;
}
