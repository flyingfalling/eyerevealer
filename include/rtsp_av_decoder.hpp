#pragma once

#include <tagged_avpkt_ptr.hpp>
#include <av_receiver.hpp>

//REV: avframe->format will contain AVSampleFormat, i.e. type of audio encoding stored.
//https://www.ffmpeg.org/doxygen/trunk/structAVFrame.html#a1d0f65014a8d1bf78cec8cbed2304992
//All data must point to same buffer (single alloced), i.e. only free data[0] i guess? Or just data...? Nah it is data[NPTR]
//No need to encode it, just save raw with some container with the relative info?
// Main info is the *TIMING*, specifically what to do with dropped audio frames?

// Problem, timebase of audio/video may be same/different (both 90k?). If that is the case, e.g. 44.1 kHz, not evenly
//divisible by 90k, so it will have a weird sample rate that is num/den with num != 1. Or it will have 90/x where x!=1

// REV: can I not just safely output raw PCM with timestamps?
//I could output the raw stuff but I'd need to find the container...(i.e. I'd have to find SDP and RTCP? RMCP? packets which
// are consumed by ffmpeg innards and not given to me...they hold the info about the format!)



struct rtsp_av_decoder
{
private:
  const AVCodec* vid_codec=nullptr; //wtf const? constant POINTER, not constant vid codec...?
  AVCodecContext* vid_codec_context=nullptr;
  
  const AVCodec* aud_codec=nullptr;
  AVCodecContext* aud_codec_context=nullptr;
  
  //AVFormatContext* format_context;
  
  SwsContext *swsContext=nullptr;
  
  int vid_stream_id=-1;
  int aud_stream_id=-1;
  
  int64_t pts_timebase_hz_sec=-1;
  int64_t aud_pts_timebase_hz_sec=-1;
  
  int64_t aud_rate_hz_sec=-1;
  int32_t aud_bytes_per_sample=-1;
  
  double video_fps=-1;
  
  std::mutex mu;

  //REV: is it negative? Wtf?
  std::int64_t first_frame_pts=0;
  std::int64_t last_frame_pts=0;
  std::uint64_t nframes=0;
  
  std::int64_t first_aud_frame_pts=0;
  std::int64_t last_aud_frame_pts=0;
  std::uint64_t naudframes=0;
  
  std::queue<AVFrame*> vid_frame_q;
  std::queue<AVFrame*> aud_frame_q;
  
  AVFrame matavframe;
  cv::Mat matwrapav;

  std::shared_ptr<av_receiver<tagged_avpkt_ptr>> receiver_ptr;
  bool decoding_initialized=false;

  size_t nallocvframes=0;
  size_t nallocaframes=0;
  size_t nfreevframes=0;
  size_t nfreeaframes=0;
  
public:
  //https://stackoverflow.com/questions/25402590/reading-rtsp-stream-with-ffmpeg-library-how-to-use-avcodec-open2
  //See mpegts_parser -- it runs same stuff?
  //REV: This is needed for parsing a raw data stream into AVPacket, which I can then send to decoder. In my case, I get the
  //     parsed stuff for free ;)
  // Becuase we use formatcontext, inputcontext, etc.
  rtsp_av_decoder(std::shared_ptr<av_receiver<tagged_avpkt_ptr>> _receiver_ptr)
    : receiver_ptr(_receiver_ptr)
  {
    
  }
  
  //REV: recall that it totally deletes the format RTSP shit? Does this shit stay around? Apparently?

  //REV todo figure out decoding issues...mutex locks, setting init or not.

  bool get_info( int& sr, int& bps, int& nc, bool& isplanar, bool& issigned, bool& isfloating )
  {
    if( !decoding_initialized ) { return false; }
    
    return info_from_codec_ctx( aud_codec_context, sr, bps, nc, isplanar, issigned, isfloating );
  }
  
  bool init_decoding( const bool withvideo=true, const int vid_str=-1, const bool withaudio=false, const int aud_str=-1 )// AVFormatContext*& format_context )
  {
    if( decoding_initialized )
      {
	fprintf(stderr, "\n\nREV: attempting to re-initialize decoding even though it is already initialized? This should actually never happen...fuck\n\n\n");
	//receiver_ptr->set_codecs_initialized();
	return true;
      }
    fprintf(stdout, "In INIT_DECODING rtsp_av_decoder.hpp\n");
    
    const std::lock_guard<std::mutex> lock(receiver_ptr->mu);
    vid_codec_context = avcodec_alloc_context3(NULL);
    aud_codec_context = avcodec_alloc_context3(NULL);
    //REV: I don't need a parser??!?
    //AVCodecParserContext *parser;
    //And why can I alloc null?
    
    auto format_context = receiver_ptr->get_stream_format_info();
    
    if( !format_context )
      {
	fprintf(stderr, "RTSP AV DECODER -- fmt context is null or something? Failing!\n");
	return false;
      }
    
    //VID
    if(withvideo)
    {
      
      if( vid_str < 0 )
	{
	  fprintf(stdout, "Finding best Vid stream...\n");
	  vid_stream_id = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO,
					      -1, -1, NULL, 0);
	  fprintf(stdout, "Found best video stream [%d]\n", vid_stream_id );
	}
      else
	{
	  vid_stream_id = vid_str;
	  if( (uint32_t)vid_stream_id >= format_context->nb_streams )
	    {
	      fprintf(stderr, "Specified video stream index [%d] is outside streams of this fmt context (%u)\n", vid_stream_id, format_context->nb_streams);
	      return false;
	    }
	}
            
      if( vid_stream_id < 0 )
	{
	  fprintf(stderr, "REV: couldn't find video stream\n");
	  return false;
	}
    
      
      
      fprintf(stdout, "Trying to alloc video codec...\n");
      
      int ret = avcodec_parameters_to_context(vid_codec_context, format_context->streams[vid_stream_id]->codecpar);
      if( ret != 0 )
	{
	  std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(ret).c_str());
	}
      
