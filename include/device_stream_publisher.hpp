#pragma once

//REV: should this offer methods to "get" parameters as well?
// E.g. for tobii, if it can know how to query the actual glasses to get desired parameters?
// REV: that is fine but I should not change it at all (interrupt stream etc.) because the parser
// will mess stuff up.
template <typename T>
struct device_stream_publisher
  : public looper
{
public:
  mutexed_buffer<T> mbuf;
  json params;

  device_stream_publisher( const size_t maxmbuf=0 )
    : looper()
  {
    mbuf.set_maxsize(maxmbuf);
  }
  
  virtual void start( loopcond& loop ) = 0;
  virtual void stop() = 0; //does this erase base class stop?

  bool islooping() const
  {
    return localloop();
  }
};
