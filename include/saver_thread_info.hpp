#pragma once


//REV: simple struct to hold saver thread for raw saving
template <typename ST, typename STT>
struct saver_thread_info
{
  
  saver_thread_info()
  {
  }
  
  ~saver_thread_info()
  {
  }
  
  std::thread t;
  loopcond saving;
  loopcond working;
  timed_buffer<ST,STT> tb;
};
