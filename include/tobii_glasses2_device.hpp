

struct tobii_glasses2_device
  : public device
{
private:
  
public:
  void init();
  
  void start_saving_raw( const std::string& path );
  void stop_saving_raw();
  
  void start_streaming( const uint64_t max_buffer_time );
  void stop_streaming();
}
