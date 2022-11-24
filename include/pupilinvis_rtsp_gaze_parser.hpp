
#pragma once

//REV: is the time base the same...?
struct pupilinvis_rtsp_gaze_parser
  : public device_stream_consumer_parser<tagged_avpkt_ptr>
{
private:
  //AVFormatContext* fmt_context_ptr; //don't really need.
  std::shared_ptr<rtsp_receiver> receiver_ptr;
  std::shared_ptr< timed_buffer<vec2f, std::uint64_t> > gaze_frames;
  std::thread decode_thread;
  double pts_timebase_hz_sec=-1;
  
public:
  pupilinvis_rtsp_gaze_parser( std::shared_ptr<rtsp_receiver> _receiver_ptr, const size_t maxtbuf=0 )
    : device_stream_consumer_parser(), receiver_ptr(_receiver_ptr)
  {
    timebase_hz_sec = -1;
    gaze_frames = std::make_shared<timed_buffer<vec2f, std::uint64_t>>(maxtbuf);
    outputs["gaze"] = gaze_frames;
    
    if( !receiver_ptr )
      {
	fprintf(stderr, "ERROR: FMT CONTEXT IS NULL (REV: TODO: try to figure out format from stream with no format info)\n");
	exit(1);
      }
  }

  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<tagged_avpkt_ptr>> dsp )
  {
    lock_startstop(); //This may not work anyways, need to call this guy's thing here safely, will ensure that no one tries to
    startlooping();
    decode_thread = std::thread( &pupilinvis_rtsp_gaze_parser::decodeloop, this, std::ref(loop), dsp );
    unlock_startstop();
  }

  ~pupilinvis_rtsp_gaze_parser()
  {
    stop();
  }
  
  //OVERLOADING
  double get_timebase_hz_sec()
  {
    //REV: or I should get it from the video side?
    //fprintf(stdout, "Attempting to get time base of gaze\n");
    if( pts_timebase_hz_sec < 0 )
      {
	//LOCK HERE
	//REV: I need to wait for it to be initialized....
	const std::lock_guard<std::mutex> lock(receiver_ptr->mu);
	auto format_context = receiver_ptr->get_stream_format_info();
	if( !format_context )
	  {
	    //fprintf(stderr, "REV: GAZE -- not yet detected timebase hz sec (b/c no format context allocated...)\n");
	    return pts_timebase_hz_sec;
	  }
	auto tb = format_context->streams[0]->time_base; //REV: ghetto as fuck? I know it is always stream zero? What is "best" stream?
	fprintf(stdout, "Time base of DATA (gaze): [%d]/[%d]\n", tb.num, tb.den);
	if( tb.num != 1 )
	  {
	    fprintf(stdout, "I don't know how to handle rational number timebases...\n");
	    exit(1);
	  }
	pts_timebase_hz_sec = tb.den;
	
	//pts_timebase_hz_sec = 90000; //1000000; //microseconds (unix time).
      }
    //fprintf(stdout, "DONE Attempting to get time base of gaze\n");
    return pts_timebase_hz_sec;
  }

  void stop()
  {
    lock_startstop(); //This may not work anyways, need to call this guy's thing here safely, will ensure that no one tries to
    std::fprintf(stdout, "IN STOP: PI gaze Parser\n");
    localloop.stop();
    if( decode_thread.joinable() ) //Will this check whether it is running or not?
      {
	decode_thread.join();
      }
    std::fprintf(stdout, "OUT STOP: PI gaze Parser\n");
    unlock_startstop();
  }


  void decodeloop( loopcond& loop, std::shared_ptr<device_stream_publisher<tagged_avpkt_ptr>> dsp )
  {
    std::string gazefname = "pi_gaze.32FC2";
    std::deque<std::shared_ptr<saver_thread_info<vec2f,std::uint64_t>>> gazesavet;
    bool shouldsave = false;
    std::uint64_t mysaveidx=0;

    while( localloop() && loop() )
      {
	wait_or_timeout_cond( [&]() -> bool { return (!localloop() || dsp->mbuf.size() > 0); },
			      loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );
	
	std::vector<tagged_avpkt_ptr> vec;
	auto npopped = dsp->mbuf.popallto(vec);
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  
	  if( shouldsave && ((mysaveidx != saving_idx) || !saving_raw ))
	    {
	      shouldsave = false;

	      fprintf(stdout, "####### PI(GAZE) -- TURNING *OFF* SAVING!\n");
	      
	      for( auto& th : gazesavet ) { th->saving.stop(); }
	    }


	  if( (saving_raw) && (mysaveidx != saving_idx) )
	    {
	      if( saving_idx != mysaveidx+1 )
		{
		  fprintf(stdout, "You skipped a saving index?\n");
		  exit(1);
		}

	      fprintf(stdout, "####### PI(GAZE) -- TURNING ON SAVING!\n");
	      mysaveidx = saving_idx;
	      shouldsave = true;
	      
	      gazesavet.push_back( std::make_shared<saver_thread_info<vec2f,std::uint64_t>>());
	      gazesavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vec2f<vec2f,std::uint64_t>, this,
						 std::ref(loop), std::ref(gazesavet.back()->saving), std::ref(gazesavet.back()->tb),
						 gazefname, gazefname+".ts", std::ref(gazesavet.back()->working) );
	      
	    }
	} //end LOCKGUARD
	
	{
	  ERASEFINISHED( gaze );
	}
	
	if( npopped > 0 )
	  {
	    //fprintf(stdout, "GAZE -- Popped and will process [%ld] data points from RTSP!\n", npopped);
	    for( auto& tpkt : vec )
	      {
		auto packet = tpkt.pkt;
		auto ts = tpkt.ts;
		
		uint64_t pts = packet->pts;
		//auto tb = packet->time_base; //AVRational
		
		vec2f gaze(-1e20, -1e20); //Pupil Invis space is pixel from top left (as 0, 0)
		
		//fprintf(stdout, "REV: GAZE :size: [%d]  pts: [%ld]  dts: [%ld]\n", packet->size, packet->pts, packet->dts );
		if( packet->size == 9 )
		  {
		    //float x = *((float*)&packet->data[0]);
		    uint32_t ux = ntohl(*(uint32_t*)&packet->data[0]);
		    if( sizeof(uint32_t) != sizeof(float) )
		      {
			fprintf(stderr, "REV: ERROR sizeof float != 32 bits on this machine?! (pupilinvis gaze parser) [%lu]\n", sizeof(float));
			exit(1);
		      }
		    float x=0; // = *((float*)&ux);
		    uint32_t uy = ntohl(*(uint32_t*)&packet->data[4]);
		    float y=0; // = *((float*)&uy);
		    //float y = *((float*)&packet->data[4]);
		    char worn = *((char*)&packet->data[9]);
		    std::memcpy(&x, &ux, sizeof(ux));
		    std::memcpy(&y, &uy, sizeof(uy));
		    		    
		    //REV: fuck, check if PTS of decoded frame is same as pkt? For video...
		    //REV: 2 -- get raw estimate of time from unix from RTP packets if I can... (realworld time all fucked).
		    //fprintf(stdout, "PTS [%ld]    X: [%f] Y: [%f]  worn? [%d]\n", pts, x, y, (int)worn );
		    gaze.x = x;
		    gaze.y = y;
		  }
		else if( packet->size == 17 )
		  {
		    //float x = *((float*)&packet->data[0]);
		    uint32_t ux1 = ntohl(*(uint32_t*)&packet->data[0]);
		    float x1=0;// = *((float*)&ux1);
		    uint32_t uy1 = ntohl(*(uint32_t*)&packet->data[4]);
		    float y1=0;// = *((float*)&uy1);
		    //float y = *((float*)&packet->data[4]);
		    char worn = *((char*)&packet->data[9]);
		    
		    uint32_t ux2 = ntohl(*(uint32_t*)&packet->data[10]);
		    float x2=0;// = *((float*)&ux2);
		    uint32_t uy2 = ntohl(*(uint32_t*)&packet->data[14]);
		    float y2=0;// = *((float*)&uy2);
		    fprintf(stdout, "REV: got binocular datum (unexpected!)!? X1: [%f] Y1: [%f]     X2: [%f] Y2: [%f]    worn? [%d]\n", x1, y1, x2, y2, (int)worn );

		    std::memcpy(&x1, &ux1, sizeof(ux1));
		    std::memcpy(&x2, &ux2, sizeof(ux2));
		    std::memcpy(&y1, &uy1, sizeof(uy1));
		    std::memcpy(&y2, &uy2, sizeof(uy2));
		    
		    exit(1);
		  }
		else
		  {
		    fprintf(stderr, "REV: Unknown packet size? Should be 9 or 17 for pupil invis...\n");
		    exit(1);
		  }
	    
		//get gaze out.
		av_packet_free(&packet); //I'm done with it now.
		
		if( isactive("gaze") )
		  {
		    //gaze_frames->add( gaze, pts );
		    gaze_frames->add( gaze, ts, sizeof(gaze) );
		  }
		if( shouldsave )
		  {
		    gazesavet.back()->tb.add( gaze, ts, sizeof(gaze) );
		  }
	      } //end for packet in vec
	  } //end if nppped > 0
	
	if( LOOP_SLEEP_MSEC > 0 )
	  {
	    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MSEC));
	  }
      } //end while loop() and localloop()

    std::fprintf(stdout, "Joining saving threads (PUPILINVIS GAZE)\n");
    for( auto& th : gazesavet ) { JOIN( th->t ); }
    std::fprintf(stdout, "DONE Joining saving threads (PUPILINVIS GAZE)\n");
  } // end decodeloop
  
}; // end struct pupilinvis_rtsp_gaze_parser
