
struct tobii2_video_parser
  : public device_stream_consumer_parser<std::byte>
{
  //REV: could be RECEIVED TIME or some shit too? :( For "simulating" replay? Too much work ugh.
  //std::shared_ptr<timed_buffer<std::string,std::uint64_t>> data;
  std::shared_ptr< timed_buffer<cv::Mat,std::uint64_t> > scene_frames;
  std::shared_ptr< timed_buffer<std::vector<std::byte>,std::uint64_t> > audio_frames;
  
  std::thread decode_thread;
  
  mpegts_parser mpegparser;
  
  
  tobii2_video_parser( const size_t maxtbuf=0 )
    : device_stream_consumer_parser()
  {
    timebase_hz_sec = -1;
    scene_frames = std::make_shared<timed_buffer<cv::Mat,std::uint64_t>>(maxtbuf);
    audio_frames = std::make_shared<timed_buffer<std::vector<std::byte>,std::uint64_t> >(maxtbuf);
    outputs["scene"] = scene_frames;
    outputs["mic"] = audio_frames;
  }
    
    
  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<std::byte>> dsp )
  {
    decode_thread = std::thread( &tobii2_video_parser::decodeloop, this, std::ref(loop), dsp );
  }
  
  ~tobii2_video_parser()
  {
    stop();
  }

  //OVERLOADING
  double get_timebase_hz_sec()
  {
    return mpegparser.get_timebase_hz_sec();
  }
  
  
  void stop()
  {
    std::fprintf(stdout, "IN STOP: Tobii2 MPEGTS Parser\n");
    localloop.stop();
    if( decode_thread.joinable() ) //Will this check whether it is running or not?
      {
	decode_thread.join();
      }
    std::fprintf(stdout, "OUT STOP: Tobii2 MPEGTS Parser\n");
  }


  //REV: todo -- save decoded videos as well. Note...they're already encoded in h264...just use ffmpeg to write that?
  //Otherwise I'm converting an re-converting LOL?

  
  void decodeloop( loopcond& loop, std::shared_ptr<device_stream_publisher<std::byte>> dsp )
  {
    std::fprintf(stdout, "IN DECODELOOP: Tobii2 MPEGTS Parser\n");
    size_t internal_buf_size=1024*1024; //buffer held by ffmpeg
    size_t minwaitbytes = 1024*128; //wait for 20 mb of data?
    size_t startbufbytes = 1024*128;
    std::ofstream rawfile;
    std::string rawfname="tobii2_mpegts.raw";
    std::thread writethread;
    cv::Mat mat;
    //cv::Mat toshow;

    bool shouldsave=false;
    std::uint64_t mysaveidx=0;

    const bool dropmem = true;
    
    
    
    fprintf(stdout, "Init Decoding -- waiting for first data\n");
    
    wait_or_timeout_cond( [&]() -> bool { return ( !dsp->islooping() || !localloop() || dsp->mbuf.size() > startbufbytes ); },
			  loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );
    
    
    fprintf(stdout, "GOT FIRST DATA! TOBII2 MPEGTS parser\n");
    mpegparser.init_decoding(dsp->mbuf, internal_buf_size); //starts to slurp and get context/codec info about streams etc.
    fprintf(stdout, "FINISHED init decode\n");
    while( localloop() && loop() )
      {
	
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  
	  if( (mysaveidx != saving_idx) || (!saving_raw) )
	    {
	      shouldsave = false;

	      if( rawfile.is_open() )
		{
		  //Something is wrong? I should close it. Probably they quickly toggled?
		  std::fprintf(stdout, "CLOSING raw tobii video\n");
		  dsp->mbuf.backup_off();
		  std::vector<std::byte> tvec;
		  dsp->mbuf.popallbackupto( tvec );
		  size_t bytestowrite = tvec.size()/sizeof(tvec[0]);
		  rawfile.write( reinterpret_cast<char*>(tvec.data()), tvec.size()/sizeof(tvec[0]) );
		  rawfile.close();
		}
	    }
	  
	  if( saving_raw && mysaveidx != saving_idx )
	    {
	      shouldsave = true;
	      mysaveidx = saving_idx;

	      if( rawfile.is_open() )
		{
		  fprintf(stderr, "fucking tobii parser -- rawfile already open?\n");
		  exit(1);
		}
	      dsp->mbuf.backup_on();
	      std::string rawpath = saving_raw_dir+"/"+rawfname;
	      std::fprintf(stdout, "Tobii2 Video Parser: Opening RAW output file [%s]\n", rawpath.c_str() );
	      rawfile.open( rawpath, std::ofstream::binary );
	    }
	}
	//UNLOCK
	
	wait_or_timeout_cond( [&]() -> bool { return (  !dsp->islooping() || !localloop() || dsp->mbuf.size() > minwaitbytes); },
			      loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );
	
	
	
	if( shouldsave )
	  {
	    if( !rawfile.is_open() )
	      {
		fprintf(stderr, "You dirty bitch, tobii raw video not open at write time\n");
		exit(1);
	      }
	    ////////////////////
	    //REV ------- WHAT THE FUCK!!! Doing file output in a write thread was causing
	    ///           out of order writes to the file?!?!?!?!?! Maybe the next "loop" as starting before I could join?
	    ///   -----> lambda [&] capture was capturing local variables (vec) declared AFTER the line, and overwriting?!?!? whoa.
	    ////////////////////
	    writethread = std::thread( [&dsp, &rawfile]()
	    {
	      std::vector<std::byte> tvec;
	      dsp->mbuf.popallbackupto( tvec );
	      size_t bytestowrite = tvec.size()/sizeof(tvec[0]);
	      rawfile.write( reinterpret_cast<char*>(tvec.data()), tvec.size()/sizeof(tvec[0]) );
	    } );
	  }
	
	//mpegparser is initialized with dsp->mbuf. I don't directly consume it here -- in fact I can't predict how much will
	// be consumed by the readFunc callback. I really should add another layer -- a local dsp->mbuf I pop to.
	mpegparser.decode_next_frames();
	

	//Pop audios for now...not using it.
	uint64_t audpts;
	while( true == mpegparser.process_pop_front_aud( audpts ) )
	  {
	    //Do nothing
	  }
	
	    
	//REV: at this point, mat is the first frame, but I have set FPS.
	uint64_t pts;
	while( true == mpegparser.process_pop_front_vid( mat, pts ) )
	  {
	    //REV: only push to timed buffer if I am streaming...
	    auto matsize = cvmatsize( mat );
	    if( isactive("scene") )
	      {
		scene_frames->add( mat, pts, matsize, dropmem );
	      }
	  }

	if( LOOP_SLEEP_MSEC > 0 )
	  {
	    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MSEC));
	  }
	
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	
	if( writethread.joinable() ){ writethread.join(); }
      } //end while SHOULDLOOP

    if( rawfile.is_open() )
      {
	dsp->mbuf.backup_off();
	std::fprintf(stdout, "(AT END) Tobii2 Video Parser: Closing raw output file\n");
	std::vector<std::byte> tvec;
	dsp->mbuf.popallbackupto( tvec );
	size_t bytestowrite = tvec.size()/sizeof(tvec[0]);
	rawfile.write( reinterpret_cast<char*>(tvec.data()), tvec.size()/sizeof(tvec[0]) );
	std::fprintf(stdout, "Writing out [%ld] dreg bytes from backup after raw output closed\n", tvec.size());
	rawfile.close();
      }

    
    std::fprintf(stdout, "OUT DECODELOOP: Tobii2 MPEGTS Parser\n");
  }
};

