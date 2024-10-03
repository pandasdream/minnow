#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point + static_cast<uint32_t>(n)};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // raw_value_ - zero_point + (1 << 32) = ret % (1 << 32) = checkpoint % (1 << 32) + x = b + x
  // raw_value_ = zero_point + b + x   , 
  // x = raw_value_ - zero_point - b
  // ret1 = checkpoint + x , 0 <= x < (1 << 32)
  uint64_t b = (checkpoint << 32) >> 32;
  uint64_t rv = static_cast<uint64_t>(raw_value_);
  uint64_t zp = static_cast<uint64_t>(zero_point.raw_value_);
  uint64_t x;
  while(rv < zp + b) rv += (1L << 32);
  x = rv - zp - b;
  if(x < (1L << 31) || checkpoint + x < (1L << 32)) return checkpoint + x;
  else return checkpoint + x - (1L << 32);
}
