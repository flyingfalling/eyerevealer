
#pragma once

struct itd_parser
 : public device_stream_consumer_parser<itd_frameset>
{
private:
  
  std::shared_ptr< timed_buffer<cv::Mat,itdtime_t> > bgr_frames;
  std::shared_ptr< timed_buffer<cv::Mat,itdtime_t> > depth_frames;
  std::shared_ptr< timed_buffer<cv::Mat,itdtime_t> > monoL_frames;
  std::shared_ptr< timed_buffer<cv::Mat,itdtime_t> > monoR_frames;
  std::shared_ptr< timed_buffer<cv::Mat,itdtime_t> > dispar_frames;
  
  std::thread mythread;

public:
  itd_parser( const size_t maxtbuf=0 )
    : device_stream_consumer_parser()
  {
    timebase_hz_sec = 1.0; //REV: why not? I'm using double...
    
    bgr_frames = std::make_shared< timed_buffer<cv::Mat,itdtime_t> >(maxtbuf);// bgr_frames;
    depth_frames = std::make_shared< timed_buffer<cv::Mat,itdtime_t> >(maxtbuf);// depth_frames;
    monoL_frames = std::make_shared< timed_buffer<cv::Mat,itdtime_t> >(maxtbuf);// monoL_frames;
    monoR_frames = std::make_shared< timed_buffer<cv::Mat,itdtime_t> >(maxtbuf);// monoR_frames;
    dispar_frames = std::make_shared< timed_buffer<cv::Mat,itdtime_t> >(maxtbuf);// dispar_frames;
    
    outputs["bgr"] = bgr_frames;
    outputs["depth"] = depth_frames;
    outputs["monoL"] = monoL_frames;
    outputs["monoR"] = monoR_frames;
    outputs["disparity"] = dispar_frames;
  }

  ~itd_parser()
  {
    stop();
  }
  
  //REV: read from the DSP params...? fps, color, etc.?
  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<itd_frameset>> dsp )
  {
    mythread = std::thread( &itd_parser::doloop, this, std::ref(loop), dsp );
  }
  
  void stop()
  {
    std::fprintf(stdout, "IN STOP: ITD parser\n");
    localloop.stop();
    JOIN( mythread );
    std::fprintf(stdout, "OUT STOP: ITD parser\n");
  }
  
  void doloop( loopcond& loop, std::shared_ptr<device_stream_publisher<itd_frameset>> dsp ) 
  {
    bool shouldsave=false;
    uint64_t mysaveidx=0;
    
    std::string bgrfname = "itd_bgr.mkv";
    std::string disparfname = "itd_disparity.mkv";
    std::string monoRfname = "itd_monoR.mkv";
    std::string monoLfname = "itd_monoL.mkv";
    std::string depthfname = "itd_depth.h5";
    //std::string depthfname = "itd_depth.mkv";
    
    
    
    
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> bgrsavet; //switch to pointers so I don't have to deal with move shit.
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> monoLsavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> monoRsavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> depthsavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> disparsavet;
    
    
    uint64_t frameidx=0;
    
    while( loop() && localloop() )
      {
	//REV: oops...too many loops
	wait_or_timeout_cond( [&]() -> bool { return (!localloop() || dsp->mbuf.size() > 0); },
			      loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );
	

	
	std::vector<itd_frameset> vec;
	dsp->mbuf.popallto(vec);


	
	//REV: never does in the middle of a vector I guess?
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  //REV: do saving threads??? Since h5 may take a super fucking long time?! Will I ever get backed up?
	  if( shouldsave && ((mysaveidx != saving_idx) || (!saving_raw)) )
	    {
	      shouldsave = false;
	      fprintf(stdout, "####### ITD -- TURNING *OFF* SAVING!\n");
	      for( auto& th : bgrsavet ) { th->saving.stop(); }
	      for( auto& th : depthsavet ) { th->saving.stop(); }
	      for( auto& th : monoLsavet ) { th->saving.stop(); }
	      for( auto& th : monoRsavet ) { th->saving.stop(); }
	      for( auto& th : disparsavet ) { th->saving.stop(); }
	      
	    }
	  
	  if( (saving_raw) && (mysaveidx != saving_idx) )
	    {
	      if( saving_idx != mysaveidx+1 )
		{
		  fprintf(stdout, "You skipped a saving index?\n");
		  exit(1);
		}

	      fprintf(stdout, "####### ITD -- TURNING ON SAVING!\n");
		  
	      mysaveidx = saving_idx;
	      shouldsave = true;


	      //REV: there are no move semantics for std::thread, so I can't emplace ;(
	      bgrsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>() );
	      bgrsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(bgrsavet.back()->saving), std::ref(bgrsavet.back()->tb),
					bgrfname, bgrfname+".ts", (int)((double)dsp->params["fps"]+0.5),
					std::ref(bgrsavet.back()->working) );

	      
	      
	      /*monoLsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      monoLsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(monoLsavet.back()->saving), std::ref(monoLsavet.back()->tb),
					monoLfname, monoLfname+".ts", (int)((double)dsp->params["fps"]+0.5),
					std::ref(monoLsavet.back()->working) );
	      */
	      	      
#ifdef ITD_SAVEMONOR
	      monoRsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      monoRsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(monoRsavet.back()->saving), std::ref(monoRsavet.back()->tb),
					monoRfname, monoRfname+".ts", (int)((double)dsp->params["fps"]+0.5),
					std::ref(monoRsavet.back()->working) );
	      
