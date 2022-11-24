
#pragma once

#include <rtsp_av_decoder.hpp>

struct tobii3_parser
  : public device_stream_consumer_parser<tagged_avpkt_ptr>
{
private:
  std::shared_ptr<rtsp_receiver> receiver_ptr;

  //REV: since all packets are passed together, I'll just handle it this way?
  //REV: or, I'll have a guy who takes the mbuf, checks the ID (stream), and passes it to appropriate guy?
  std::shared_ptr< timed_buffer<cv::Mat, tobii3time_t> > scene_frames; //fuck it just call it scene frames here too lol
  std::shared_ptr< timed_buffer<std::vector<std::vector<std::byte>>,tobii3time_t> > audio_frames;
  std::shared_ptr< timed_buffer<cv::Mat, tobii3time_t> > eye_frames; //fuck it just call it scene frames here too lol

  std::shared_ptr< timed_buffer<json, tobii3time_t> > gaze_frames;
  std::shared_ptr< timed_buffer<json, tobii3time_t> > sync_frames;
  std::shared_ptr< timed_buffer<json, tobii3time_t> > imu_frames;
  std::shared_ptr< timed_buffer<json, tobii3time_t> > event_frames;
  
  std::shared_ptr<rtsp_av_decoder> scene_avdecoder;
  std::shared_ptr<rtsp_av_decoder> eye_avdecoder;
  
  std::thread decode_thread;
  
  
public:

  tobii3_parser( std::shared_ptr<rtsp_receiver> _receiver_ptr, const size_t maxtbuf=0 )
    : device_stream_consumer_parser(), receiver_ptr(_receiver_ptr)
  {
    timebase_hz_sec = -1;
    scene_frames = std::make_shared<timed_buffer<cv::Mat,tobii3time_t>>(maxtbuf);
    eye_frames = std::make_shared<timed_buffer<cv::Mat,tobii3time_t>>(maxtbuf);
    audio_frames = std::make_shared<timed_buffer<std::vector<std::vector<std::byte>>,tobii3time_t>>(maxtbuf);

    gaze_frames = std::make_shared<timed_buffer<json,tobii3time_t>>(maxtbuf);
    sync_frames = std::make_shared<timed_buffer<json,tobii3time_t>>(maxtbuf);
    imu_frames = std::make_shared<timed_buffer<json,tobii3time_t>>(maxtbuf);
    event_frames = std::make_shared<timed_buffer<json,tobii3time_t>>(maxtbuf);

    scene_avdecoder = std::make_shared<rtsp_av_decoder>( _receiver_ptr );
    eye_avdecoder = std::make_shared<rtsp_av_decoder>( _receiver_ptr );
      
    outputs["scene"] = scene_frames;
    outputs["eye"] = eye_frames;
    outputs["mic"] = audio_frames;

    outputs["gaze"] = gaze_frames;
    outputs["sync"] = sync_frames;
    outputs["imu"] = imu_frames;
    outputs["event"] = event_frames;
    
    if( !receiver_ptr )
      {
	fprintf(stderr, "ERROR: FMT CONTEXT IS NULL (REV: TODO: try to figure out format from stream with no format info)\n");
	exit(1);
      }
  } //end tobii3_parser constor

  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<tagged_avpkt_ptr>> dsp )
  {
     startlooping();
     decode_thread = std::thread( &tobii3_parser::decodeloop, this, std::ref(loop), dsp );
  } //end start (tobii3_parser)

  ~tobii3_parser()
  { stop(); }

  double get_timebase_hz_sec()
  {
    if( !scene_avdecoder ) { return -1; }
    {
      return scene_avdecoder->get_timebase_hz_sec();
    }
  }

  void stop()
  {
    localloop.stop();

    //REV: wait for it to finish looping/saving before returning.
    if( decode_thread.joinable() ) //Will this check whether it is running or not?
      {
	decode_thread.join();
      }
  }

  void decodeloop( loopcond& loop, std::shared_ptr<device_stream_publisher<tagged_avpkt_ptr>> dsp )
  {
    //My dict
    std::map<std::string,int> st;
    st["scene"] = 0;
    st["mic"] = 1;
    st["eye"] = 2;
    
    st["gaze"] = 3;
    st["sync"] = 4;
    st["imu"] = 5;
    st["event"] = 6;
    
    //set_loop_sleep_msec(10);

    std::string scenefname = "tobii3_scene" + RTEYE_VID_EXT;
    std::string micfname = "tobii3_mic.wav"; //" + RTEYE_VID_EXT; //REV: causes some issues, but can hold double data etc.?
    std::string eyefname = "tobii3_eye" + RTEYE_VID_EXT;
    std::string datafname = "tobii3_data.json";

    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,tobii3time_t>>> scenesavet;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,tobii3time_t>>> eyesavet;
    std::deque<std::shared_ptr<saver_thread_info<std::vector<std::vector<std::byte>>,tobii3time_t>>> micsavet;
    
    //REV: I'm just going to dump all the JSON into a single file...
    std::deque<std::shared_ptr<saver_thread_info<json,tobii3time_t>>> datasavet;
    
    
    bool shouldsave = false;
    std::uint64_t mysaveidx=0;
        
    
    {
      const bool withaudio=true;
      const bool withvideo=true;
      bool success = scene_avdecoder->init_decoding(withvideo, st["scene"], withaudio, st["mic"]);
      if( !success )
	{
	  fprintf(stderr, "Failed to start scene avdecoder\n");
	  localloop.stop();
	  return;
	}
    }
    
    {
      const bool withaudio=false;
      const bool withvideo=true;
      bool success = eye_avdecoder->init_decoding(withvideo, st["eye"], withaudio, -1);
      if( !success )
	{
	  fprintf(stderr, "Failed to start eye avdecoder\n");
	  localloop.stop();
	  return;
	}
    }
    

    uint64_t frameidx=0;
    Timer totaltime;
    Timer laststart;

    const bool memdrop = true;
    
    while( localloop() && loop() )
      {
	wait_or_timeout_cond( [&]() -> bool { return (!localloop() || dsp->mbuf.size() > 0); },
			      loop, dsp->mbuf.cv, dsp->mbuf.pmu, 5e6 );

#if DEBUG_LEVEL>50
	fprintf(stderr, "TOBII3 [%lf] sec since last iter (slept/blocked?)\n", laststart.elapsed());
#endif
	
	Timer etime;
	
	
	std::vector<tagged_avpkt_ptr> vec;
	auto npopped = dsp->mbuf.popallto(vec);
	
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  
	  if( shouldsave && ((mysaveidx != saving_idx) || !saving_raw ))
	    {
	      shouldsave = false;

	      fprintf(stdout, "####### TOBII3 -- TURNING *OFF* SAVING!\n");
	      
	      for( auto& th : scenesavet ) { th->saving.stop(); }
	      for( auto& th : eyesavet ) { th->saving.stop(); }
	      for( auto& th : micsavet ) { th->saving.stop(); }
	      for( auto& th : datasavet ) { th->saving.stop(); }
	    }


	  if( (saving_raw) && (mysaveidx != saving_idx) )
	    {
	      if( saving_idx != mysaveidx+1 )
		{
		  fprintf(stdout, "You skipped a saving index?\n");
		  exit(1);
		}

	      fprintf(stdout, "####### TOBII3 -- TURNING ON SAVING!\n");
	      mysaveidx = saving_idx;
	      shouldsave = true;

	      //REV: You must turn on streams before you start saving
	      //REV: -- disable turning on/off streams during saving?
	      scenesavet.push_back( std::make_shared<saver_thread_info<cv::Mat,std::uint64_t>>());
	      scenesavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,std::uint64_t>, this,
						 std::ref(loop), std::ref(scenesavet.back()->saving), std::ref(scenesavet.back()->tb),
						 scenefname, scenefname+".ts", (int)(scene_avdecoder->get_fps()+0.5),
						 std::ref(scenesavet.back()->working) );

	      eyesavet.push_back( std::make_shared<saver_thread_info<cv::Mat,std::uint64_t>>());
	      eyesavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,std::uint64_t>, this,
						 std::ref(loop), std::ref(eyesavet.back()->saving), std::ref(eyesavet.back()->tb),
						 eyefname, eyefname+".ts", (int)(eye_avdecoder->get_fps()+0.5),
						 std::ref(eyesavet.back()->working) );


	      //REV: HERE, get info from scene_avdecoder, and pu before working:
	      //const int samprate, const int numchan, const int bps, const bool issigned, const bool isplanar, const bool isfloating,
	      int sr=-1, bps=-1, nc=-1;
	      bool issigned=false, isplanar=false, isfloating=false;
	      bool gotinfo = scene_avdecoder->get_info( sr, bps, nc, issigned, isplanar, isfloating );
	      int64_t avtimebase = scene_avdecoder->get_timebase_hz_sec();
	      if( !gotinfo ) { fprintf(stderr, "WTF couldn't get info?!\n"); exit(1); }
	      micsavet.push_back( std::make_shared<saver_thread_info<std::vector<std::vector<std::byte>>,std::uint64_t>>());
	      micsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_aud<std::vector<std::vector<std::byte>>,std::uint64_t>,
					       this, std::ref(loop),
					       std::ref(micsavet.back()->saving),
					       std::ref(micsavet.back()->tb),
					       micfname, micfname+".ts",
					       sr, nc, bps, issigned, isplanar, isfloating, avtimebase,
					       std::ref(micsavet.back()->working) );
	      
	      
	      datasavet.push_back( std::make_shared<saver_thread_info<json,std::uint64_t>>());
	      datasavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_json<json,std::uint64_t>, this,
						std::ref(loop), std::ref(datasavet.back()->saving), std::ref(datasavet.back()->tb),
						datafname, datafname+".ts", std::ref(datasavet.back()->working) );
	      
	    }
	} //end LOCKGUARD
	
	{
	  ERASEFINISHED( scene );
	  ERASEFINISHED( eye );
	  ERASEFINISHED( mic );
	  ERASEFINISHED( data );
	}

	size_t nsceneframes=0;
	size_t neyeframes=0;
	for( auto& tpkt : vec )
	  {
	    auto avpkt = tpkt.pkt;
	    std::uint64_t pts = avpkt->pts;
	    int si = avpkt->stream_index;
	    
	    if(    si == st["gaze"]
		|| si == st["sync"]
		|| si == st["imu"]
		|| si == st["event"]
		)
	      {
		//REV: static cast uint8_t* to char*? What is the problem...? No network order single bytes.
		std::string datstr = std::string( (char*)(avpkt->data), avpkt->size ); //need to deep copy? Meh.
		
		auto jvec = parse_multiline_json( datstr );
		for( auto& j : jvec )
		  {
		    if( !j.empty() )
		      {
			if( si == st["gaze"] && isactive("gaze") )
			  { gaze_frames->add( j, pts, j.size() ); }
			else if( si == st["sync"] && isactive("sync") )
			  { sync_frames->add( j, pts, j.size() ); }
			else if( si == st["imu"] && isactive("imu") )
			  { imu_frames->add( j, pts, j.size() ); }
			else if( si == st["event"] && isactive("event") )
			  { event_frames->add( j, pts, j.size() ); }
			    //else //REV: can't do else since it will always catch if others are not isactive() (since is &&)
			  ///{ fprintf(stderr, "Wtf?! JSON? \n"); exit(1); }

			if( shouldsave )
			  {
			    datasavet.back()->tb.add( j, pts, j.size());
			  }
		      }
		  }
	      } //end if it is gaze, sync, imu, or event (i.e. json streams)
	    ////////////////// REV: AUDIO OR VIDEO STREAM IDX /////////////////
	    else if (   si == st["scene"]
		     || si == st["mic"]
		     || si == st["eye"]
		     )
	      {
		//REV: pass it to the right decoder...
		//REV: note failure may be because 1) decoders are not initialized 2) signal to stop decoding.
		if( si==st["scene"]  )
		  {
		    auto succ = scene_avdecoder->add_pkt_and_decode( tpkt );
		    if(false==succ) { fprintf(stdout, "Detected failure in scene decoding, stoppping\n"); localloop.stop(); }
		  }
		else if( si==st["mic"] )
		  {
		    auto succ = scene_avdecoder->add_pkt_and_decode( tpkt );
		    if(false==succ) { fprintf(stdout, "Detected failure in mic decoding, stoppping\n"); localloop.stop(); }
		  }
		else if( si==st["eye"]  )
		  {
		    auto succ = eye_avdecoder->add_pkt_and_decode( tpkt );
		    if(false==succ) { fprintf(stdout, "Detected failure in eye decoding, stoppping\n"); localloop.stop(); }
		  }
		
		//else
		//  { fprintf(stderr, "Wtf?! Video\n"); exit(1); }
	      } //end else if it is a video or audio stream...
	    else
	      {
		fprintf(stderr, "Tobii 3 -- Unknown stream [%d]\n", si);
		exit(1);
	      }
	    av_packet_unref(avpkt);
	    av_packet_free(&avpkt);

	    
	    //REV: try pop and push once per loop!
	    
	    uint64_t ts;
	    bool scenepopped = false;
	    do
	      {
		cv::Mat mat;
		scenepopped = scene_avdecoder->process_pop_front_vid( mat, ts );
		
		if( scenepopped )
		  {
		    uint64_t msizebytes = cvmatsize( mat );
		    ++nsceneframes;
		    ++frameidx; //Total frames since start.
		    if( shouldsave )
		      {
			scenesavet.back()->tb.add( mat, ts, msizebytes);
		      }
		    
		    if( isactive("scene") )
		      {
			scene_frames->add( mat, ts, msizebytes, memdrop ); //REV: not guaranteed to be contiguous memory...
		      }
		  }
	      } while( scenepopped == true );
	

	    std::vector<std::vector<std::byte>> audvec;
	    while( true == scene_avdecoder->process_pop_front_aud( audvec, ts ) )
	      {
		uint64_t vbytesize = vecvecbytesize( audvec );
		if( isactive("mic") )
		  {
		    audio_frames->add( audvec, ts, vbytesize, memdrop );
		  }
		
		if( shouldsave )
		  {
		    //fprintf(stdout, "Pushing back mic to save vec\n");
		    micsavet.back()->tb.add( audvec, ts, vbytesize );
		  }	
	      }

	    bool eyepopped=false;
	    do
	      {
		cv::Mat mat;
		eyepopped  = eye_avdecoder->process_pop_front_vid( mat, ts );
		if( eyepopped )
		  {
		    uint64_t msizebytes = cvmatsize( mat );
		    ++neyeframes;
		    if( shouldsave )
		      {
			eyesavet.back()->tb.add( mat, ts, msizebytes);
		      }
		    if( isactive("eye") )
		      {
			eye_frames->add( mat, ts, msizebytes, memdrop ); //REV: not guaranteed to be contiguous memory
		      }
		  }
	      } while( eyepopped );
	  }

