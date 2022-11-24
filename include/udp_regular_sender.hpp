
#pragma once

struct udp_regular_sender
  : public looper
{
protected:
  std::vector<std::byte> tosend;
  double waittime_sec;
  sockaddr_in6 otherip6;
  int otherport;
  std::shared_ptr<socket_udp6> sock; //REV: this sock will also receive data?
  std::thread mythread;


public:
  udp_regular_sender()
    : looper()
  {}
  
  ~udp_regular_sender()
  {
    stop();
  }
  
  std::shared_ptr<socket_udp6> get_sock() const
  {
    return sock;
  }
  
  //Won't specify "from" port? -- assume all system-allocated automatic ones will be fine.
  //, const std::string& myip6, const int myport )
  udp_regular_sender(const std::vector<std::byte>& msg, const double towait_sec, const sockaddr_in6& ip6, const int port)
    : looper(), tosend(msg), waittime_sec(towait_sec), otherip6(ip6), otherport(port)
  {
    sock = std::make_shared<socket_udp6>();
  }

  udp_regular_sender(const std::string& msg, const double towait_sec, const sockaddr_in6& ip6, const int port)
    : looper(), waittime_sec(towait_sec), otherip6(ip6), otherport(port)
  {
    //REV: I bet this is it! I needs the null on the end!
    tosend = str_to_bytevec( msg );
    sock = std::make_shared<socket_udp6>();
  }

  udp_regular_sender(const std::vector<std::byte>& msg, const double towait_sec, const sockaddr_in6& ip6, const int port, const std::shared_ptr<socket_udp6>& sock2)
    : looper(), tosend(msg), waittime_sec(towait_sec), otherip6(ip6), otherport(port), sock(sock2)
  {
  }

  udp_regular_sender(const std::string& msg, const double towait_sec, const sockaddr_in6& ip6, const int port, const std::shared_ptr<socket_udp6>& sock2)
    : looper(), waittime_sec(towait_sec), otherip6(ip6), otherport(port), sock(sock2)
  {
    tosend = str_to_bytevec(msg);
  }

    
  //REV: lock the *socket* mutex?
  int get_outgoing_port()
  {
    return sock->get_listen_port();
  }
    
  void stop()
  {
    std::fprintf(stdout, "IN STOP: UDP send loop (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: UDP send loop (Tag=[%s])\n", tag.c_str());
  }
  
  void start( loopcond& loop )
  {
    mythread = std::thread( &udp_regular_sender::doloop, this, std::ref(loop) );
  }

  void doloop( loopcond& loop )
  {
    //sock->set_send_timeout( 2.0 );
    std::fprintf(stdout, "IN DOLOOP: UDP send loop (Tag=[%s])\n", tag.c_str());
    sock->sendto6( tosend, otherip6, otherport );
    fprintf(stdout, "UDP SENDER: SENT FIRST DATA (will now loop send to same OTHER)\n");
    while( localloop() && loop() )
      {
	loop.sleepfor( waittime_sec );
	sock->sendto6( tosend, otherip6, otherport ); //need to do this...I keep changing foreign socket ;(
	//sock->send6( tosend );
      }
    std::fprintf(stdout, "OUT DOLOOP: UDP send loop (Tag=[%s])\n", tag.c_str());
  }
};
