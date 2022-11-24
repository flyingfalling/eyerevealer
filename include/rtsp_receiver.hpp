
#pragma once


//REV: the classes are not public classes anyways, need access to the source of ffmpeg?
//REV: would like to compare against "system" library sizes...
//REV: maybe better to just include and compile ffmpeg myself...? As part of this? Then, it is not LINKING!
//REV: better to patch...
//That is fine... lol. But they can't be nicely included since they include references only used when compiling FFMPEG
//in their context and config and shit.
#include <rteye2ffmpeg/ffmpeg_rtsp_structs.h>

#include <av_receiver.hpp>
#include <tagged_avpkt_ptr.hpp>


//Receives RTSP data from pupil invis using the new API...
// Replaces the previous NDSI-4 shit (thanks, Obama, for more work for me...).

//REV: use AVPacket pointers? Or...? Pointer gives me more control...?
//REV: make a general RTSP receiver? Which pushes AVPacket to queue? But need context to get codec and some shit?
//REV: what to do when the avformatcontext has multiple "streams" (channels? programs?).
//REV: packets will be mixed among them...? I can make a queue for each? Each has a "marker"...
//REV: but, I can't sort them into multiple mbuf can I? Anyways, user can access my format shit to handle the packets after...?

uint64_t parse_ntp_time(const uint64_t ntpts)
{
  uint64_t sec = ntpts >> 32;
  uint64_t frac = ntpts & 0xFFFFFFFFULL;
  uint64_t usec = (frac * 1000000) / 0xFFFFFFFFULL;
  return (sec * 1000000) + usec;
}

