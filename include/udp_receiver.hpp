
#pragma once

struct udp_receiver
  : public device_stream_publisher<std::byte>
{
protected:
  std::thread mythread;
  std::shared_ptr<socket_udp6> sock; //REV: this sock will also receive data...
  sockaddr_in6 otherip6;
  int otherport;
  size_t chunksize;
  
public:
  udp_receiver( const size_t mbufsize=0)
    : device_stream_publisher( mbufsize )
  {}
  
  udp_receiver( const std::shared_ptr<socket_udp6>& sock2, const size_t _chunksize,  const size_t mbufsize=0 )
    : device_stream_publisher( mbufsize ), sock(sock2), chunksize(_chunksize)
  {}

  ~udp_receiver()
  {
    stop();
  }

  //REV: error bc returns protected?
  std::shared_ptr<socket_udp6> get_sock() const
  {
    return sock;
  }
  
    
  void stop()
  {
    std::fprintf(stdout, "IN STOP: UDP receiver loop thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: UDP receiver loop thread (Tag=[%s])\n", tag.c_str());
  }

  void doloop( loopcond& loop )
  {
    std::fprintf(stdout, "IN DOLOOP: UDP receiver loop thread (Tag=[%s])\n", tag.c_str());
    std::vector<std::byte> vec(chunksize);

    socklen_t otherlen;
    otherip6 = sock->get_other_addr( otherlen );

    //REV: data is not guaranteed to be sent from their side keepalive port :49152, only to arrive at our port that we sent from.
    //otherport = sock->get_other_port();
    otherport = -1;
    
    //sock->set_recv_timeout( 2.0 );
    
    fprintf(stdout, "UDP RECV: WAITING FOR FIRST DATA\n"); //REV: note, it should ont be OTHERPORT, it should be this?
    sock->recvupto_from_into( chunksize, otherip6, otherport, vec );
    
    if( !vec.empty() )
      {
	mbuf.movefrom( vec ); //Will add call notifyall by default
      }
    
    fprintf(stdout, "UDP RECV: Got first data, will now communicate with same OTHER\n");
    while( localloop() && loop() )
      {
	
	//REV: need to respecify -1 foreign port each time... (note this will reset the port I am *sending* to? Fuck?)
	//sock->recvupto_from_into( chunksize, otherip6, otherport, vec );
	sock->recvupto_into( chunksize, vec ); //This will write over the OTHER guys? It will overwrite OTHER IP with same one
	// But port it sent it may not be the KEEPALIVE port? So...what to do? Either keep sepa
	
	if( !vec.empty() )
	  {
	    mbuf.movefrom( vec );
	    //mbuf.cv.notify_all(); //No need to wait() conditionally because I will just read no matter what haha.
	  }
      }
    std::fprintf(stdout, "OUT DOLOOP: UDP recv loop (Tag=[%s])\n", tag.c_str());
  }
  
  void start( loopcond& loop )
  {
    mythread = std::thread( &udp_receiver::doloop, this, std::ref(loop) );
    //mythread.detach(); //No need to join. Resources released when execution stops.
  }
  
};
