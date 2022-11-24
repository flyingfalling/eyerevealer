
#pragma once

#include <av_receiver.hpp>
#include <tagged_avpkt_ptr.hpp>

struct av_file_receiver
//: public device_stream_publisher<tagged_avpkt_ptr>
  : av_receiver<tagged_avpkt_ptr>
{
protected:
  std::thread mythread;
  
public:

  bool init( );
  
    
  av_file_receiver( const std::string& _myurl )
    : av_receiver( _myurl )
  {
    
  }
  
  ~av_file_receiver()
  {
    stop();
    fprintf(stdout, "AV_FILE_RECEIVER destructor -- stopped and now returning [%s]\n", myurl.c_str());
    return;
  }

  void start( loopcond& loop )
  {
    lock_startstop();
    startlooping();
    
    fprintf(stdout, "START() of av file receiver [%s] -- Will init (av file RECEIVER)\n", myurl.c_str());
    //Get type of this stream too?

    reset_timer();

    bool success = init();
    
    set_timer_finished();
    if( !success )
      {
	fprintf(stderr, "REV: failed to successfully init!\n");
	unlock_startstop();
	stop();
      }
    //REV: don't start until codec is properly initialized? We have massive problems with all this other shit?
    
    fprintf(stdout, "START() finished init, now spin off thread [%s]\n", myurl.c_str());
    mythread = std::thread( &av_file_receiver::doloop, this, std::ref(loop) ); //thread is in background via callbacks.
    unlock_startstop();
    return;
  }

  //REV: FUCK this will tear out the fmt_context from under the feet of decoding contexts and etc.!?
  void stop()
  {
    lock_startstop();
    
    unset_codecs_initialized();
    
    fprintf(stdout, "IN: STOP: av_file_receiver (%s)\n", myurl.c_str());
    localloop.stop();
    fprintf(stdout, "IN: STOP: av_file_receiver JOINING (%s)\n", myurl.c_str());
    JOIN( mythread );
    fprintf(stdout, "IN: STOP: av_file_receiver JOINED (%s)\n", myurl.c_str());
    
    const std::lock_guard<std::mutex> lock(mu);
    fprintf(stdout, "Got lock! [%s] (in stop)\n", myurl.c_str());
    if( fmt_context )
      {
	fprintf(stdout, "Freeing? av_file receiver [%s]\n", myurl.c_str());
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
    
    fprintf(stdout, "OUT: STOP: av file_receiver [%s]\n", myurl.c_str());
    fprintf(stdout, "Passing lock! [%s] (in stop)\n", myurl.c_str());
    unlock_startstop();
    return;
  }

    
  
  
			   
  void doloop( loopcond& loop )
  {
        
    while( loop() && localloop() )
      {
	bool added=false;
	
	//fprintf(stdout, "AV FILE RECEIVER [%s] looping\n", myurl.c_str() );
	//Allocate it here, I will not free it though (I am adding pointer to MBUF -- the consumer will be responsible for freeing
	// it and unreference counting it etc. via av_packet_free(&packet);
	AVPacket* packet = av_packet_alloc();
	
	//REV: what if there is nothing to read? Should I sleep? Should I not read until there is no more to read, i.e. in chunks?
	int v=-1;
	{
	  //fprintf(stdout, "Getting lock! [%s] (in loop)\n", myurl.c_str());
	  const std::lock_guard<std::mutex> lock(mu);
	  //fprintf(stdout, "Got lock! [%s] (in loop)\n", myurl.c_str());
	  //REV: pass it a callback to stop here? So it won't hang here?
	  reset_timer();
	  v = av_read_frame( fmt_context, packet ); //REV this will block or will it always return?
	  set_timer_finished();
	}
	
	//fprintf(stdout, "RTSP RECEIVER [%s] read frame...\n", myurl.c_str() );
	if( 0 == v )
	  {
	    //fprintf(stdout, "Read frame succ\n");
	    mbuf.addone( tagged_avpkt_ptr( packet, 0 ) );
	    added=true;
	  } //end if 0==v
	else
	  {
	    if( v == AVERROR_EOF )
	      {
		fprintf(stderr, "END OF FILE DETECTED V=[%d] (%s)\n", v, myurl.c_str());
		std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(v).c_str());
		//stop();
		set_eof(true);
		av_packet_free(&packet); 
		return;
		//localloop.stop(); //don't dealloc, just stop looping... note may it throw shit off since "looping" will
		//return false but I will have all shit allocated?
	      }
	    else
	      {
		//REV; print errno stringhere.
		fprintf(stderr, "REV: ERROR/WARNING -- V was errorful (not zero)...? V=[%d] (%s)\n", v, myurl.c_str());
		std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(v).c_str());
	      }
	  }
	
	if( !added )
	  {
	    av_packet_free(&packet); //REV: I could just keep a single alloc packet...? And its pointed to stuff would change...
	  }
      }
  }
  
};

int fmtintcallback(void* opaque)
{
  //REV: static cast..
  av_file_receiver* myrecvr = (av_file_receiver*)opaque;
  
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


bool av_file_receiver::init( )
{
  //INIT NETWORK -- REV: It is not recommended, and will be deprecated. Only exists for old openssh and gnuTLS impls
  //REV: this should only happen once per whole program? Or per header etc.?
  //Actually, before THREAD is created? Note:
  //https://ffmpeg.org/doxygen/trunk/group__lavf__core.html#ga245f2875f80ce67ec3d1e0f54dacf2c4
  //avformat_network_init();

  
  fprintf(stdout, "AV FILE RECEIVER, in init, waiting for lock!\n");
  const std::lock_guard<std::mutex> lock(mu);
  
  fprintf(stdout, "AV FILE RECEIVER, Got lock! Will alloc!!\n");
  fmt_context = avformat_alloc_context();
  
  if( !fmt_context )
    {
      fprintf(stderr, "AV FILE Receiver INIT() -- fmt context not exist!?\n");
      return false;
    }
  fprintf(stdout, "AV FILE Receiver -- Setting callbacks: [%s]\n", myurl.c_str() );
  
  //if it's not initialized yet...what to do?
  fmt_context->interrupt_callback.opaque = (void*)this;
  fmt_context->interrupt_callback.callback = &fmtintcallback;
  
  
  
  fprintf(stdout, "AV FILE Receiver -- Opening stream: [%s]\n", myurl.c_str() );
    
  AVDictionary *opts = NULL;

  reset_streamtime(); //REV: Should either do here or after the call, not sure...
  int averr1 = avformat_open_input(&fmt_context, myurl.c_str(), NULL, &opts);
  if( averr1 != 0 )
    {
      fprintf(stderr, "ERROR opening input (AV FILE: [%s])\n", myurl.c_str() );
      std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(averr1).c_str());
      //stop();
      return false;
    }
  
  fprintf(stdout, "AV FILE Receiver -- Opened! Getting format...: [%s]\n", myurl.c_str() );
  
  //REV: not sure what this is for, just puts the data in every packet?
  //REV: won't that write over packet-specific data...? :(
  //av_format_inject_global_side_data(fmt_context);
  
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
  fprintf(stdout, "AV FILE Receiver -- Found stream info: [%s], dumping it...\n", myurl.c_str() );
  
  
  int zero_if_input = 0;
  av_dump_format(fmt_context, 0, myurl.c_str(), zero_if_input);
  
  fprintf(stdout, "Finished init for AV FILE receiver [%s]\n", myurl.c_str() );

  bool seeked = seek_first();
  if( !seeked ) { fprintf(stderr, "Failed to seek?\n"); }
  
  return true;
}