struct rtsp_receiver
//: public device_stream_publisher<tagged_avpkt_ptr>
  : public av_receiver<tagged_avpkt_ptr>
{
protected:
  std::thread mythread;
  
  bool usecustomts=false;
  
  int64_t init_ntp_time_usec=-1;
  std::mutex ntpmu;
  
public:
  
  //REV: problem is, it should be related to when I actually first zero the time of the timed_buffer, not when I read the first
  //packet...so I should do this on the other side ;0
  //REV: basically check when I push to see if it is zeroed (was zeroed?)
  //REV: this is all retarded, just do everything here. NTP time is relative to this very first guy, as a jump
  void set_orig_ntp_time( const int64_t newntptime )
  {
    //REV: don't set if network packet thing is still not set...?
    if(newntptime <= 0 || newntptime == AV_NOPTS_VALUE)
      {
	return;
      }
    
    const std::lock_guard<std::mutex> lock(ntpmu);

    if( init_ntp_time_usec < 0 )
      {
	init_ntp_time_usec = newntptime;
	fprintf(stdout, "REV: [%s] RTSP RECEIVER -- set very first RTP time to %ld\n", myurl.c_str(), init_ntp_time_usec);
      }
  }

  int64_t get_orig_ntp_time()
  {
    const std::lock_guard<std::mutex> lock(ntpmu);
    return init_ntp_time_usec;
  }

  bool init( );
  
  int64_t get_stream_realtime_unixepoch_usec()
  {
    const std::lock_guard<std::mutex> lock(ntpmu);
    auto val = fmt_context->start_time_realtime;
    if( val < 0 )
      {
	//fprintf(stderr, "REV: get stream time less than zero?!\n");
	return -1;
      }
    else if( val == AV_NOPTS_VALUE )
      {
	//fprintf(stderr, "REV: get stream time was AV NOPTS value..?!\n");
	//return -1;
	return -1;
      }
    return val;
  }
  
  rtsp_receiver( const std::string& _myurl, const bool _usecustomts=false )
    :  av_receiver(_myurl), usecustomts(_usecustomts)
  {
    
  }
  
  ~rtsp_receiver()
  {
    stop();
    fprintf(stdout, "RTSP_RECEIVER destructor -- stopped and now returning [%s]\n", myurl.c_str());
    return;
  }

  void start( loopcond& loop )
  {
    lock_startstop();
    startlooping();
    
    fprintf(stdout, "START() of rtsp receiver [%s] -- Will init (RTSP RECEIVER)\n", myurl.c_str());
    //Get type of this stream too?

    reset_timer();

    bool success = init();
    
    set_timer_finished();
    if( !success )
      {
	unlock_startstop();
	stop();
      }
    //REV: don't start until codec is properly initialized? We have massive problems with all this other shit?
    
    fprintf(stdout, "START() finished init, now spin off thread [%s]\n", myurl.c_str());
    mythread = std::thread( &rtsp_receiver::doloop, this, std::ref(loop) ); //thread is in background via callbacks.
    unlock_startstop();
    return;
  }
  
  void stop()
  {
    lock_startstop();
    
    unset_codecs_initialized();
    
    fprintf(stdout, "IN: STOP: rtsp_receiver (%s)\n", myurl.c_str());
    localloop.stop();
    fprintf(stdout, "IN: STOP: rtsp_receiver JOINING (%s)\n", myurl.c_str());
    JOIN( mythread );
    fprintf(stdout, "IN: STOP: rtsp_receiver JOINED (%s)\n", myurl.c_str());
    
    const std::lock_guard<std::mutex> lock(mu);
    fprintf(stdout, "Got lock! [%s] (in stop)\n", myurl.c_str());
    if( fmt_context )
      {
	fprintf(stdout, "Freeing? RTSP receiver [%s]\n", myurl.c_str());
	// std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	fprintf(stdout, "Done sleeping...\n");
	//REV: this necessary? Shouldn't close_input do it...?
	reset_timer();
	if( fmt_context->pb )
	  {
	    fprintf(stderr, "Closing AVIO!\n");
	    avio_close(fmt_context->pb);
	  }
	set_timer_finished();
	
	fprintf(stdout, "Freed...\n");
	//REV: should I really close the input? I will reopen it again, so...yes?
	//REV: is there another way to check "isopen"?
	fprintf(stdout, "Closing format context...[%s]\n", myurl.c_str());
	reset_timer();
	avformat_close_input( &fmt_context );
	if( !fmt_context )
	  {
	    fprintf(stderr, "Closing the input dealloced? fuuuu\n");
	  }
	set_timer_finished();
      }
    else
      {
	fprintf(stderr, "Hmmm, in STOP but fmt_context has not yet been allocated? Oh well...\n");
      }
    
    //Don't do here, some other dude may still be using it...?
    //avformat_network_deinit();
    reset_timer();
    if( fmt_context )
      {
	avformat_free_context( fmt_context ); //REV: fuck parser has a pointer to this!!!!!!!!!
      }
    set_timer_finished();
    //Should already be deallocated wwtf
    
    fprintf(stdout, "OUT: STOP: rtsp_receiver [%s]\n", myurl.c_str());
    fprintf(stdout, "Passing lock! [%s] (in stop)\n", myurl.c_str());
    unlock_startstop();
    return;
  }


  
  
  void print_stream_status()
  {
    auto ioctx = fmt_context->pb;
    if( ioctx )
      {
	fprintf(stdout, "Will print stream status...\n");
	//REV, note fmt_context->pb is an AVIOContext*, so we can get ->buffer_size, buf_ptr, buf_end, pos.. can I use ftell etc.?
	fprintf(stdout, "STREAM STATUS:   buf size: [%d]   buf_start: [%p]  buf_now: [%p]  buf_end: [%p]   pos: [%ld]   (now - start: [%ld])\n", ioctx->buffer_size, ioctx->buffer, ioctx->buf_ptr, ioctx->buf_end, ioctx->pos, ((int64_t)ioctx->buf_end - (int64_t)ioctx->buf_end) );
      }
    else
      {
	//fprintf(stderr, "IOCTX is null...whoa?\n");
      }
    return;
  }
			   
  void doloop( loopcond& loop )
  {
    //Wait, why do I need codecs?
    /*
    while( loop() && localloop() && !get_codecs_initialized() )
      {
	fprintf(stdout, "Waiting for codecs to be initialized...[%s]\n", myurl.c_str());
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    fprintf(stdout, "DONE -- codecs initialized!...[%s]\n", myurl.c_str());
    */

    
    
    while( loop() && localloop() )
      {
	bool added=false;
	
	//fprintf(stdout, "RTSP RECEIVER [%s] looping\n", myurl.c_str() );
	//Allocate it here, I will not free it though (I am adding pointer to MBUF -- the consumer will be responsible for freeing
	// it and unreference counting it etc. via av_packet_free(&packet);
	AVPacket* packet = av_packet_alloc();
	
	//print_stream_status();
	//REV: what if there is nothing to read? Should I sleep? Should I not read until there is no more to read, i.e. in chunks?
	int v=-1;
	int64_t ntptime;
	int64_t origntptime;
	{
	  //fprintf(stdout, "Getting lock! [%s] (in loop)\n", myurl.c_str());
	  const std::lock_guard<std::mutex> lock(mu);
	  //fprintf(stdout, "Got lock! [%s] (in loop)\n", myurl.c_str());
	  //REV: pass it a callback to stop here? So it won't hang here?
	  reset_timer();
	  v = av_read_frame( fmt_context, packet ); //REV this will block or will it always return?
	  set_timer_finished();
	  
	  ntptime = get_stream_realtime_unixepoch_usec();
	  set_orig_ntp_time(ntptime);
	  origntptime = get_orig_ntp_time();
	  
	  //fprintf(stdout, "Passing lock! [%s] (in loop)\n", myurl.c_str());
	}
	
	//fprintf(stdout, "RTSP RECEIVER [%s] read frame...\n", myurl.c_str() );
	if( 0 == v )
	  {
	    //#define RTSP_DEBUG_LEVEL 11
#ifdef RTSP_DEBUG_LEVEL
#if RTSP_DEBUG_LEVEL>10
	    fprintf(stdout, "[%s]  RTSP packet :size: [%d]  pts: [%ld]  dts: [%ld]\n", myurl.c_str(), packet->size, packet->pts, packet->dts );
#endif
#endif

	    //Set original private NTP time if needed.
	    //If it is set, I need to compare against the current
	    
	    
	    
	    //REV: pts zero is realtime. So, I need to just store everything in realtime...easy, right?
	    //REV: lol yea just use uh, huge monolithic PTS!
	    
	    if( false == usecustomts )
	      {
		mbuf.addone( tagged_avpkt_ptr( packet, 0 ), avpktsize( packet ) );
		added=true;
	      }
	    else
	      {
		
#define USE_PRIVATE_FFMPEG
#ifndef USE_PRIVATE_FFMPEG

		if( ntptime >= 0 )
		  {
		    if( origntptime < 0 )
		      {
			fprintf(stderr, "Wtf? This should literally never happen since they are set at same time...\n");
			exit(1);
		      }

		    //REV: microsecs, so 1e6/sec
		    double bigoffsetsec = (ntptime - origntptime)/1e6;
		    auto streamtb = fmt_context->streams[packet->stream_index]->time_base;
#if DEBUG_LEVEL > 10
		    if( streamtb.den != 90000 || streamtb.num != 1)
		      {
			fprintf(stderr, "NUM/DEN is not 1/90k: [%d]/[%d]\n", streamtb.num, streamtb.den);
			//exit(1);
		      }
#endif
		
		    int64_t mypts = packet->pts;
		    if( mypts >= 0 )
		      {
			double smalloffsetsec = (static_cast<double>(mypts)/streamtb.den) * streamtb.num;
		    
			//fprintf(stdout, "[%s] Big: [%lf]  Small: [%lf]\n", myurl.c_str(), bigoffsetsec, smalloffsetsec );
		    
			double fulloffsetsec = bigoffsetsec + smalloffsetsec;
			if( fulloffsetsec < 0 )
			  {
			    fprintf(stderr, "Wtf neg time?! \n");
			    fprintf(stderr, "[%s] Big: [%lf]  Small: [%lf]\n", myurl.c_str(), bigoffsetsec, smalloffsetsec );
			    fprintf(stderr, "PTS: [%ld]\n", mypts);
			    exit(1);
			  }
			else
			  {
			    uint64_t fulloffsetpts = (uint64_t)((fulloffsetsec * streamtb.den) / streamtb.num);
			    
			    mbuf.addone( tagged_avpkt_ptr( packet, fulloffsetpts ), avpktsize( packet ) );
			    added=true;
			  }
		      }
		    else
		      {
			fprintf(stderr, "Negative PTS value? Perhaps AV_NOPTS_VALUE? PTS=[%ld], ==NOPTS? [%s]\n", mypts, mypts==AV_NOPTS_VALUE ? "yes" : "no" );
		      }
		  }
		  
#else //#ifdef USE_PRIVATE_FFMPEG


		    /*
		      //Here is my sync shit.
		      s->last_rtcp_ntp_time  = AV_RB64(buf + 8);
		      s->last_rtcp_timestamp = AV_RB32(buf + 16);

		      uint64_t 	last_rtcp_ntp_time
 
int64_t 	last_rtcp_reception_time
 
uint64_t 	first_rtcp_ntp_time
 
uint32_t 	last_rtcp_timestamp
 
int64_t 	rtcp_ts_offset
//REV: wtf is unwrapped?
//REV: base timestamp is the zero timestamp for "this" guy... (i.e. current timestamp of this packet, most recent content packet).
//It will always just be the last RTCP timestamp
		     */
		    
		    //REV: get private RTP data (or...try to!)
		if( fmt_context->priv_data )
		  {
		    const int myidx = 0; //e.g. video audio etc. -_-. Note this assumes only one stream...oh well.
		    RTSPState* rt = (RTSPState*)fmt_context->priv_data;
			
		    for( int i=0; i<rt->nb_rtsp_streams; ++i)
		      {
			RTSPStream* rtsp_st = rt->rtsp_streams[i]; // Check rtsp_st->stream_index is mine!!
			RTPDemuxContext *rtpctx = (RTPDemuxContext*)rtsp_st->transport_priv;
			    
			if( rtsp_st->stream_index == packet->stream_index )
			  {
			    //auto tmpts = rtpctx->unwrapped_timestamp;

			    //only if base time is set!
			    //REV: fuck...is this in NETWORK ORDER??!!?!? Nah...
			    if( rtpctx->last_rtcp_timestamp && rtpctx->last_rtcp_ntp_time && rtpctx->timestamp )
			      {
				uint32_t content_timestamp_delta = rtpctx->timestamp - rtpctx->last_rtcp_timestamp;
				uint64_t lastntp = rtpctx->last_rtcp_ntp_time;
				    
				auto streamtb = fmt_context->streams[packet->stream_index]->time_base;
				    
				//Timetamp is in timebase i.e. 90k, ntp time is 1e6
				//double ntpsec = lastntp / 1e6;
				uint64_t ntpusec = parse_ntp_time(lastntp);
				double ntpsec = ntpusec / 1e6;
				double dsec = (content_timestamp_delta / (double)streamtb.den) * streamtb.num;
				double fullsec = ntpsec + dsec;
				uint64_t ptsts = (fullsec * streamtb.den) / streamtb.num;
				
				tagged_avpkt_ptr tpkt( packet, ptsts );
				mbuf.addone( tpkt, avpktsize( packet ) );
				added = true;
			      }
			  }
		      }
		  }
			/*
			
#define DEBUG_CUSTOMTS
#ifdef DEBUG_CUSTOMTS
			    fprintf(stdout, "[%s]:[%d] (true stream index: %d): last RTP packet (read?) is of time [%u], first time [%lu] (unwrapped %lu) (offset %lu)\n", myurl.c_str(), i, rtsp_st->stream_index, rtpctx->timestamp, rtpctx->first_rtcp_ntp_time, rtpctx->unwrapped_timestamp, rtpctx->range_start_offset);
			    auto timestamp = rtpctx->timestamp;
			    auto utimestamp = rtpctx->unwrapped_timestamp;
			    //How do we know the time base? Just assume 90k...?
			    double tsassec = timestamp / 90000.0;
			    double utsassec = utimestamp / 90000.0;
			    auto ste = streamtime.elapsed();
			    auto starttime = fmt_context->start_time_realtime;
			    uint64_t mypts = packet->pts;
			    double sec_from_starttime = (mypts / 90000.0);
			    double unixt_sec = (starttime/1e6) + sec_from_starttime;
			    fprintf(stdout, "[%lf]  PTS: [%lu] (%lf unix) TS: [%lf]   UTS: [%lf]    ZEROED: [%lf]   [%lf] (Note: starttime realtime: [%lu])\n", ste, mypts, unixt_sec, tsassec, utsassec, tsassec-ste, utsassec-ste, starttime);
#endif
			
			    if( rtsp_st->stream_index == myidx )
			      {
				auto tmpts = rtpctx->unwrapped_timestamp;
			    
				int64_t lastntp = get_last_orig_ntp_time();
			    	  
				if( lastntp != ntptime )
				  {
				    if( ntptime >= 0 )
				      {
					fprintf(stderr, "REV: [%s] -- detected change in NTP start time of stream! prev [%ld] -> [%ld] ([%lf] sec)\n", myurl.c_str(), lastntp, ntptime, (double)(ntptime-lastntp)/1e6);
				      }
				    else
				      {
					fprintf(stderr, "REV: [%s] -- NTP start time set to -1 wtf? Prev [%ld]\n", myurl.c_str(), lastntp);
				      }
				  }
				else
				  {
				    fprintf(stderr, "Last NTP is same as this NTP: [%ld] -> [%ld]\n", lastntp, ntptime);
				  }
			    
				//REV: doesn't work this way. I need to "zero" everything.
				//REV: i.e. first guy should be just fucking zero NTP time.
				//REV: i.e. correction will be in the same timebase...
				//REV: so...I need to keep track of original pts time (custom ts time...)
				//REV: because I don't know how many customts I skipped!
				//REV: can I just use PTS? Nah...
				//uint64_t elapsed_usec_since_last_start =
				//REV: just always offset from the most recent guy, and sum together diffs from the beginning.
				//But, then I need to know how far I am from the first...I could use PTS for that?
				if( ntptime >= 0 && origntptime >= 0 )
				  {
				
				    //ntp time of this packet's stream, minus original start time of first packets...
				    auto offset = ntptime - origntptime; //offset in usec...
				
				    double offsetsec = offset/1e6;
				
				    //Should lock to access this...
				    auto tb = fmt_context->streams[myidx]->time_base;
				    double fixedoffset = (offsetsec * tb.den) / tb.num; //in time base fractions of second.
				    fprintf(stdout, "[%s] Manipulating timestamp -- start time differs [%ld] vs [%ld], diff=[%lf] sec. Modifying forward [%lf] (%d/%d)\n", myurl.c_str(), origntptime, ntptime, offsetsec, fixedoffset, tb.num, tb.den );
				    int64_t fixed_ts = tmpts + (uint64_t)fixedoffset;
				    tmpts = fixed_ts;
				  }
				//REV: need to add correct offset from very original to current ts if it is set NTP... (since stream
				// may have been stopped and restarted and thus timestamps may represent different no-sequential NTP times
				//, but timed buffer doesn't know that).
				//auto origntptime = get_orig_ntp_time();
				//ntptime is ntptime of this packet
				//Only allow if we know the start time? ;/ Nasty...?
				//REV: fuck, I don't know if it will ever provide me with good NTP times...FUCK.
				//I can't wait forever...can I? Fuck it, force NTP times...

				if( ntptime >= 0 )
				  {
				    tagged_avpkt_ptr tpkt( packet, tmpts );
			      

				    //fprintf(stdout, "RTSP RECEIVER [%s] attempting to push pkt...\n", myurl.c_str() );
				    mbuf.addone( tpkt ); //This will fill up quite quickly...I am allocating packets very fast...
				    //fprintf(stdout, "Added one!!\n");
				    //break;
				    if( added )
				      {
					fprintf(stderr, "REV: wtf added twice rtsp receiver\n");
				      }
				    added = true;
				  }
			      }
			  }
			//rtpctx->first_rtcp_ntp_time
			//rtpctx->base_timestamp
		      }
		    if( !added )
		      {
			fprintf(stderr, "REV: wtf never added a packet, problem with timing info (possibly due to wrong stream etc.?\n");
		
		      }
			*/
	      	      
#endif //USE_PRIVATE_FFMPEG
	      } //end else (if not usecustomts)
	  } //end if 0==v
	else
	  {
	    //REV; print errno stringhere.
	    fprintf(stderr, "REV: ERROR/WARNING -- V was errorful (not zero)...? V=[%d] (%s)\n", v, myurl.c_str());
	    std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(v).c_str());
	    localloop.stop();
	    //OR -- if not, set loop to false (indicating some error?)
	    //double sleeptime_sec = 0.005; //3 msec
	    //loop.sleepfor( sleeptime_sec );
	    //dealloc packet!
	    
	  }
	if( !added )
	  {
	    av_packet_unref(packet); //should not be needed..
	    av_packet_free(&packet);
	  }
      }
  }
  
};

