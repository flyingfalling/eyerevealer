
#pragma once

#include <rtsp_av_decoder.hpp>

struct pupilinvis_rtsp_vid_parser
  : public device_stream_consumer_parser<tagged_avpkt_ptr>
{
private:
  //AVFormatContext* fmt_context_ptr;
  std::shared_ptr<rtsp_receiver> receiver_ptr; //this is fucking nasty...
  std::shared_ptr< timed_buffer<cv::Mat, std::uint64_t> > world_frames; //fuck it just call it scene frames here too lol
  std::shared_ptr<rtsp_av_decoder> avdecoder;
  std::thread decode_thread;
  //my decoder? notmpegts_parser, but something else (just normal codec parser for the video I get... lol).
  // But, I want to know output size etc.?
  // Also, I have to save raw in here -_-; Can I save raw without fucked up shit? Without decoding? I could save raw packets
  // But then what would I do without the format info? I'd have to somehow save that as well? I guess I could do that, some
  // kind of SDP representation shit telling what the format of the packets are? But how the fuck to serialize those packets?
  // I have no idea what their format is...ugh. Maybe better to just decode...? But then I may be already performing some
  // color conversion or some shit, and thus losing info? I'm past caring haha.
  // I assume RTSP will not fuck with my frames before decoding (i.e re-ordering, missing?)? Or will it? Will having access to more
  // packets from later allow better decoding? No, because I'm not slurping at different rate...already have packets.
  
public:
  //REV: need the fmt_context to determine info about the streams? OK to const *& it?
  pupilinvis_rtsp_vid_parser( std::shared_ptr<rtsp_receiver> _receiver_ptr, const size_t maxtbuf=0 )
    : device_stream_consumer_parser(), receiver_ptr(_receiver_ptr)
  {
    timebase_hz_sec = -1;
    world_frames = std::make_shared<timed_buffer<cv::Mat,std::uint64_t>>(maxtbuf);
    avdecoder = std::make_shared<rtsp_av_decoder>(_receiver_ptr);
    outputs["world"] = world_frames;
    
    if( !receiver_ptr )
      {
	fprintf(stderr, "ERROR: FMT CONTEXT IS NULL (REV: TODO: try to figure out format from stream with no format info)\n");
	exit(1);
      }
    //no audio?
  }
  
  //REV: what will happen? It just won't let me in until it is ALL finished.
  //Can I start twice?

  //REV: I should stop and restart it whenever that shit happens. Just start and stop, not dealloc!
  void start( loopcond& loop, std::shared_ptr<device_stream_publisher<tagged_avpkt_ptr>> dsp )
  {
    lock_startstop(); //This may not work anyways, need to call this guy's thing here safely, will ensure that no one tries to
    //unalloc me while I am alloced.
    startlooping();
        
    
    fprintf(stdout, "PI RTSP VID PARSER -- Will Initializing decoding!\n");
    bool success = avdecoder->init_decoding(); //this will happen multiple times? It won't "delete" the codecs each time tho?
    fprintf(stdout, "--> DONE  PI RTSP VID PARSER -- Will Initializing decoding!\n");
    if( false == success )
      {
	fprintf(stderr, "Failed to initialize decoder!\n");
	unlock_startstop();
	stop();
	return;
      }
    decode_thread = std::thread( &pupilinvis_rtsp_vid_parser::decodeloop, this, std::ref(loop), dsp );
    unlock_startstop();
  }

  ~pupilinvis_rtsp_vid_parser()
  {
    stop();
  }

  //OVERLOADING
  double get_timebase_hz_sec()
  {
    //fprintf(stdout, "Attempting to get timebase of vid...\n");
    if( !avdecoder ) { return -1; }
    return avdecoder->get_timebase_hz_sec();
    //return 90000; //1000000; //microseconds (unix time).
  }

  void stop()
  {
    lock_startstop(); //This may not work anyways, need to call this guy's thing here safely, will ensure that no one tries to
    std::fprintf(stdout, "IN STOP: PI vid Parser\n");
    localloop.stop();
    if( decode_thread.joinable() ) //Will this check whether it is running or not?
      {
	decode_thread.join();
      }

    avdecoder->uninit_decoding();
    
    std::fprintf(stdout, "OUT STOP: PI vid Parser\n");
    
    unlock_startstop(); //This may not work anyways, need to call this guy's thing here safely, will ensure that no one tries to
  }

  void decodeloop( loopcond& loop, std::shared_ptr<device_stream_publisher<tagged_avpkt_ptr>> dsp )
  {
    //REV: create and do decoding in here, push to timed buffer if I get frame out.
    //Also, save "raw" if it is turned on :) (how to save it? -- heavy? Lossless?)

    //get packets from the MBUF in dsp, and pass them to the decoder. Get frames back (if available?).
    cv::Mat mat;

    //REV: *FUCK* pupil only streams gaze and world!
    //std::string worldfname = "pi_world.mkv";
    std::string worldfname = "pi_world" + RTEYE_VID_EXT;
    std::deque<std::shared_ptr<saver_thread_info<cv::Mat,std::uint64_t>>> worldsavet;
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

	      fprintf(stdout, "####### PI -- TURNING *OFF* SAVING!\n");
	      
	      for( auto& th : worldsavet ) { th->saving.stop(); }
	    }


	  if( (saving_raw) && (mysaveidx != saving_idx) )
	    {
	      if( saving_idx != mysaveidx+1 )
		{
		  fprintf(stdout, "You skipped a saving index?\n");
		  exit(1);
		}

	      fprintf(stdout, "####### PI -- TURNING ON SAVING!\n");
	      mysaveidx = saving_idx;
	      shouldsave = true;
	      
	      worldsavet.push_back( std::make_shared<saver_thread_info<cv::Mat,std::uint64_t>>());
	      worldsavet.back()->t =std::thread( &device_stream_consumer_parser::rawsaveloop_vid<cv::Mat,std::uint64_t>, this,
						 std::ref(loop), std::ref(worldsavet.back()->saving), std::ref(worldsavet.back()->tb),
						 worldfname, worldfname+".ts", (int)(avdecoder->get_fps()+0.5),
						 std::ref(worldsavet.back()->working) );
	      
	    }
	} //end LOCKGUARD
	
	{
	  ERASEFINISHED( world );
	}
	
	//REV: If I can't get a vid frame, use the last one? (forever? ew?). Or, give a blank frame? Or, white noise?
	//REV: encoding the video lossfully will lead to issues...
	//REV: it always gives back some data (maybe none). Before I know how big pupil invis video is, how do I set it? I know
	//REV: it has a certain size hardcoded...
	if( npopped > 0 )
	  {
	    //fprintf(stdout, "VID -- INSIDE Popped [%ld] from RTSP\n", npopped);
	    //Allow it to process as vector?
	    for( auto& tpkt : vec )
	      {
		//auto mpkt = tpkt.pkt;
		//fprintf(stdout, "Will decode\n");
		const bool customts=true;
		avdecoder->add_pkt_and_decode( tpkt, customts );
		//fprintf(stdout, "Done decode\n");
		av_packet_free(&tpkt.pkt); //I'm done with it now.
	      }
	  }
	
	
	uint64_t ts;
	//fprintf(stdout, "Will proces and push to timed frames\n");
	const bool usecustomts = true;
	while( true == avdecoder->process_pop_front_vid( mat, ts, usecustomts ) )
	  {
	    //fprintf(stdout, "VID Popping and pushing! [%ld]\n", ts);
	    //REV: copy it? Was it overwriting the old ones? Shit. How was it so efficient?
	    
	    auto matsize = cvmatsize( mat );
	    //REV: only push to timed buffer if I am streaming...
	    if( isactive("world") )
	      {
		world_frames->add( mat, ts, matsize, dropmem );
	      }
	    if( shouldsave )
	      {
		worldsavet.back()->tb.add( mat, ts, matsize);
	      }

	  }
	//fprintf(stdout, "DONE Will proces and push to timed frames\n");
	
	//Pop audios for now...not using it.
	uint64_t audpts;
	std::vector<std::vector<std::byte>> audvec;
	while( true == avdecoder->process_pop_front_aud( audvec, audpts ) )
	  {
	    //Do nothing

	    //REV: save/add to tbuffer as necessary.
	  }
	
	//std::this_thread::sleep_for(std::chrono::milliseconds(33));
	
	if( LOOP_SLEEP_MSEC > 0 )
	  {
	    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MSEC));
	  }
      } //while loop forever
    std::fprintf(stdout, "Joining saving threads (PUPILINVIS)\n");
    for( auto& th : worldsavet ) { JOIN( th->t ); }
    std::fprintf(stdout, "DONE Joining saving threads (PUPILINVIS)\n");
  }
};
