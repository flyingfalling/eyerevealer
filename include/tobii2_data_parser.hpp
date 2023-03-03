#pragma once


struct tobii2_data_parser
  : public device_stream_consumer_parser<std::byte>
{
  //REV: could be RECEIVED TIME or some shit too? :( For "simulating" replay? Too much work ugh.
  std::shared_ptr<timed_buffer<json,tobii2time_t>> all_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> pc_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> pd_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> gd_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> gp_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> gp3_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> gy_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> ac_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> pts_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> epts_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> vts_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> evts_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> sig_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> ets_data;
  std::shared_ptr<timed_buffer<json,tobii2time_t>> marker2d_data;
  std::thread mythread;

  //REV: allow them to set per-output...?
  //const double ts_timebase_hz_sec = 1e6; //microseconds...cf tobii 2 developers guide. Monotonic.

#define MAKE_AND_OUTPUT( s ) s ## _data = std::make_shared<timed_buffer<json,tobii2time_t>>( maxtbuf ); outputs[ #s ] = s ## _data;


  tobii2_data_parser( const size_t maxtbuf=0 )
    : device_stream_consumer_parser()
  {
    timebase_hz_sec = 1e6; //TOBII microsecs
    
    MAKE_AND_OUTPUT( all );
    MAKE_AND_OUTPUT( pc );
    MAKE_AND_OUTPUT( pd );
    MAKE_AND_OUTPUT( gd );
    MAKE_AND_OUTPUT( gp );
    MAKE_AND_OUTPUT( gp3 );
    MAKE_AND_OUTPUT( gy );
    MAKE_AND_OUTPUT( ac );
    MAKE_AND_OUTPUT( pts );
    MAKE_AND_OUTPUT( epts );
    MAKE_AND_OUTPUT( vts );
    MAKE_AND_OUTPUT( evts );
    MAKE_AND_OUTPUT( sig );
    MAKE_AND_OUTPUT( ets );
    MAKE_AND_OUTPUT( marker2d );
  }
  
  ~tobii2_data_parser()
  {
    stop();
  }

  void stop()
  {
    localloop.stop();
    if( mythread.joinable() )
      {
	mythread.join();
      }
  }
  
  //void start( loopcond& loop, mutexed_buffer<std::byte>& mbuf )
  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<std::byte>> dsp )
  {
    mythread = std::thread( &tobii2_data_parser::doloop, this, std::ref(loop), dsp );
  }

  
  //void doloop( loopcond& loop, device_stream_publisher<std::byte>& dps )
  void doloop( loopcond& loop, std::shared_ptr<device_stream_publisher<std::byte>> dsp )
  {
    std::fprintf(stdout, "IN DOLOOP: Tobii2 Data Parser\n");
    std::ofstream rawfile;
    std::thread writethread;
    std::string rawfname = "tobii2_json.raw";
    std::uint64_t mysaveidx=0;
    bool shouldsave=false;
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
		  std::fprintf(stdout, "CLOSING raw tobii DATA)\n");
		  rawfile.close();
		}
	    }
	  
	  if( saving_raw && mysaveidx != saving_idx )
	    {
	      shouldsave=true;
	      mysaveidx = saving_idx;
	      
	      if(rawfile.is_open())
		{
		  fprintf(stderr, "Err already open tobii data parser raw\n");
		  exit(1);
		}
	      	      
	      //Some unique name for each buffer consumer... its type and ? Note I should get e.g. serial number etc.
	      std::string rawpath = saving_raw_dir+"/"+rawfname;
	      std::fprintf(stdout, "DATA PARSER, opening raw output file path [%s]\n", rawpath.c_str() );
	      rawfile.open( rawpath, std::ofstream::binary );
	    }
	}
	//UNLOCK
	
	wait_or_timeout_cond( [&]() -> bool { return (!localloop() || dsp->mbuf.size() > 0); },
			      loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );
	
	std::vector<std::byte> vec;
	dsp->mbuf.popallto(vec);

	if( !vec.empty() )
	  {
	    if( shouldsave )
	      {
		if( !rawfile.is_open() )
		  {
		    fprintf(stderr, "wtf rawfile not open\n");
		    exit(1);
		  }
		
		//auto tvec = vec; //deep copy..? Or it might get fucked in parallel.
		writethread = std::thread( [vec,&rawfile]()
		{
		  //REV: fuck it just copy the vector?...?
		  size_t bytestowrite = vec.size()/sizeof(vec[0]);
		  //fprintf(stdout, "Writing %ld bytes to file!\n", bytestowrite);
		  rawfile.write( (char*)const_cast<std::byte*>(vec.data()), bytestowrite );
		}
		  );
		//rawfile.write( reinterpret_cast<char*>(vec.data()), bytestowrite );
	      }
	    	
	    //std::string svec(vec.begin(), vec.end());
	    auto svec = bytevec_to_str( vec );
	    std::istringstream iss(svec);
	    std::string line = pop_line_from_ss( iss );
	    while( !line.empty() )
	      {
		auto j = json::parse( line );

#define ISACTIVE_CONTAINS( s )   if( isactive( #s ) && j.contains( #s ) ) { s ## _data->add( j, ts, j.size() );  }
#define ISACTIVE_CONTAINS_SYNC( s )   if( isactive( #s ) && j.contains( #s ) ) { s ## _data->add( j, j[#s], j.size() ); }
		try
		  {
		    if( j.contains("ts") && j.contains("s") )
		      {
			std::uint64_t ts = j["ts"];
			int s = j["s"];
			if( 0 == s ) //Only push "good" status messages...
			  {
			    if( isactive( "all" ) ) //REV: not mistake! doesn't need to "contain"
			      {
				all_data->add(j,ts,j.size());
			      }
			    ISACTIVE_CONTAINS( pc ); //pupil center
			    ISACTIVE_CONTAINS( pd ); //pupil diameter
			    ISACTIVE_CONTAINS( gd ); //gaze direction
			    ISACTIVE_CONTAINS( gp ); //2d gaze pos
			    ISACTIVE_CONTAINS( gp3 ); //3d gaze pos
			    ISACTIVE_CONTAINS( gy ); //gyro
			    ISACTIVE_CONTAINS( ac ); //accelerometer
			    ISACTIVE_CONTAINS_SYNC( pts ); //pts sync (vid pts)
			    ISACTIVE_CONTAINS_SYNC( epts ); //eye cam pts
			    ISACTIVE_CONTAINS_SYNC( vts ); //vts sync
			    ISACTIVE_CONTAINS_SYNC( evts ); //eye vts sync
			    ISACTIVE_CONTAINS( sig ); //signal port
			    ISACTIVE_CONTAINS( ets ); //api event
			    ISACTIVE_CONTAINS( marker2d ); //marker2d for visualize calibration marker detection
			  }
		      }
		    else
		      {
			std::fprintf(stderr, "REV: error -- no ts? [%s]\n", line.c_str() );
			loop.stop();
		      }
		  }
		catch (json::exception& e)
		  {
		    // output exception information
		    std::cout << "message: " << e.what() << '\n'
			      << "exception id: " << e.id << std::endl;
		    std::cout << line << std::endl;
		    loop.stop();
		  }


		line = pop_line_from_ss( iss );
	      } //end while !line.empty
	    
	    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
	    
	    if( writethread.joinable() ) { writethread.join(); }
	    
	  } //if !vec.empty
      }
    
    if( rawfile.is_open() )
      {
	std::fprintf(stdout, "(AT END) DATA PARSER, closing raw file\n" );
	rawfile.close();
      }
    
    std::fprintf(stdout, "OUT DOLOOP: Tobii2 Data Parser\n");
  }

  //I need to convert pts_from_ts_and_sync...but to find closest ts sync I have to search through all PTS packets (which are
  //   organized by PTS timestamp, not ts timestamp...as GP etc are?!)

  //Should I invert the payload (or give inverted value finding).
  //I guess give it a timed thing...but there is no way to get things "by index" is there? :(
  // Perhaps timed_blah should have a as_array() type thing, which gives to me as array so I can search?
  // Since I don't know the ordering of the contents (although they are theoretically also sorted from lowest to highest...)
  // But it only cares about sorting of time-types, not the payload.

  //REV: make a method to invert it or some shit?


  //REV: so it specifically doesn't work because of two of same type?
  std::shared_ptr<timed_buffer<tobii2time_t,tobii2time_t>> invert_for_ts_to_pts_buffer()
  {
    auto mycopy = pts_data->freeze_copy();
    
    //std::shared_ptr<timed_buffer<tobii2time_t,tobii2time_t>> inverted_ts_pts =
    auto inverted_ts_pts =
      std::make_shared<timed_buffer<tobii2time_t,tobii2time_t>>();
    
    while( mycopy.size() > 0 )
      {
	auto pts = mycopy.front().timestamp;
        auto ts = mycopy.front().payload["ts"];
	mycopy.pop_front();
	
	//elem, timestamp
	//REV: SPECIFICALLY this fucking place with 3 int64_t, int64_t etc. is the issue?
	//And, specifically it is only with emplace() (not emplace back), some ambiguity with multiple uint64_t forwarding?
	auto succ = inverted_ts_pts->add( pts, ts, sizeof(pts) );
      }
    
    return inverted_ts_pts;
  }
  
  //REV: helper function, convert pts->ts
  uint64_t ts_from_pts_and_sync( const uint64_t ptstarg, const uint64_t ptssync, const uint64_t tssync, const double pts_tb_hz_sec )
  {
    uint64_t tstarg;
    if( ptssync < ptstarg )
      {
	//REV: lol hope this does not overflow.
	uint64_t offset = (timebase_hz_sec * (ptstarg - ptssync) / pts_tb_hz_sec );
	tstarg = tssync + offset;
      }
    else
      {
	uint64_t offset = (timebase_hz_sec * (ptssync - ptstarg) / pts_tb_hz_sec );
	assert( offset <= tssync );
	tstarg = tssync - offset;
      }
    return tstarg;
  }

  uint64_t pts_from_ts_and_sync( const uint64_t tstarg, const uint64_t tssync, const uint64_t ptssync, const double pts_tb_hz_sec )
  {
    uint64_t ptstarg;
    if( tssync < tstarg )
      {
	//REV: lol hope this does not overflow.
	//pts is equal to // (tstarg - tssync) * (pts_hz_sec / ts_hz_sec)
	uint64_t offset = (pts_tb_hz_sec * (tstarg - tssync) / timebase_hz_sec );
	ptstarg = ptssync + offset;
      }
    else //tssync >= tstarg...i.e. its the first one?
      {
	uint64_t offset = (pts_tb_hz_sec * (tssync - tstarg) / timebase_hz_sec );
	assert( offset <= ptssync );
	ptstarg = ptssync - offset;
      }
    return ptstarg;
  }

  std::optional<uint64_t> ts_from_pts( const uint64_t ptstarg, const double pts_tb_hz_sec, const std::string& ptstype, const bool dropbefore=false )
  {
    std::any op = get_output( ptstype );
    if( op.has_value() )
      {
	auto ptr = std::any_cast< std::shared_ptr<timed_buffer<json,uint64_t>> >( op );
	
	//DROP SYNC PACKETS BY DEFAULT (note -- it will keep the one that was found);
	auto sp = ptr->get_timed_element( ptstarg, relative_timing::EITHER, dropbefore );
	if( sp.has_value() )
	  {
	    auto v = sp.value();
	    auto ptssync = v.timestamp;
	    assert( ptssync == v.payload[ptstype] );
	    auto tssync = v.payload["ts"];
	    
	    auto result = ts_from_pts_and_sync( ptstarg, ptssync, tssync, pts_tb_hz_sec );
	    return result;
	  }
      }
    return std::nullopt;
  }

  
  
  std::optional<uint64_t> pts_from_ts( const uint64_t tstarg, const double pts_tb_hz_sec, std::shared_ptr<timed_buffer<tobii2time_t,tobii2time_t>> ts_pts_ptr )
  {
    bool dropbefore=false;
    
    auto sp = ts_pts_ptr->get_timed_element( tstarg, relative_timing::EITHER, dropbefore );
    if( sp.has_value() )
      {
	auto v = sp.value();
	auto tssync = v.timestamp;
	auto ptssync = v.payload;

	fprintf(stdout, "Go sync from TS: [%ld]  PTS: [%ld]\n", tssync, ptssync );
	
	auto result = pts_from_ts_and_sync( tstarg, tssync, ptssync, pts_tb_hz_sec );
	return result;
      }
    
    return std::nullopt;
  }
  
  
}; //end tobii data parser