      if( !vid_codec_context )
	{
	  fprintf(stderr, "REV: vid_codec_context is null?!\n");
	  return false;
	}
      
      if( !format_context->streams[vid_stream_id] )
	{
	  fprintf(stderr, "Can't get desired stream (vid stream id), #[%d]\n", vid_stream_id);
	  return false;
	}
      
      vid_codec_context->pkt_timebase = format_context->streams[vid_stream_id]->time_base;
      
      vid_codec = avcodec_find_decoder( vid_codec_context->codec_id );
      //REV: in this case, do I need to supply input on "surfaces", not in main memory avpacket?
      //vid_codec = avcodec_find_decoder_by_name( "h264_qsv" );
      //vid_codec = avcodec_find_decoder_by_name( "h264_cuvid" );
      if( !vid_codec )
	{
	  fprintf(stderr, "REV: vid_codec is null?!\n");
	  return false;
	}
      else
	{
	  //https://ffmpeg.org/doxygen/trunk/structAVCodec.html#ad3daa3e729850b573c139a83be8938ca
	  fprintf(stdout, "REV: Found video decoder! Name [%s]\n", vid_codec->name);
	}
      
      vid_codec_context->codec_id = vid_codec->id;
      
      fprintf(stdout, "Did params to context codec (VIDEO)...\n");
      