int fmtintcallback(void* opaque)
{
  //REV: static cast..
  rtsp_receiver* myrecvr = (rtsp_receiver*)opaque;
  
  bool shouldgo=myrecvr->get_should_go();

  //fprintf(stderr, "LOL calling fmt interrupt callback!\n");

  //REV: check a timeout as well, if islooping etc., is true, but I'm stuck in a single given call (how do I know which one?)
  //Then what? I need a global mutex, and a global unique static indexer type thing with start-times? And current time...
  //Each time, I don't know which one "called" me though. Give it a timeout after trying to "init" shit? I.e. in the loop...
  const double TIMEOUT_SEC = 4.0;
  if( myrecvr->get_elapsed() > TIMEOUT_SEC )
    {
      //fprintf(stderr, "\n\n\n\nWHOA, INTERRUPTED FROM TIMEOUT!\n");
      return 1;
    }
  
  if( false == shouldgo )
    {
      //fprintf(stderr, "\n\n\n\nWHOA, INTERRUPTED FROM SHOULD NOT GO!\n");
      return 1;
    }
  
  
  //fprintf(stdout, "Calling int callback!!!\n");
  if( false == myrecvr->islooping() )
    {
      fprintf(stdout, "FORMAT INTERRUPT CALLBACK: detected not looping!!\n");
      return 1;
    }
  return 0;
}


