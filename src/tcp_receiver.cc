#include "tcp_receiver.hh"
#include "wrapping_integers.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if(message.RST) {
    rst_ = true;
    reassembler_.reader().set_error();
  }
  if(message.SYN) {
    zero_point_ = message.seqno + 1;
    syn_ = true;
  }
  auto msg_seq = message.SYN ? message.seqno + 1 : message.seqno;
  if(syn_) {
    reassembler_.insert(msg_seq.unwrap(zero_point_, reassembler_.reader().bytes_popped() + reassembler_.reader().bytes_buffered()), message.payload, message.FIN);
    if(message.FIN) fin_ = true;
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage trm;
  bool fin = fin_;
  if(reassembler_.bytes_pending()) fin = false;
  if(syn_) trm.ackno = Wrap32::wrap(static_cast<uint32_t>(reassembler_.reader().bytes_popped() + reassembler_.reader().bytes_buffered()), zero_point_) + fin;
  trm.RST = rst_ || reassembler_.reader().has_error();
  trm.window_size = reassembler_.writer().available_capacity() > UINT16_MAX ? UINT16_MAX : reassembler_.writer().available_capacity();
  return trm;
}
