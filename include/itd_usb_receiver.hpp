
struct itd_usb_receiver
  : public device_stream_publisher<itd_frameset>
{
protected:
  std::thread mythread;
  std::shared_ptr<ITD_wrapper> itd;

  uint64_t frameidx;
  
public:
  itd_usb_receiver( std::shared_ptr<ITD_wrapper> itdwrap, const size_t mbufsize=0 )
    : device_stream_publisher(mbufsize), itd(itdwrap)
  {
    return;
  }

  itd_usb_receiver( const size_t mbufsize=0 )
    : device_stream_publisher(mbufsize)
  {
    itd = std::make_shared<ITD_wrapper>();
    return;
  }

  std::shared_ptr<ITD_wrapper> get_itd_ptr() const
  {
    return itd;
  }
  
  ~itd_usb_receiver()
  {
    stop();
  }

  void stop()
  {
    fprintf(stdout, "IN STOP: ITD USB RECV\n");
    localloop.stop();
    JOIN( mythread );
    fprintf(stdout, "OUT STOP: ITD USB RECV\n");
  }
  
  void start( loopcond& loop )
  {
    JOIN( mythread ); //In case we are restarting?
    mythread = std::thread( &itd_usb_receiver::doloop, this, std::ref(loop));
  }
  
  void doloop( loopcond& loop )
  {
    //REV: connect to ITD here...
    int whatfps=30;
    bool docolor=true;
    params["fps"] = 30;
    params["iscolor"] = true;
    
    if( !itd->is_open() )
      {
	fprintf(stderr, "ERROR -- REV: in ITD USB RECEIVER -- ITD is not OPEN yet?!?!\n");
	localloop.stop();
	return;
      }
    
    fprintf(stdout, "------- ITD -- CONNECTING! ---------\n");
    bool succconn = itd->connect( whatfps, docolor );
    if( !succconn )
      {
	fprintf(stderr, "\n\nREV: COULD NOT CONNECT TO ITD! (timeout or other error). Setting LOCALLOOP false!\n");
	localloop.stop();
	return;
      }
    
    fprintf(stdout, "------- ITD -- DONE CONNECTED! ---------\n");
    
    frameidx = 0;
    Timer t;
    while( localloop() && loop() )
      {
	const double timeoutsec = 2.0;
	auto itdfs = itd->fill_external( timeoutsec );
	if( itdfs.has_value() )
	  {
	    mbuf.addone( itdfs.value() );
	    if( frameidx % 100 == 0 )
	      {
		fprintf(stdout, " ITD USB receiver: Frame IDX [%ld]  @  Elapsed [%lf] sec (%lf fps)\n", frameidx, t.elapsed(), frameidx/t.elapsed());
	      }
	    ++frameidx;
	  }
	else
	  {
	    fprintf(stdout, "Framegrab from ITD timed out after [%lf] seconds...ending ITD loop\n", timeoutsec);
	    localloop.stop();
	  }
      }
    
    //REV: cleanup here in case of failure?
    itd->disconnect(); //I may have set state?
  }
};