bool rtsp_receiver::init( )
{
  //INIT NETWORK -- REV: It is not recommended, and will be deprecated. Only exists for old openssh and gnuTLS impls
  //REV: this should only happen once per whole program? Or per header etc.?
  //Actually, before THREAD is created? Note:
  //https://ffmpeg.org/doxygen/trunk/group__lavf__core.html#ga245f2875f80ce67ec3d1e0f54dacf2c4
  //avformat_network_init();

  
  fprintf(stdout, "RTSP RECEIVER, in init, waiting for lock!\n");
  const std::lock_guard<std::mutex> lock(mu);
  
  fprintf(stdout, "RTSP RECEIVER, Got lock! Will alloc!!\n");
  fmt_context = avformat_alloc_context();
  
  if( !fmt_context )
    {
      fprintf(stderr, "RTSP Receiver INIT() -- fmt context not exist!?\n");
      return false;
    }
  fprintf(stdout, "RTSP Receiver -- Setting callbacks: [%s]\n", myurl.c_str() );
  
  //if it's not initialized yet...what to do?
  fmt_context->interrupt_callback.opaque = (void*)this;
  fmt_context->interrupt_callback.callback = &fmtintcallback;
  
  
  
  fprintf(stdout, "RTSP Receiver -- Opening stream: [%s]\n", myurl.c_str() );
    
  AVDictionary *opts = NULL;

  av_dict_set(&opts, "buffer_size", "655360", 0);
  //reorder_queue_size
  av_dict_set(&opts, "recv_buffer_size", "655360", 0); //only for UDP...?
  av_dict_set(&opts, "thread_queue_size", "512", 0); //REV: not sure what this is ... seems to be read both for demux and mux?
  // Set to 8 or 1 depending on number of input files?
  //av_dict_set(&opts, "rtsp_transport", "tcp", 0);
  //av_dict_set(&opts, "rtsp_flags", "prefer_tcp", 0);
  
  
  reset_streamtime(); //REV: Should either do here or after the call, not sure...
  int averr1 = avformat_open_input(&fmt_context, myurl.c_str(), NULL, &opts);
  if( averr1 != 0 )
    {
      fprintf(stderr, "ERROR opening input (RTSP: [%s])\n", myurl.c_str() );
      std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(averr1).c_str());
      //stop();
      return false;
    }
  
  fprintf(stdout, "RTSP Receiver -- Opened! Getting format...: [%s]\n", myurl.c_str() );
  
  //REV: not sure what this is for, just puts the data in every packet?
  //REV: won't that write over packet-specific data...? :(
  av_format_inject_global_side_data(fmt_context);
  
  //REV: I actually don't even need this (except to dump format)
  int averr2 = avformat_find_stream_info(fmt_context, NULL);
  if( averr2 < 0 )
    {
      fprintf(stderr, "ERROR fmt_context contains no streams? [%s]\n", myurl.c_str());
      std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(averr2).c_str());
      //stop();
      return false;
      //exit(1);
    }

  //REV: it may notice that it is DC for example, and then save this shit from hanging..
  fprintf(stdout, "RTSP Receiver -- Found stream info: [%s], dumping it...\n", myurl.c_str() );
  
  
  int zero_if_input = 0;
  av_dump_format(fmt_context, 0, myurl.c_str(), zero_if_input);
  
  fprintf(stdout, "Finished init for RTSP receiver [%s]\n", myurl.c_str() );
  
  return true;
}