      if(ret<0)
	{
	  fprintf(stderr, "REV: error, couldnt get params to context video...\n");
	  std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(ret).c_str());
	  return false;
	}
      
      ret = avcodec_open2(vid_codec_context, vid_codec, NULL); //&opts);
      if( ret < 0 )
	{
	  fprintf(stderr, "Error, couldn't open video codec context!\n");
	  std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(ret).c_str());
	  return false;
	}
      
      //Get time base?!
      auto vidtb = format_context->streams[vid_stream_id]->time_base;
      fprintf(stdout, "Time base of video: [%d]/[%d]\n", vidtb.num, vidtb.den);
      if( vidtb.num != 1 )
	{
	  fprintf(stdout, "I don't know how to handle rational number timebases...\n");
	  return false;
	}
      
      
      pts_timebase_hz_sec = vidtb.den;
      
      //AVRational tb = is->video_st->time_base;
      AVRational frame_rate = av_guess_frame_rate(format_context, format_context->streams[vid_stream_id], NULL);
      fprintf(stdout, "REV: frame rate estimated by AV FFMPEG LIBRARY: [%d]/[%d]\n", frame_rate.num, frame_rate.den );
      
      fprintf(stdout, "Finished opening (video) codec\n");
    } //end VID


    if( withaudio )
    { //AUDIO

      if( aud_str < 0 )
	{
	  fprintf(stdout, "Finding best Aud stream...\n");
	  aud_stream_id = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO,
					      -1, -1, NULL, 0);
	  fprintf(stdout, "Found best audio stream [%d]\n", aud_stream_id );
	}
      else
	{
	  aud_stream_id = aud_str;
	  if( (uint32_t)aud_stream_id >= format_context->nb_streams )
	    {
	      fprintf(stderr, "Specified audio stream index [%d] is outside streams of this fmt context (%u)\n", aud_stream_id, format_context->nb_streams);
	      return false;
	    }
	}
            
      if( aud_stream_id < 0 )
	{
	  fprintf(stderr, "REV: couldn't find audio stream\n");
	  return false;
	}
    
      
      
      fprintf(stdout, "Trying to alloc audio codec...\n");
      
      int ret = avcodec_parameters_to_context(aud_codec_context, format_context->streams[aud_stream_id]->codecpar);
      if( ret != 0 )
	{
	  std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(ret).c_str());
	  return false;
	}
      
      if( !aud_codec_context )
	{
	  fprintf(stderr, "REV: aud_codec_context is null?!\n");
	  return false;
	}
      
      if( !format_context->streams[aud_stream_id] )
	{
	  fprintf(stderr, "Can't get desired stream (aud stream id), #[%d]\n", aud_stream_id);
	  return false;
	}

      //REV: why is this necessary?
      aud_codec_context->pkt_timebase = format_context->streams[aud_stream_id]->time_base;

      fprintf(stdout, "REV: AUDIO packet timebase: [%d]/[%d]\n", aud_codec_context->pkt_timebase.num, aud_codec_context->pkt_timebase.den );
      
      aud_codec = avcodec_find_decoder( aud_codec_context->codec_id );
      if( !aud_codec )
	{
	  fprintf(stderr, "REV: aud_codec is null?!\n");
	  return false;
	}
      
      aud_codec_context->codec_id = aud_codec->id;
      
      fprintf(stdout, "Did params to context codec (AUDIO)...\n");
      
      if(ret<0)
	{
	  fprintf(stderr, "REV: error, couldnt get params to context audio...\n");
	  std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(ret).c_str());
	  return false;
	}
      
      ret = avcodec_open2(aud_codec_context, aud_codec, NULL); //&opts);
      if( ret < 0 )
	{
	  fprintf(stderr, "Error, couldn't open audio codec context!\n");
	  std::fprintf(stderr, "REV: AVERR: [%s]\n", av_error_str(ret).c_str());
	  return false;
	}
      
      //Get time base?!
      auto audtb = format_context->streams[aud_stream_id]->time_base;
      fprintf(stdout, "Time base of audio: [%d]/[%d]\n", audtb.num, audtb.den);
      if( audtb.num != 1 )
	{
	  fprintf(stdout, "I don't know how to handle rational number timebases (AUDIO)...but will try anyways?\n");
	  //return false;
	}

      aud_pts_timebase_hz_sec = audtb.den;
      
      aud_rate_hz_sec = aud_codec_context->sample_rate;
      aud_bytes_per_sample = av_get_bytes_per_sample(aud_codec_context->sample_fmt);
      const char* fmtname = av_get_sample_fmt_name( aud_codec_context->sample_fmt );
      bool isplanar = (1==av_sample_fmt_is_planar( aud_codec_context->sample_fmt ));
      bool issigned = (1==av_sample_fmt_is_signed( aud_codec_context->sample_fmt ));
      //how to know how many planes? Test data[0], data[1], etc.
      
      bool isinterleaved = !isplanar;
      //S16, S16P, etc, etc. How can I check if it is signed...?
      //I.e. little/big endian? :(
      //Endian internally is always NATIVE ORDER (codec will give me appropriate representation).
      //REV: generally planar 16-bit signed...
      fprintf(stdout, "REV String name sample format: [%s]\n", fmtname );

      //REV: I don't want to write out raw native organization (i.e. it may be big/little endian etc...)
      //REV: On the other hand, my representation of U16 inside YUV420 assumes same native endianness? Since I effectively
      //     just write the raw bytes...fuck. Should I "note" what the endianness is of the writing machine? Assume it will have
      //     already been converted from the depth camera?

      // REV: I need to "push" sound constantly to output otherwise it will "skip" (what makes those psychopy noises?)
      // REV: I can write out samples to file, and then "mark" each chunk of X msec as having a specific "true" video timing?
      // REV: that way I can detect "skips"? (and combine them later?)
      // REV: note, how much is included in a sample? I.e. "length" size is fixed...?

      //nb_samples is number of audio SAMPLES (per channel!) in this frame

      // frame->colorspace etc.? Wow..
      // frame->ch_layout for layout! AVChannelLayout and duration (in pts timebase)
      // ch_layout.nb_channels and ch_layout.order is enum AVChannelOrder
      
      //av_channel_layout_describe() and then build it using av_channel_layout_from_string() to get same...
    } //end AUDIO
    
    
    decoding_initialized = true;
    return true;
  }

  ~rtsp_av_decoder( )
  {
    while( vid_frame_q.size() > 0 )
      {
	auto avframe = vid_frame_q.front();
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	++nfreevframes;
	vid_frame_q.pop();
      }

    while( aud_frame_q.size() > 0 )
      {
	auto avframe = aud_frame_q.front();
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	++nfreeaframes;
	aud_frame_q.pop();
      }

    if(swsContext)
      {
	sws_freeContext(swsContext);
      }
    
    
    //free_codec_context();
    uninit_decoding();
  } //end destructor
  
  void uninit_decoding()
  {
    const std::lock_guard<std::mutex> lock(receiver_ptr->mu);
    
    //free stuff that I allocated, i.e. codec etc., but not the format context...
    if( vid_codec_context )
      {
	avcodec_close( vid_codec_context );
	avcodec_free_context( &vid_codec_context );
	fprintf(stdout, "Freed vid ctx\n");
      }

    if( aud_codec_context )
      {
	avcodec_close( aud_codec_context );
	avcodec_free_context( &aud_codec_context );
	fprintf(stdout, "Freed aud ctx\n");
      }

    
    decoding_initialized = false;
  }


  //Format context may stay legal, but I may dealloc and close input stream? Fuck? They should return errors then (noting that it is no longer looping). But I am getting segfault from...? I should cleanly deallocate/close that input over there and over here,
  //and not allow decoding to happen... This video_codec_context etc. should be detected as no longer valid...
  
  //basically, yields frames for you. Contains the actual codecs and shit
  //for each stream? (if it can?).
  //REV: fills up the queues (i.e. doesnt return).
  //Note, I do not own this, it is freed after the call to this by the caller (usually).
  //void add_pkt_and_decode( AVPacket*& avpkt )

  //REV: it seems that for single send and multiple receive (e.g. audio...), only the first frame has PTS set, and the others have
  // NOPTS? Fuck? Do I have to manually calculate it myself? Seems that way...
  
  bool add_pkt_and_decode( tagged_avpkt_ptr& tavpkt, const bool use_custom_ts=false )
  {
    if( !vid_codec_context )
      {
	fprintf(stderr, "VIDEO is not initialized?\n");
	return false;
      }
    if( !aud_codec_context )
      {
	fprintf(stderr, "AUDIO is not initialized?\n");
	return false;
      }

    if( !decoding_initialized )
      {
	fprintf(stderr, "DECODING INITIALIZED is not initialized?\n");
	return false;
      }
	
    
    auto avpkt = tavpkt.pkt;
    auto ts = tavpkt.ts;
    
    //adds packet, then decodes until done.
    if( vid_stream_id == avpkt->stream_index )
      {
	int sent;
	sent = avcodec_send_packet( vid_codec_context, avpkt );
		
	//ownership of mypkt remains with me, and not written to. May create reference to it (or copy if not ref counted)
	if( 0 == sent )
	  {
	    //avframe is 1) av_frame_unref() and then set pointer to point to a frame **allocated by decoder**.
	    //Note, ref-counted. Am I guaranteed that the decoder will not write over it?
	    int recv;
	    
	    //fprintf(stdout, "VID Packet pts: [%ld] (duration: %ld) (DTS: %ld)\n", avpkt->pts, avpkt->duration, avpkt->dts);

	    //frame index?
	    int64_t idx_in_pkt=0;
	    
	    //REV: aud frame PTS is only set for the first frame I receive? Fuck?
	    do
	      {
		bool pushed=false;
		AVFrame* avframe = av_frame_alloc(); //I should dealloc it if I don't push it somewhere?
		++nallocvframes;
		recv = avcodec_receive_frame( vid_codec_context, avframe ); //0 on success
		
		//while (ret != AVERROR(EAGAIN)); Should receive while I shouln't do again then send while I shouldn't do again?
		//Note, in ffplay, the decoder object has d->pkt...and that's what it uses (and maybe needs to packet_pending).
		
		if( recv >= 0 )
		  {
		    if( idx_in_pkt > 0 )
		      {
			fprintf(stderr, "REV: WEIRD -- VIDEO FRAME -- GOT MULTIPLE FRAMES IN PAKT\n");
		      }
		    
		    idx_in_pkt ++;
		    
		    //REV: hm, does avframe have other ?
		    if( use_custom_ts )
		      {
			//REV: overwrite DTS (note, I do not use it anymore and i should own this thing i.e. no longer used
			// by decoder anymore so it shouldn't matter if I overwrite it...right?
			//REV: note they use AV_TIME_BASE (=1M, i.e. 1 microsec) and AV_TIME_BASE_Q which is 1/AV_TIME_BASE.
			avframe->pkt_dts = ts;
		      }

		    //REV: AVFRAME->DURATION is added after 5.1.2...!
		    
		    { // REV: START MUTEX LOCK
		      const std::lock_guard<std::mutex> lock(mu);
		      //fprintf(stdout, "VID PTS: [%ld] (PKT DTS: %ld)\n", avframe->pts, avframe->pkt_dts);
		      if( avframe->pts != AV_NOPTS_VALUE )
			{
			  if( 0==nframes )
			    {
			      int decode_error_flags = avframe->decode_error_flags;
			      if( 0 == decode_error_flags )
				{
				  //Don't let first frame have decode errors? I.e. wait for a clean frame.
				  //pushed=true;
				  //vid_frame_q.push( avframe );
				  first_frame_pts = avframe->pts;
				  last_frame_pts = avframe->pts;
				  ++nframes; //nframes refers to number of decoded, not pushed!
				  fprintf(stdout, "FIRST FRAME PTS -- [%ld]\n", first_frame_pts );
				}
			      else
				{
				  fprintf(stdout, "REV: FFMPEG VIDEO -- DETECTED DECODE FLAGS (first frame...)\n");
				}
			      //REV: will PTS ever reset??!
			    } //if zeroth frame.
			  else
			    {
			      int decode_error_flags = avframe->decode_error_flags;
			      if( 0 != decode_error_flags )
				{
				  fprintf(stderr, "REV: FFMPEG VIDEO -- DETECTED DECODE FLAGS (later frame)\n");
				}
			  
			      //REV: fixing first/last frame errors
			      // (beginning of stream can have weird PTS? Sender forgot to flush?)
			      if( avframe->pts <= last_frame_pts )
				{
				  int64_t offset = last_frame_pts - avframe->pts;
				  double offset_as_sec = offset / (double)pts_timebase_hz_sec;

				  //If the stream jumps "backwards" in time more than 1 second, restart it...no b frame that crazy?
				  const double STREAM_STARTBUG_THRESH_SEC = 1.0;
				  if( offset_as_sec > STREAM_STARTBUG_THRESH_SEC )
				    {
				      fprintf(stderr, "REV: warning/ERROR frame PTS < previous frame's PTS?! [%ld] -> [%ld] (%lf) sec\n",
					      last_frame_pts, avframe->pts, offset_as_sec );
				      nframes = 0;
				    }
				  else
				    {
				      pushed=true;
				      vid_frame_q.push( avframe );
				      last_frame_pts = avframe->pts;
				      ++nframes;
				    }
				}
			      else
				{
				  pushed=true;
				  vid_frame_q.push( avframe );
				  last_frame_pts = avframe->pts;
				  ++nframes;
				}
			  		      
			      //_estimate_set_fps(); //internal
			    }
			} //end if PTS value is NOPTS
		      else
			{
			  fprintf(stderr, "REV: VID AVFRAME PTS value is unset, skipping!\n");
			}
		      
		    } //END MUTEX LOCK
		
		    //Note, it's just a pointer to it? How do they know when I no longer use it? unref()
		    //Checked source, it is not ref() if error condition, only on success.
		    //http://ffmpeg.org/doxygen/3.4/decode_8c_source.html
		    //av_frame_unref( avframe ); //tell them I no longer reference it. Does it set if it fails?
		  } //end recv>=0
		else
		  {
		    
		    if( false == pushed)
		      {
			av_frame_unref(avframe);
			av_frame_free(&avframe);
			++nfreevframes;
		      }
		    if( recv != AVERROR(EAGAIN) )
		      {
			std::fprintf(stderr, "VIDEO RECV FRAME (from decoder) FAILED (ret!=0)\n -- REV: AVERR: [%s]\n", av_error_str(recv).c_str());
			//REV: turn me off!
			return false;
		      }
		    //handle errors (receive_frame)
		  }
		
		//Either way, unref the frame?! Even if it failed?
	    
	    
		//recv == AVERROR(EAGAIN) means it needs more data to get a frame...but it will fill up automatically.
		// AVERROR(EOF) means end of file? So just exit.
		//assert(recv != AVERROR(EINVAL));
		//if( AVERROR_EOF == state ) { }
		//assert(recv >= 0);
		//Frame f(std::move(frame), *pkt.get());
	      } while (recv >= 0 );
	  }
	else
	  {
	    std::fprintf(stderr, "VIDEO SEND PACKET (to decoder) FAILED (ret!=0)\n -- REV: AVERR: [%s]\n", av_error_str(sent).c_str());
	    //dealloc the packet? No, he will handle that..
	    return false;
	  }
      }
    else if( aud_stream_id == avpkt->stream_index )
      {
	//REV: do audio!!! (save it in packets? Note PTS etc. needs to be all lined up?)
	//How is audio represented? Need to buffer it to the speakers via e.g. SDL2...
	// Just buffer it as I get it in time? Does this avpkt have a PTS?
	//fprintf(stdout, "START aud frame\n");
	int sent;
	sent = avcodec_send_packet( aud_codec_context, avpkt ); //SEND the mypacket to the decoder
	//ownership of mypkt remains with me, and not written to. May create reference to it (or copy if not ref counted)
	
	if( 0 == sent )
	  {
	    //avframe is 1) av_frame_unref() and then set pointer to point to a frame **allocated by decoder**.
	    //Note, ref-counted. Am I guaranteed that the decoder will not write over it?
	    int recv;

	    //https://github.com/FFmpeg/FFmpeg/blob/7d377558a6/fftools/ffmpeg.c#L2349
	    //REV: see line  in fftools, need special case where multiple frames in packet, have to calculate shit myself? :(
	    //REV: note, just use DTS
	    //fprintf(stdout, "AUD Packet pts: [%ld] (Duration: %ld) (DTS: %ld) (TB: %d/%d)\n", avpkt->pts, avpkt->duration, avpkt->dts , avpkt->time_base.num, avpkt->time_base.den);
	    
	    //REV: duration is fucked up, only half of what it should be (unless something in 45000 or something?)
	    //Ignore it? But they use it
	    //http://dranger.com/ffmpeg/tutorial05_print.html
	    int64_t frameidx_in_pkt=0;
	    int64_t samples_offset=0;
	    
	    do
	      {
		bool pushed=false;
		AVFrame* avframe = av_frame_alloc(); //I should dealloc it if I don't push it somewhere?
		++nallocaframes;
		recv = avcodec_receive_frame( aud_codec_context, avframe ); //0 on success

		//REV: I will do the scaling!!
		
		if( recv >= 0 )
		  {
		    
		    {
		      const std::lock_guard<std::mutex> lock(mu);

		      int64_t frame_duration_pts = (aud_pts_timebase_hz_sec * avframe->nb_samples) / avframe->sample_rate;
		      //fprintf(stdout, "Audio frame [%10lu] samples PTS [%ld], SEC [%lf]\n", avframe->pts, frame_duration_pts, (double)avframe->nb_samples/avframe->sample_rate);
		      //Only set for the first one (who has his own PTS)
		      if( avframe->pts != AV_NOPTS_VALUE )
			{
			  if( avframe->pts != avpkt->pts )
			    {
			      fprintf(stderr, "REV: wtf frame pts != pkt pts?! Audio...\n");
			      exit(1);
			    }
			  //Do nothing
			}
		      else
			{
			  //REV: sample rate is # samples / sec. timebase_hz_sec is 90k/sec.
			  // 90k / 24000 = pts per sample. and I have n samples, so mult by samples. OK.
			  
			  
			  int64_t offset_pts = (aud_pts_timebase_hz_sec * samples_offset) / avframe->sample_rate;
			  //e.g. 90k * (576/24000) = 2160,
			  //fprintf(stdout, "Activating offset! PTS [%lu] + [%lu]\n",  avpkt->pts, offset_pts );
			  avframe->pts = avpkt->pts + offset_pts;
			  
			}

		      samples_offset += avframe->nb_samples;
		      frameidx_in_pkt ++;
		      
		      if( avframe->pts != AV_NOPTS_VALUE )
			{
			  //REV: correct my PTS value...
			  //REV: note, this will not be exact since
			  
			  //fprintf(stdout, "AUD PTS: [%ld]  (%ld samples @ %ld) (PKT DTS: %ld)\n", avframe->pts, avframe->nb_samples, avframe->sample_rate, avframe->pkt_dts);
			  if( 0==naudframes )
			    {
			      int decode_error_flags = avframe->decode_error_flags;
			      //fprintf(stdout, "FIRST (DECODED) AUD FRAME!\n");
			      if( 0 == decode_error_flags )
				{
				  //fprintf(stdout, "FIRST (GOOD) AUD FRAME!\n");
				  //Don't let first frame have decode errors?
				  //pushed=true;
				  //aud_frame_q.push( avframe );
				  first_aud_frame_pts = avframe->pts;
				  last_aud_frame_pts = avframe->pts;
				  ++naudframes;
				  fprintf(stdout, "FIRST AUD FRAME PTS -- [%ld] (%ld)\n", first_aud_frame_pts, avframe->pts );
				}
			      else
				{
				  fprintf(stdout, "REV: FFMPEG AUDIO -- DETECTED DECODE FLAGS\n");
				}
			      //REV: will PTS ever reset??!
			    }
			  else
			    {
			      int decode_error_flags = avframe->decode_error_flags;
			      if( 0 != decode_error_flags )
				{
				  fprintf(stderr, "REV: FFMPEG AUDIO -- DETECTED DECODE FLAGS (later frame)\n");
				}
			      if( avframe->pts <= last_aud_frame_pts )
				{
				  //REV: fuck audio timebase is 1/24000, video is 1/90000.
				  //auto tb = aud_codec_context->time_base;
				  //REV: is TB really different?
				  /*if( tb.den != pts_timebase_hz_sec )
				    {
				    fprintf(stderr, "(AUDIO) Numerator timebase != pts [%d]  vs [%ld]\n", tb.den, pts_timebase_hz_sec );
				    }*/
				  int64_t offset = last_aud_frame_pts - avframe->pts;
				  double offset_as_sec = offset / (double)pts_timebase_hz_sec;
				  
				  //If the stream jumps "backwards" in time more than 1 second, restart it...no b frame that crazy?
				  const double STREAM_STARTBUG_THRESH_SEC = 1.0;
				  if( offset_as_sec > STREAM_STARTBUG_THRESH_SEC )
				    {
				      fprintf(stderr, "REV: AUDIO!! warning/ERROR frame PTS < previous frame's PTS?! [%ld] -> [%ld] (%lf) sec  (note last video PTS was: [%ld])\n", last_aud_frame_pts, avframe->pts, offset_as_sec, last_frame_pts );
				      naudframes = 0;
				    }
				  else
				    {
				      pushed=true;
				      aud_frame_q.push( avframe );
				      last_aud_frame_pts = avframe->pts;
				      ++naudframes;
				    }
				}
			      else
				{
				  pushed=true;
				  aud_frame_q.push( avframe );
				  last_aud_frame_pts = avframe->pts;
				  ++naudframes;
				}
			    }
			}
		      else
			{
			  fprintf(stderr, "REV: AUD AVFRAME PTS value is unset, skipping!\n");
			}
		    } //end MUTEX block
		  } //end if recv>=0
		else
		  {
		    
		    if( false == pushed )
		      {
			av_frame_unref(avframe);
			av_frame_free(&avframe);
			++nfreeaframes;
		      }
		    if( recv != AVERROR(EAGAIN) )
		      {
			std::fprintf(stderr, "AUDIO RECV FRAME (from decoder) FAILED (ret!=0)\n -- REV: AVERR: [%s]\n", av_error_str(recv).c_str());
			return false;
		      }
		    //handle errors (receive_frame)
		  }
		//Either way, unref the frame?! Even if it failed?
	      }	  while(recv >= 0);
	  }//end if 0 == sent
	else
	  {
	    std::fprintf(stderr, "AUDIO SEND PACKET (to decoder) FAILED (ret!=0)\n -- REV: AVERR: [%s]\n", av_error_str(sent).c_str());
	    return false;
	  }
      } //end IF AUDIO STREAM
    else
      {
	fprintf(stderr, "Packet from unknown stream (SKIPPING)?!\n");
	
	//fprintf(stderr, "Packet is from unknown stream (not audio or video?!\n");
      }

    //fprintf(stdout, "Freed/Alloced Vid: [%lu/%lu]   Aud: [%lu/%lu]\n", nfreevframes, nallocvframes, nfreeaframes, nallocaframes);
    
    //packet is not owned by me, so leave it be.
    return true;
  } //end add_avpkt_and_decode();


  //REV: make a copier/rescaler shit?
  void matBGR24_copy_from_avframe( const AVFrame* inputframe, int dstX, int dstY, cv::Mat& result )
  {
    if( nullptr == swsContext )
      {
	init_converter( inputframe, dstX, dstY );
      }
    
    //REV: this will fill matavframe from inputframe
    sws_scale(swsContext, inputframe->data, inputframe->linesize, 0, inputframe->height, matavframe.data, matavframe.linesize);

    //REV: I am copying, so it should create new data!
    matwrapav.copyTo(result); //copy it over...in case line-size issues etc.? It's fine since I set to 1. But copy anyways...
    return;
    }

  // probably a vector of uint16 PCM data?
  // Depends on the type of data stored in the thing (and the time base etc.)
  // So, I need to know all that shit to properly process it.

  //REV: get some struct that describes it in my terms?
  //sample rate, bits per sample, format, etc.
  //Note how about time-base etc?
  //REV: should I let it depend on "type"? I.e.  user may cast as per-sample etc...
  //REV: how do I know endianness etc.? Will it be in the AVFrame format?
  //REV: get other info from the decoder/codec itself (i.e. from?)
  //REV: or we can just do everything in real time. We may want to do speech rec here though? Lol.
  //REV: get format from aud_codec-> and aud_codec_context->
  //REV: note AVSampleFormat will contain size/type of it, e.g. 16 bit signed planar, etc.
  //ffmpeg -formats
  bool process_pop_front_aud( std::vector<std::vector<std::byte>>& vec, std::uint64_t& pts  )
  {
    AVFrame* avframe=nullptr;
    if( aud_frame_q.empty() )
      {
	return false;
      }
    else
      {
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu); //only lock to make sure I don't mess with queue order. Once i ahve pointer, ok.
	  avframe = aud_frame_q.front();
	  
	  if( aud_bytes_per_sample > 0 )
	    {
	      //REV: this won't work, since it will depend on CHANNELS as well? I.e. nb_samples is samples PER CHANNEL.
#ifdef FFMPEG_VERSION_GEQ_5 //( "5.0" )
	      auto chlayout = avframe->ch_layout;
	      int nchannels = chlayout.nb_channels;
	      AVChannelOrder chorder = chlayout.order;
	      if( chorder == AV_CHANNEL_ORDER_NATIVE || chorder == AV_CHANNEL_ORDER_UNSPEC )
		{
		  //I think this is OK? Native or unspecified...
		}
	      else
		{
		  fprintf(stderr, "Channel order is...unexpected. E.g. CUSTOM or AMBISONIC. I can't handle this.\n");
		}
	      const size_t bufsize=2000;
	      char BUF[bufsize];
	      int bytesreq = av_channel_layout_describe(&chlayout, &(BUF[0]), bufsize);
	      if( bytesreq < 0 ) { fprintf(stderr, "Wtf audio chan layout desc failed\n"); }
	      
	      //fprintf(stdout, "AUDIO: NCH: [%d]  Order [%d] Desc: [%s]\n", nchannels, chorder, BUF );
	      if( nchannels <= 0 )
		{
		  fprintf(stderr, "REV: wtf, <=0 audio channels?! [%d]\n", nchannels);
		}
#if DEBUG_LEVEL > 5
	      if(  nchannels > 1 )
		{
		  fprintf(stderr, "REV: number audio channels > 1 -- NOT 100pct confident about non-mono yet\n");
		}
#endif

#else //REV: RV_AV_VERSION
	      int nchannels = av_get_channel_layout_nb_channels( avframe->channel_layout );
#endif

	      //REV: note that data[0], data[1], data[2] will point to the channel data!
	      size_t nbytes = aud_bytes_per_sample * avframe->nb_samples; //REV: check frame class to make sure it is audio frame...
	      vec.resize( nchannels );
	      for( int c=0; c<nchannels; ++c)
		{
		  vec[c] = std::vector<std::byte>( (std::byte*)avframe->data[c], (std::byte*)(avframe->data[c]+nbytes) );
		}
	      pts = avframe->pts;
	    }
	  else
	    {
	      fprintf(stderr, "Can't handle audio unknown data frame size...\n");
	      exit(1);
	    }
	  
	  aud_frame_q.pop();
	}
      }
    
    //Do something with it if we had one.
    if( nullptr != avframe )
      {
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	++nfreeaframes;
	return true;
      }

    return false;
  }

  bool pop_front_vid( AVFrame*& avframe )
  {
    if( vid_frame_q.empty() )
      {
	return false;
      }
    else
      {
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu); //only lock to make sure I don't mess with queue order. Once i ahve pointer, ok.
	  avframe = vid_frame_q.front();
	  vid_frame_q.pop();
	}
      }

     if( nullptr != avframe )
      {
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	++nfreevframes;
	return true;
      }
    
    return true;
  }

  bool pop_front_aud( AVFrame*& avframe )
  {
    if( aud_frame_q.empty() )
      {
	return false;
      }
    else
      {
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu); //only lock to make sure I don't mess with queue order. Once i ahve pointer, ok.
	  avframe = aud_frame_q.front();
	  aud_frame_q.pop();
	}
      }

     if( nullptr != avframe )
      {
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	++nfreeaframes;
	return true;
      }
    
    return true;
  }
  
  bool process_pop_front_vid( cv::Mat& mat, std::uint64_t& pts, const bool usecustomts=false )
  {
    AVFrame* avframe=nullptr;

    if( vid_frame_q.empty() )
      {
	return false;
      }
    else
      {
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu); //only lock to make sure I don't mess with queue order. Once i ahve pointer, ok.
	  avframe = vid_frame_q.front();
	  vid_frame_q.pop();
	}
	//UNLOCK

	if( nullptr != avframe )
	  {
	    matBGR24_copy_from_avframe( avframe, 0, 0, mat );
	    pts = avframe->pts; //presentation time stamp
	    
	    std::uint64_t pkt_dts = avframe->pkt_dts; //time stamp of packet DTS...
	    if( usecustomts )
	      {
		pts = pkt_dts;
	      }
	    
	    std::uint64_t timestamp = avframe->best_effort_timestamp;
	    std::uint64_t pkt_duration = avframe->pkt_duration; //duration of packet in timebase
	    int decode_error_flags = avframe->decode_error_flags;
	    
	    if( decode_error_flags )
	      {
		//REV: can conceal error flags in AVCodecContext->error_concealment (1, 2, 256?)
		//#define FF_EC_GUESS_MVS   1
		//#define FF_EC_DEBLOCK     2
		//#define FF_EC_FAVOR_INTER 256

	
		//#define FF_DECODE_ERROR_INVALID_BITSTREAM   1
		//#define FF_DECODE_ERROR_MISSING_REFERENCE   2
		// #define FF_DECODE_ERROR_CONCEALMENT_ACTIVE  4
		// #define FF_DECODE_ERROR_DECODE_SLICES       8
		fprintf( stderr, "REV: detected decode error flags! Value: [%d]\n", decode_error_flags );
	      }
	    
	    av_frame_unref( avframe ); //REV: this will double un-ref it!
	    av_frame_free( &avframe );
	    ++nfreevframes;
	    return true;
	  }
      }
    return false;
  }

  //REV will never init!!!?!
  int64_t get_timebase_hz_sec( double timeoutsec = 20.0 )
  {
    std::unique_lock<std::mutex> lock(mu);
#if DEBUG_LEVEL > 100
    fprintf(stdout, "Attempting to get timebase of av decoder...\n");
#endif
    
    Timer t;
    while( pts_timebase_hz_sec < 0 && t.elapsed() < timeoutsec)
      {
	lock.unlock();
	std::this_thread::sleep_for( std::chrono::milliseconds(50) );
	lock.lock();
      }
    lock.unlock();
    if( t.elapsed() >= timeoutsec )
      {
	fprintf(stderr, "REV: BIG ERROR -- get_timebase_hz_sec TIMED OUT (maybe never init?!)\n");
      }
    //fprintf(stdout, "DONE Attempting to get timebase of av decoder...\n");
    return pts_timebase_hz_sec; //assume it is set? :(
  }
  
  double get_fps()
  {
    const std::lock_guard<std::mutex> lock(mu);
    _estimate_set_fps();
    return video_fps;
  }

  void init_converter( const AVFrame* inputframe, int dstX, int dstY )
  {
    
    int srcX = inputframe->width;
    int srcY = inputframe->height;
    fprintf(stdout, "REV: Initial decoded BGR image size is %d %d\n", srcX, srcY );
    if( srcX < 1 || srcY < 1 )
      {
	fprintf(stderr, "REV: error init BGR convertor, srcX or srcY is less than or equal to zero\n");
	exit(1);
      }
    
    if( 0 == dstY && 0 == dstX )
      {
	fprintf(stdout, "dstX/dstY are both 0, so will copy image to same size (%d %d)!\n", srcX, srcY);
	dstX = srcX;
	dstY = srcY;
      }
    else if( 0 == dstY )
      {
	fprintf(stdout, "dstY (requested video frame size height) is 0, so Rescaling based on decoded frame dimensions (ratio) and requested width\n");
	double scaling = (double)dstX / srcX; //e.g. 640 / 1280 = 0.5
	dstY = (srcY * scaling); //+1? To account for errorful integer?
      }
    else if( 0 == dstX )
      {
	fprintf(stdout, "dstX (requested video frame size width) is 0, so Rescaling based on decoded frame dimensions (ratio) and requested height\n");
	double scaling = (double)dstY / srcY; //e.g. 640 / 1280 = 0.5
	dstX = (srcX * scaling); //+1? To account for errorful integer?
      }
    else
      {
	//fprintf(stdout, "Will copy image to new size (%d %d) -> (%d %d)!\n", srcX, srcY, dstX, dstY);
      }

    fprintf(stdout, "INITIALIZING: Setting SWS scaling context to go from original w: %d h: %d to new w: %d h: %d\n", srcX, srcY, dstX, dstY);

    enum AVPixelFormat src_pixfmt = (enum AVPixelFormat)inputframe->format;
    //AV_PIX_FMT_YUV420P

    if( swsContext )
      {
	sws_freeContext( swsContext );
      }

    fprintf(stdout, "REV: pixel format of incoming frames is: [%s] Size: (W: %ld H: %ld)\n", av_get_pix_fmt_name( src_pixfmt ), srcX, srcY );
    
    swsContext = sws_getContext(srcX, srcY, src_pixfmt, dstX, dstY, AV_PIX_FMT_BGR24, SWS_AREA, NULL, NULL, NULL);

    matwrapav = cv::Mat::zeros( dstY, dstX, CV_8UC3 );
    
    //int linesize_align=32; //I happen to know AV uses 32 bit alignment...? Can I check this from the source codec (frames?)
    int linesize_align;
    if( matwrapav.isContinuous() )
      {
	linesize_align=1; //But my contiguous OPENCV has line size alignment of 1...I think?
      }
    else
      {
	fprintf(stdout, "REV: CV matrix not contig?\n");
	exit(1);
      }

    //This allocates a frame STRUCT, not the actual data (I will use cv::Mat memory chunk).
    //I just want it to fill in all the AVFrame metadata appropriately.

    int ret = av_image_alloc( matavframe.data, matavframe.linesize,
			      dstX, dstY, AV_PIX_FMT_BGR24, linesize_align );
    
    if( !ret )
      {
	fprintf(stderr, "Couldnt alloc new image for BGR conversion\n");
	exit(1);
      }

    //This may cause issues at destruction.
    //"The allocated image buffer must be freed by calling av_freep(&pointers[0])"
    //REV: delete it now, I won't use it. I will use CV instead.
    //av_freep(&(matavframe.data[0]));
    //av_free(matavframe.data[0]);
    av_freep(&(matavframe.data[0]));
    
    //Set the AVFrame's data to actually point to the cv::Mat memory chunk (.data())
    matavframe.data[0] = (std::uint8_t*)(matwrapav.data);
    
    //REV: not needed -- redundant?
    //REV: these fields were set with av_image_alloc -- I could have just used av_image_fill_arrays without av_image_alloc.
    //     It would fill in the required settings without actually allocating ->data[0] etc.
    //av_image_fill_arrays( avframeBGR->data, avframeBGR->linesize, (std::byte*)mat.data, AV_PIX_FMT_BGR24, avframeBGR->width, avframeBGR->height, linesize_align);

  } //end init_converter
  
  void _estimate_set_fps()
  {
    //Don't bother estimating if set e.g. in initialization?
    if( video_fps > 0 )
      {
	fprintf(stdout, "Estimating FPS -- already >0 (=%lf), returning\n", video_fps);
	return;
      }
#if DEBUG_LEVEL > 10
    fprintf(stdout, "ESTIMATING FPS! (RTSP AV DECODER)\n");
#endif
    
    double fps = -1.0;
    double min_sampletime_sec = 3.0;
    //uint64_t required_pts = min_sampletime_sec * pts_timebase_hz_sec;
    //const std::lock_guard<std::mutex> lock(mu);
    if( pts_timebase_hz_sec <= 0 )
      {
	fprintf(stderr, "Major error during estimating FPS, timebase is still not set! (%ld)\n", pts_timebase_hz_sec);
	video_fps = fps;
	return;
      }
    double elapsed_sec = (last_frame_pts - first_frame_pts)/(double)pts_timebase_hz_sec;
    if( (first_frame_pts < last_frame_pts) )
      {
	if( elapsed_sec > min_sampletime_sec )
	  {
	    double elapsed_as_seconds = (double)(last_frame_pts - first_frame_pts)/(double)pts_timebase_hz_sec;
	    fps = nframes / elapsed_as_seconds;
	  }
	else
	  {
	    fprintf(stderr, "REV: Insufficent time for vid FPS (Elapsed: [%lf] sec, Required: [%lf] sec (note [%ld] frames, PTS: [%lu]-[%lu]\n", elapsed_sec, min_sampletime_sec, nframes, first_frame_pts, last_frame_pts );
	  }
      }
    else
      {
	fprintf(stderr, "REV: error in PTS times for vid FPS estimation? (note [%ld] frames, PTS: [%lu]-[%lu]\n", nframes, first_frame_pts, last_frame_pts );
      }
    video_fps = fps;
    return;
  }
  
};


