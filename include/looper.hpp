
struct looper
{
  //REV: lol wtf derived class can't access private:? Weird...anyways, protected works.
protected:
  loopcond localloop;
  std::string tag;
  int64_t LOOP_SLEEP_MSEC;
  std::mutex startstop_mu;
  
public:
  looper(const std::string& mytag="")
    : tag(mytag), LOOP_SLEEP_MSEC(-1)
  {  }
  
  ~looper()
  {
    stop();
    return;
  }

  void lock_startstop()
  {
    fprintf(stdout, "LOCKING for start/stop\n");
    startstop_mu.lock();
    fprintf(stdout, "LOCKED for start/stop\n");
  }

  void unlock_startstop()
  {
    fprintf(stdout, "UNLOCKING for start/stop\n");
    startstop_mu.unlock();
    fprintf(stdout, "UNLOCKED for start/stop\n");
  }
  
  void set_loop_sleep_msec( const int64_t _msec )
  {
    LOOP_SLEEP_MSEC = _msec;
    return;
  }
  
  const std::string& gettag() const
  {
    return tag;
  }

  void set_tag( const std::string& _newtag )
  {
    tag = _newtag;
  }
  
  bool islooping() const
  {
    return localloop();
  }

  void startlooping()
  {
    localloop.start();
  }
  
  void stop()
  {
    localloop.stop();
  }
};


