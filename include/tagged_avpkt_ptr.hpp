#pragma once

struct tagged_avpkt_ptr
{
  AVPacket* pkt;
  uint64_t ts;

  tagged_avpkt_ptr( AVPacket* p, const uint64_t t )
    : pkt(p), ts(t)
  {
  }
};
