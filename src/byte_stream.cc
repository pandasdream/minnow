#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return close_;
}

void Writer::push( string data )
{
  uint64_t len = available_capacity();
  if ( len > data.size() ) {
    buf_ += data;
    pushcnt_ += data.size();
  } else {
    buf_ += data.substr( 0, len );
    pushcnt_ += len;
  }
  return;
}

void Writer::close()
{
  close_ = true;
  if ( buf_.empty() ) {
    finished_ = true;
  }
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buf_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return pushcnt_;
}

bool Reader::is_finished() const
{
  return finished_;
}

uint64_t Reader::bytes_popped() const
{
  return popcnt_;
}

string_view Reader::peek() const
{
  return buf_;
}

void Reader::pop( uint64_t len )
{
  if ( len > buf_.size() ) {
    popcnt_ += buf_.size();
    buf_.clear();
  } else {
    popcnt_ += len;
    buf_ = buf_.substr( len );
  }
  if ( close_ && buf_.empty() )
    finished_ = true;
}

uint64_t Reader::bytes_buffered() const
{
  return buf_.size();
}