//REV: They seek in ffplay ffmpeg...
/* if seeking requested, we execute it */
/*if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = start_time;
         add the stream start time
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                    is->filename, (double)timestamp / AV_TIME_BASE);
        }
*/


//check is->realtime(?) do they change how they respond?



// REV: they play:
//if (is->paused != is->last_paused) {
//            is->last_paused = is->paused;
//            if (is->paused)
//                is->read_pause_return = av_read_pause(ic);
//            else
//                av_read_play(ic);


//REV: hm, does it start streaming by default? I can read it, but the "transferring" light is not on...

//REV: this is for UDP?
//ff_rtp_send_punch_packets(rtsp_st->rtp_handle);

//REV: otherwise I just wipe my packet queue?
/*
ff_rtp_reset_packet_queue(rtpctx);
                rtpctx->last_rtcp_ntp_time  = AV_NOPTS_VALUE;
                rtpctx->first_rtcp_ntp_time = AV_NOPTS_VALUE;
                rtpctx->base_timestamp      = 0;
                rtpctx->timestamp           = 0;
                rtpctx->unwrapped_timestamp = 0;
                rtpctx->rtcp_ts_offset      = 0;
*/


//ff_rtsp_send_cmd(s, "PLAY", rt->control_uri, cmd, reply, NULL);


//REV: check if it calls "play" explicitly or not?

//REV: So, I don't need to get from play, but if I don't will it randomly DC and shit? Check it...


//RV: OK, so I have to call *SOME* type of command? Assume ffmpeg is not doing it for me?


//REV: default options for e.g. ffmpeg thing, reduce timeout time?

//REV: why does the RTSP URL disappear? So confused. For like 20 seconds at a time...although the API stays?

//REV: Do I need to have a better way to check the RTSP info? Try it in python...and check what is happening?