#endif

#ifdef ITD_SAVEDISPAR
	      disparsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      disparsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(disparsavet.back()->saving), std::ref(disparsavet.back()->tb),
					disparfname, disparfname+".ts", (int)((double)dsp->params["fps"]+0.5),
					std::ref(disparsavet.back()->working) );

	      
#endif

	      
	      depthsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      depthsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_depth<cv::Mat,double>, this,
					std::ref(loop), std::ref(depthsavet.back()->saving), std::ref(depthsavet.back()->tb),
					depthfname, depthfname+".ts", (int)((double)dsp->params["fps"]+0.5),
					std::ref(depthsavet.back()->working) );

	    }
	}
	//UNLOCK
	
	
	//Pop working guys? -- problem is I will mess up the iterator...just chunk them in struct?
	{
	  ERASEFINISHED( bgr );
	  ERASEFINISHED( depth );
	  ERASEFINISHED( dispar );
	  ERASEFINISHED( monoR );
	  ERASEFINISHED( monoL );
	}
	
	
	Timer lf;
	
	//REV: for each frameset to process (itd_frameset)
	for( auto& v : vec )
	  {
	    v.flip_and_convert(); //awb?

	    auto timestamp = v.timestamp;
	    
	    auto obgr = v.get_bgr();
	    auto odep = v.get_depth();
	    auto oimR = v.get_R();
	    auto oimDisp = v.get_disparity();
	    	
	    if( !obgr.has_value() || !odep.has_value() || !oimR.has_value() || !oimDisp.has_value() )
	      {
		fprintf(stderr, "Wtf something doesn't have value?\n");
		exit(1);
	      }
	
	    auto oimL = v.get_L();
	    if( oimL.has_value() )
	      {
		fprintf(stderr, "WTF imL should not have value...\n");
		exit(1);
	      }
	
	    auto bgr = obgr.value();
	    auto imR = oimR.value();
	    auto imDisp = oimDisp.value();
	    auto dep = odep.value();

	    	    
	   
	    if( shouldsave )
	      {
		
		bgrsavet.back()->tb.add( bgr, timestamp);
		
#ifdef ITD_SAVEMONOR
		monoRsavet.back()->tb.add( imR, timestamp );
#endif
#ifdef ITD_SAVEDISPAR
		disparsavet.back()->tb.add( imDisp, timestamp );
#endif
		
#ifdef HALFDEPTH
		cv::Mat halfdep;
		cv::resize( dep, halfdep, cv::Size(), 0.5, 0.5, HALFDEPTH_INTERPO );
		depthsavet.back()->tb.add( halfdep, timestamp ); //no need to clone since i resized into new array
#else
		depthsavet.back()->tb.add( dep, timestamp );
#endif

		//monoLtb.back()->add( imL.clone(), timestamp );
	      }

	
	    //Push to my timed buffers
	    if( isactive("bgr" ) )
	      {
		//bgr_frames->add( bgr.clone(), timestamp );
		bgr_frames->add( bgr, timestamp );
	      }
	    if( isactive("depth" ) )
	      {
		//depth_frames->add( dep.clone(), timestamp );
		depth_frames->add( dep, timestamp );
	      }
	    if( isactive("monoR" ) )
	      {
		//monoR_frames->add( imR.clone(), timestamp );
		monoR_frames->add( imR, timestamp );
	      }
	    if( isactive("disparity" ) )
	      {
		//dispar_frames->add( imDisp.clone(), timestamp );
		dispar_frames->add( imDisp, timestamp );
	      }

	    /*if( frameidx % FRSKIP == 0 )
	      {
		double mi, ma;
		cv::minMaxLoc( dep, &mi, &ma );
		fprintf(stdout, "DEPTH [%5.3f]  [%5.3f]\n", mi, ma );
		}*/
	    
	  } //end for v in vec

	++frameidx;
	if( frameidx % FRSKIP == 0 )
	  {
	    fprintf(stdout, "ITD: (FINISHED) Will handle [%ld] inputs! (%lf msec)\n", vec.size(), lf.elapsed()*1e3);
	  }
	
      } //end while loop and localloop
    
    for( auto& th : bgrsavet ) { JOIN( th->t ); }
    for( auto& th : monoLsavet ) { JOIN( th->t ); }
    for( auto& th : monoRsavet ) { JOIN( th->t ); }
    for( auto& th : disparsavet ) { JOIN( th->t ); }
    for( auto& th : depthsavet ) { JOIN( th->t ); }

    fprintf(stdout, "ITD -- finished joining all save threads! Ending doloop.\n");
    
  } //end doloop
  
};