#if DEBUG_LEVEL>0
#define NFRAMES_TO_TRIGGER_PRINT 3
	if( nsceneframes > NFRAMES_TO_TRIGGER_PRINT || neyeframes > NFRAMES_TO_TRIGGER_PRINT )
	  {
	    fprintf(stderr, "Took [%lf] sec to pop/push [%lu] (SCENE: %lu   EYE: %lu) packets to decoders (and decode?), and push to timed buffers/savers (note: total scene frames: [%lu] in [%5.2lf] seconds (=[%lf] fps))\n", etime.elapsed(), npopped, nsceneframes, neyeframes, frameidx, totaltime.elapsed(), frameidx/totaltime.elapsed());
	  }
#endif


	    
	if( LOOP_SLEEP_MSEC > 0 )
	  {
	    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MSEC));
	  }
	laststart.reset();
      } //end while true

    std::fprintf(stdout, "Joining saving threads (Tobii3)\n");
    for( auto& th : scenesavet ) { JOIN( th->t ); }
    for( auto& th : eyesavet ) { JOIN( th->t ); }
    for( auto& th : micsavet ) { JOIN( th->t ); }
    for( auto& th : datasavet ) { JOIN( th->t ); }
    std::fprintf(stdout, "DONE Joining saving threads (Tobii3)\n");
    
  } //end func decodeloop
  
}; //end tobii3_parser
