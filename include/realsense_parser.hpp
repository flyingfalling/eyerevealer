#pragma once




struct realsense_parser
  : public device_stream_consumer_parser<rs2::frame>
{
  std::shared_ptr< timed_buffer<cv::Mat,double> > bgr_frames;
  std::shared_ptr< timed_buffer<cv::Mat,double> > depth_frames;
  std::shared_ptr< timed_buffer<cv::Mat,double> > infraL_frames;
  std::shared_ptr< timed_buffer<cv::Mat,double> > infraR_frames;
  std::shared_ptr< timed_buffer<vec3f,double> > accel_frames;
  std::shared_ptr< timed_buffer<vec3f,double> > gyro_frames;
  
  //const double timebase_hz_sec=1000.0; //realsense lib -- always in milliseconds
  
  std::thread mythread;
    
  realsense_parser( const size_t maxtbuf=0 )
    : device_stream_consumer_parser()
  {
    timebase_hz_sec = 1e3; //milliseconds...see realsense doc. Global timestamps.
    
    bgr_frames = std::make_shared< timed_buffer<cv::Mat,double> >(maxtbuf);
    depth_frames = std::make_shared< timed_buffer<cv::Mat,double> >(maxtbuf);
    infraL_frames = std::make_shared< timed_buffer<cv::Mat,double> >(maxtbuf);
    infraR_frames = std::make_shared< timed_buffer<cv::Mat,double> >(maxtbuf);
    accel_frames = std::make_shared< timed_buffer<vec3f,double> >(maxtbuf);
    gyro_frames = std::make_shared< timed_buffer<vec3f,double> >(maxtbuf);

    outputs["bgr"] = bgr_frames;
    outputs["depth"] = depth_frames;
    outputs["infraL"] = infraL_frames;
    outputs["infraR"] = infraR_frames;
    outputs["accel"] = accel_frames;
    outputs["gyro"] = gyro_frames;
    
    //starttime_rs = std::numeric_limits<double>::min();
  }

  ~realsense_parser()
  {
    stop();
  }

  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<rs2::frame>> dsp )
  {
    mythread = std::thread( &realsense_parser::doloop, this, std::ref(loop), dsp );
  }
  
  void stop()
  {
    std::fprintf(stdout, "IN STOP: Realsense parser\n");
    localloop.stop();
    if(mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: Realsense parser\n");
  }





  //REV: TODO
  // I can optimize the raw file writers by sharing the timed_buffer normally published as outputs
  // Specifically, once it starts saving, just make sure nothing before start saving time is dropped, and walk through
  // timestamps/iterators up until normal current point (dropping if user would normally drop, otherwise keeping).
  // (even better...just keep a pointer to it?). Note, I'm cloning...
  
  void doloop( loopcond& loop, std::shared_ptr<device_stream_publisher<rs2::frame>> dsp )
  {
    std::fprintf(stdout, "IN DOLOOP: Realsense parser\n");
  
    rs2::align align_to_color(RS2_STREAM_COLOR);
    //rs2::decimation_filter dec_filter;
    //rs2::spatial_filter spat_filter;
    //rs2::hole_filter hole_filter;
    //Process acceleration etc. for camera pose:
    //  https://dev.intelrealsense.com/docs/rs-motion
    
    
    std::string colorfname="rs_color" + RTEYE_VID_EXT;
    std::string infraLfname="rs_infraL" + RTEYE_VID_EXT;
    std::string infraRfname="rs_infraR" + RTEYE_VID_EXT;
    std::string depthfname="rs_depth";

#ifdef USEH5
    depthfname += ".h5";
#endif

    //std::string depthfname="rs_depth" + RTEYE_VID_EXT;
    std::string gyrofname="rs_gyro.32FC3";
    std::string accelfname="rs_accel.32FC3";

    //rstimetype?
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> colorsavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> infraLsavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> infraRsavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,double>>> depthsavet;
    std::deque<std::shared_ptr<saver_thread_info<vec3f,double>>> gyrosavet;
    std::deque<std::shared_ptr<saver_thread_info<vec3f,double>>> accelsavet;
    
    bool shouldsave = false;
    std::uint64_t mysaveidx=0;
    const bool dropmem = true;
    
    uint64_t frameidx=0;
    Timer totaltime;
    
    while( localloop() && loop() )
      {
	//fprintf(stdout, "RS: Waiting for mbuf!\n");
	wait_or_timeout_cond( [&]() -> bool { return (!localloop() || dsp->mbuf.size() > 0); },
			      loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );
	
	//REV: just pop them all and process
	std::vector<rs2::frame> vec;
	dsp->mbuf.popallto(vec);
	//fprintf(stdout, "RS: got [%ld] in vec!\n", vec.size());
	
	//REV: unfortunately, seems there is no way to dump rs2::frames to a file (bag file?) that I can find easily.
	//So, I will need to dump real..which can get heavy.

	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  
	  if( shouldsave && ((mysaveidx != saving_idx) || !saving_raw ))
	    {
	      shouldsave = false;

	      fprintf(stdout, "####### RS -- TURNING *OFF* SAVING!\n");
	      
	      //REV: *FUCK* assume it always starts from 0
	      //and always goes up by one each time, and I never
	      //pop or delete them ;(
	      
	      for( auto& th : colorsavet ) { th->saving.stop(); }
	      for( auto& th : depthsavet ) { th->saving.stop(); }
	      for( auto& th : infraLsavet ) { th->saving.stop(); }
	      for( auto& th : infraRsavet ) { th->saving.stop(); }
	      for( auto& th : gyrosavet ) { th->saving.stop(); }
	      for( auto& th : accelsavet ) { th->saving.stop(); }
	    }
	  
	  //I should be saving, but I'm saving the wrong index!
	  //Need to start a new one.
	  if( (saving_raw) && (mysaveidx != saving_idx) )
	    {
	      if( saving_idx != mysaveidx+1 )
		{
		  fprintf(stdout, "You skipped a saving index?\n");
		  exit(1);
		}

	      fprintf(stdout, "####### RS -- TURNING ON SAVING!\n");
	      mysaveidx = saving_idx;
	      shouldsave = true;
	      
	      colorsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      colorsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(colorsavet.back()->saving), std::ref(colorsavet.back()->tb),
					  colorfname, colorfname+".ts", (int)((double)dsp->params["color_fps"]+0.5),
					  std::ref(colorsavet.back()->working) );
	      
	      
#ifdef RS_SAVEINFRAL
	      infraLsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      infraLsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(infraLsavet.back()->saving), std::ref(infraLsavet.back()->tb),
					  infraLfname, infraLfname+".ts", (int)((double)dsp->params["infraL_fps"]+0.5),
					  std::ref(infraLsavet.back()->working) );
#endif
	      

#ifdef RS_SAVEINFRAR
	      infraRsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      infraRsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,double>, this,
					std::ref(loop), std::ref(infraRsavet.back()->saving), std::ref(infraRsavet.back()->tb),
					  infraRfname, infraRfname+".ts", (int)((double)dsp->params["infraR_fps"]+0.5),
					  std::ref(infraRsavet.back()->working) );
#endif
	      depthsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,double>>());
	      depthsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_depth<cv::Mat,double>, this,
					std::ref(loop), std::ref(depthsavet.back()->saving), std::ref(depthsavet.back()->tb),
					  depthfname, depthfname+".ts", (int)((double)dsp->params["depth_fps"]+0.5),
					  std::ref(depthsavet.back()->working) );

	      gyrosavet.push_back( std::make_shared<saver_thread_info<vec3f,double>>());
	      gyrosavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vec3f<vec3f,double>, this,
					  std::ref(loop), std::ref(gyrosavet.back()->saving), std::ref(gyrosavet.back()->tb),
					  gyrofname, gyrofname+".ts", std::ref(gyrosavet.back()->working) );

	      accelsavet.push_back( std::make_shared<saver_thread_info<vec3f,double>>());
	      accelsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vec3f<vec3f,double>, this,
					  std::ref(loop), std::ref(accelsavet.back()->saving), std::ref(accelsavet.back()->tb),
					  accelfname, accelfname+".ts",  std::ref(accelsavet.back()->working) );
	      
	      
	    }
	} //end lock guard


	{ // threads must be named TAGsavet, e.g. colorsavet, etc.
	  ERASEFINISHED( color );
	  ERASEFINISHED( depth );
	  ERASEFINISHED( infraR );
	  ERASEFINISHED( infraL );
	  ERASEFINISHED( accel );
	  ERASEFINISHED( gyro );
	}

	
	
	//REV: pop front every time?
	for( auto& frame : vec )
	  {
	    //double parsetime, addtime, jointime;
	    //fprintf(stdout, "Doing for frame!\n");
	    try
	      {
		double timestamp = frame.get_timestamp(); //each timed_buffer may have separate zero time based on fps/etc. diff.
		//Need to coordiante with set_zero_first(), and then get that value and run set_zeroto(XXX) for each other one.

		//If it is a frameset (i.e. contains more than one actual frame...)
		if(rs2::frameset frameset = frame.as<rs2::frameset>())
		  {
		    ++frameidx;

		    Timer lf;
		    
		    //fprintf(stdout, "Parsing RS: [%lf]\n", timestamp);
		    //Camera, depth, etc. frame?
		    Timer t2;
		    rs2::frameset alignedfs = align_to_color.process(frameset);
		    
		    //BGR COLOR
		    rs2::video_frame rgb_frame = alignedfs.get_color_frame();
		    const int cw = rgb_frame.get_width();
		    const int ch = rgb_frame.get_height();
		    
		    cv::Mat rgb_image( cv::Size(cw, ch), CV_8UC3, (void*)rgb_frame.get_data(), cv::Mat::AUTO_STEP);
		    cv::Mat bgr_image;
		    cv::cvtColor( rgb_image, bgr_image, cv::COLOR_RGB2BGR ); //Will this copy?
		    bgr_image = bgr_image.clone(); // Not sure, may just reverse pointer...so be safe and clone
		    		    
		    //DEPTH 
		    rs2::depth_frame depth_frame = alignedfs.get_depth_frame();
		    const int dw = depth_frame.get_width();
		    const int dh = depth_frame.get_height();

		    //REV: these functions *DO NOT* copy or allocate data, nor release the data!
		    //REV: I should use this for the frame thing...? It just "points" to it? What is auto_step?
		    //Theory: if I pass by reference without clone, the cv::Mat just points to the depth_frame (get_frame), which
		    //itself is reference counted, and thus I point to "newer" data that that buffer has been over-written with
		    //in the meantime? I.e. alignedfs.get_depth_frame ptr is overwritten in the meantime. OK, so I need to copy.
		    cv::Mat depth_image( cv::Size(dw, dh), CV_16UC1, (void*)depth_frame.get_data(), cv::Mat::AUTO_STEP);
		    depth_image = depth_image.clone();
		    
		    //INFRA L
		    rs2::video_frame iframe1 = frameset.get_infrared_frame(1);
		    const int i1w = iframe1.get_width();
		    const int i1h = iframe1.get_height();
		    cv::Mat infraL_image( cv::Size(i1w, i1h), CV_8UC1, (void*)iframe1.get_data(), cv::Mat::AUTO_STEP);
		    infraL_image = infraL_image.clone();
		    
		    //INFRA R
		    rs2::video_frame iframe2 = frameset.get_infrared_frame(2);
		    const int i2w = iframe2.get_width();
		    const int i2h = iframe2.get_height();
		    cv::Mat infraR_image( cv::Size(i2w, i2h), CV_8UC1, (void*)iframe2.get_data(), cv::Mat::AUTO_STEP);
		    infraR_image = infraR_image.clone();

		    auto bgrsize = cvmatsize( bgr_image );
		    auto depthsize = cvmatsize( depth_image );
		    auto infraLsize = cvmatsize( infraL_image );
		    auto infraRsize = cvmatsize( infraR_image );
		    
		    
		    //Put in the timed buffers...
		    if( isactive( "bgr" ) )
		      {
			bgr_frames->add( bgr_image, timestamp, bgrsize, dropmem );
			//cv::imshow( "BGR", bgr_image );
		      }
		    if( isactive( "depth" ) )
		      {
			depth_frames->add( depth_image, timestamp, depthsize, dropmem );
			//cv::imshow( "depth", depth_image );
		      }
		    if( isactive( "infraL" ) )
		      {
			//REV: issue was not cloning it! But, I wasn't saving at the time, so...wtf?
			//REV: it was displaying a "newer" one, although it shouldn't? The memory inside was all fucked up?
			//REV: force a copy rather than a reference in timed_buffer?
			//Is cv::Mat copying "delayed"? I am reading from different thread...but still...
			infraL_frames->add(infraL_image, timestamp, infraLsize, dropmem );
			//cv::imshow( "infraL", infraL_image );
		      }
		    if( isactive( "infraR" ) )
		      {
			infraR_frames->add(infraR_image, timestamp, infraRsize, dropmem );
			//cv::imshow( "infraR", infraR_image );
		      }
		    //cv::waitKey(1);

		    if( shouldsave )
		      {
			colorsavet.back()->tb.add( bgr_image, timestamp, bgrsize );

#ifdef RS_SAVEINFRAL
			infraLsavet.back()->tb.add( infraL_image, timestamp, infraLsize );
#endif
			
#ifdef RS_SAVEINFRAR
			infraRsavet.back()->tb.add( infraR_image, timestamp, infraRsize);
#endif

#ifdef HALFDEPTH
			cv::Mat halfdep;
			cv::resize( depth_image, halfdep, cv::Size(), 0.5, 0.5, HALFDEPTH_INTERPO );
			auto depthsize2 = cvmatsize( halfdep );
			depthsavet.back()->tb.add( halfdep, timestamp, depthsize2 );
#else
			depthsavet.back()->tb.add( depth_image, timestamp, depthsize );//REV: wtf need to clone or memory gets fucked?
			//REV: YES it does LOLOL
			//Note -- it is not freeing memory WTF?! From saving...getting gobbed up
#endif
			
		      }
#if DEBUG_LEVEL>0
		    if( frameidx % FRSKIP == 0 )
		      {
			fprintf(stdout, "REALSENSE: (FINISHED frame %ld) (%lf msec) (total: %lf sec, i.e. %lf fps)\n", frameidx, lf.elapsed()*1e3, totaltime.elapsed(), frameidx/totaltime.elapsed());
		      }
#endif
		  }
		else
		  {
		    Timer tx;
		    auto motion = frame.as<rs2::motion_frame>();
		    // If casting succeeded and the arrived frame is from gyro stream
		    if(motion)
		      {
			if(motion.get_profile().stream_type() == RS2_STREAM_GYRO && 
			   motion.get_profile().format() == RS2_FORMAT_MOTION_XYZ32F )
			   {
			     rs2_vector gy = motion.get_motion_data();
			     auto gyrval = vec3f(gy.x,gy.y,gy.z);
			     
			     if( isactive( "gyro") )
			       {
				 gyro_frames->add( gyrval, timestamp, sizeof(gyrval) );
			       }
			     if( shouldsave )
			       {
				 //fprintf(stdout, "GYRO val is: %f %f %f\n", gyrval.x, gyrval.y, gyrval.z );
				 gyrosavet.back()->tb.add( gyrval, timestamp, sizeof(gyrval) );
			       }
			   }
			//REV: incorrectly was saving GYRO data twice up until 09 dec 2022 (had _GYRO here too) ;(
			if( motion.get_profile().stream_type() == RS2_STREAM_ACCEL && 
			    motion.get_profile().format() == RS2_FORMAT_MOTION_XYZ32F )
			  {
			    rs2_vector ac = motion.get_motion_data();
			    auto accval = vec3f(ac.x,ac.y,ac.z);
			    
			    if( isactive( "accel" ) )
			      {
				accel_frames->add( accval, timestamp, sizeof(accval) );
			      }
			    if( shouldsave )
			      {
				accelsavet.back()->tb.add( accval, timestamp, sizeof(accval) );
			      }
			  }
			//std::fprintf(stdout, "All motion proc: [%lf] msec\n", tx.elapsed()*1e3);
		      }
		    else
		      {
			fprintf(stderr, "REV: unrecognized frame type?!?!?\n");
		      }
		  } //end ELSE (not frameset -- i.e. must be IMU frame?!)
	      } //try
	    catch( rs2::error& e )
	      {
		fprintf(stdout, "REV: RS2 exception?!\n");
		std::cerr << e.what() << std::endl;
		loop.stop();
	      }
	    catch( std::exception& e )
	      {
		std::cerr << "REV: in RS: general exception " << e.what() << std::endl;
		loop.stop();
	      }
	    //fprintf(stdout, "Done for frame!\n");
	  } //for frame in vec
	
	
	//REV: this is pointless as it does not actually handle frames,
	//it handles gyro stuff too!
	
	
	//fprintf(stdout, "RS: Finished loop! \n");
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	
	//std::fprintf(stdout, "#### RS: Handled [%ld] frames -- single loop took [%lf] msec (%ld built up)\n", vec.size(), t.elapsed()*1e3, dsp->mbuf.size());
	//t.reset();
      } //while loop

    std::fprintf(stdout, "Joining saving threads (REALSENSE)\n");
    // JOIN saving threads.
    for( auto& th : colorsavet ) { JOIN( th->t ); }
    for( auto& th : infraLsavet ) { JOIN( th->t ); }
    for( auto& th : infraRsavet ) { JOIN( th->t ); }
    for( auto& th : depthsavet ) { JOIN( th->t ); }
    for( auto& th : accelsavet ) { JOIN( th->t ); }
    for( auto& th : gyrosavet ) { JOIN( th->t ); }
    
    std::fprintf(stdout, "OUT DOLOOP: Realsense parser\n");
  } //end void doloop
  
}; //end realsense recv struct.

