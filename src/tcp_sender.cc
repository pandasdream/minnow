#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <iostream>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_send_seq_ - next_ack_seq_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retrans_cnt;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  TCPSenderMessage send;
  if(window_size_ == 0) {
    win0_ = true;
    window_size_ = 1;
  }
  ::read(input_.reader(), min(TCPConfig::MAX_PAYLOAD_SIZE, next_ack_seq_ + window_size_ - next_send_seq_), send.payload);
  send.RST = input_.writer().has_error();
  send.SYN = next_send_seq_ == 0;
  send.seqno = Wrap32::wrap(next_send_seq_, isn_);
  if(next_ack_seq_ + window_size_ > next_send_seq_ + send.payload.size() && input_.writer().is_closed() && !fin_ && input_.reader().is_finished()) {
    send.FIN = true;
    fin_ = true;
  }
  next_send_seq_ += send.payload.size() + send.SYN + send.FIN;
  if(send.SYN || send.FIN || send.payload.size() != 0) {
    if(!timer_.isOn())
      timer_ = Timer(ticks_, current_RTO_ms_ == 0 ? initial_RTO_ms_ : current_RTO_ms_);
    outstandings_.push(send);
    transmit(send);
  }
  if(!send.payload.empty()) push(transmit);
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage send;
  send.seqno = Wrap32::wrap(next_send_seq_, isn_);
  send.RST = input_.writer().has_error();
  return send;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if(msg.RST) {
    input_.set_error();
    return;
  }
  optional<Wrap32> ackno = msg.ackno;
  window_size_ = msg.window_size;
  win0_ = window_size_ == 0;
  // not connnected
  if(!ackno.has_value()) return;
  uint64_t abs_ack = ackno.value().unwrap(isn_, next_ack_seq_);
  if(next_send_seq_ < abs_ack || abs_ack <= next_ack_seq_) return;
  current_RTO_ms_ = initial_RTO_ms_;
  next_ack_seq_ = abs_ack;
  while(!outstandings_.empty()) {
    auto& out_seg = outstandings_.front();
    // auto &out_seg heap used after free?
    if(out_seg.seqno == ackno) break;
    if(out_seg.seqno.unwrap(isn_, next_ack_seq_) < abs_ack &&
      abs_ack < out_seg.SYN + out_seg.seqno.unwrap(isn_, next_ack_seq_) + out_seg.payload.size() + out_seg.FIN) {
      break;
    }
    outstandings_.pop();
    // if(out_seg.seqno + out_seg.SYN + out_seg.payload.size() + out_seg.FIN == ackno.value()) break;
  }
  timer_.set_off();
  if(!outstandings_.empty()) timer_ = Timer(ticks_, current_RTO_ms_);
  retrans_cnt = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  ticks_ += ms_since_last_tick;
  // if(false) {
  //   transmit(outstandings_.front());
  // }
  if(!outstandings_.empty() && timer_.alarm(ticks_)) {
    auto resend = outstandings_.front();
    transmit(resend);
    if(!win0_) current_RTO_ms_ = current_RTO_ms_ == 0 ? initial_RTO_ms_ * 2 : current_RTO_ms_ * 2;
    timer_ = Timer(ticks_, current_RTO_ms_);
    if(!win0_) ++retrans_cnt;
  }
}
