

#pragma once


//REV: not a looper -- it takes external loopcond, and just wraps the various streams from tobii2 (data & video)
struct tobii2_streaming_conn
{
public:
  std::shared_ptr<udp_receiver> datarecv;
  std::shared_ptr<udp_receiver> videorecv;
  
  //std::shared_ptr<device_stream_publisher<std::byte>> datarecv;
  //std::shared_ptr<device_stream_publisher<std::byte>> videorecv;
  std::shared_ptr<udp_regular_sender> datakeepalive;
  std::shared_ptr<udp_regular_sender> videokeepalive;
  
  std::string datakamsg;
  std::string videokamsg;
  std::string datakillmsg;
  std::string videokillmsg;
  
  //REV: could query towait from glasses...but fuck that
  //double ka_wait_sec;

    
  tobii2_streaming_conn( const tobii_glasses2_info& info )
  {
    uuids::uuid const id = uuids::uuid_system_generator{}();
    
    std::string idstr = uuids::to_string(id);
    
    datakamsg = "{\"type\": \"live.data.unicast\", \"key\": \"" + idstr + "\", \"op\": \"start\"}";
    videokamsg = "{\"type\": \"live.video.unicast\", \"key\": \"" + idstr + "\", \"op\": \"start\"}";
    
    datakillmsg = "{\"type\": \"live.data.unicast\", \"key\": \"" + idstr + "\", \"op\": \"stop\"}";
    videokillmsg = "{\"type\": \"live.video.unicast\", \"key\": \"" + idstr + "\", \"op\": \"stop\"}";

    std::cout << datakamsg << std::endl;
    std::cout << videokamsg << std::endl;
    
    auto j1 = json::parse(datakamsg);
    auto j2 = json::parse(videokamsg);

    auto j3 = json::parse(datakillmsg);
    auto j4 = json::parse(videokillmsg);
    
    
    //std::fprintf(stdout, "datakamsg: [%s]\n", datakamsg.c_str() );
    
    const double ka_wait_sec = 1.0; //2.000; //should query from REST at /api/conf or /api/status
    int liveport = 49152; //query from REST
    datakeepalive = std::make_shared<udp_regular_sender>( datakamsg, ka_wait_sec, info.live_i6addr, liveport );
    videokeepalive = std::make_shared<udp_regular_sender>( videokamsg, ka_wait_sec, info.live_i6addr, liveport );
    
    int datachunksize=1024;
    int videochunksize=1024*16;
    datarecv = std::make_shared<udp_receiver>( datakeepalive->get_sock(), datachunksize);
    videorecv = std::make_shared<udp_receiver>( videokeepalive->get_sock(), videochunksize);

    if( datarecv->get_sock()->sd != datakeepalive->get_sock()->sd )
      {
	fprintf(stderr, "wtf\n");
	exit(1);
      }
    
    if( videorecv->get_sock()->sd != videokeepalive->get_sock()->sd )
      {
	fprintf(stderr, "wtf\n");
	exit(1);
      }
  }

  void stop()
  {
    std::fprintf(stdout, "IN STOP: Tobii2 Conn\n");
    datakeepalive->stop();
    videokeepalive->stop();

    datarecv->stop();
    videorecv->stop();
    std::fprintf(stdout, "OUT STOP: Tobii2 Conn\n");
  }

  void start( loopcond& loop )
  {
    std::fprintf(stdout, "IN START: Tobii2 Conn\n");
    datakeepalive->start( loop );
    videokeepalive->start( loop );
    
    const int senddelay_msec=2000; //will it matter?: Will the port have buffered stuff?
    std::this_thread::sleep_for(std::chrono::milliseconds(senddelay_msec));
    datarecv->set_tag("TOBII2DATA");
    videorecv->set_tag("TOBII2VIDEO");
    
      
    datarecv->start(loop);
    videorecv->start(loop);
    std::fprintf(stdout, "OUT START: Tobii2 Conn\n");
  }
};
