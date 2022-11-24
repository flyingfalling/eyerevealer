#pragma once



//What if there are multiple streams?
struct mpegts_parser
{
  const AVCodec* vid_codec=nullptr;
  AVCodecContext* vid_codec_context=nullptr;
  
  const AVCodec* aud_codec=nullptr;
  AVCodecContext* aud_codec_context=nullptr;
  
  AVIOContext* input_output_context=nullptr;
  AVFormatContext* format_context=nullptr;
  
  SwsContext *swsContext=nullptr;
  
  int vid_stream_id=-1;
  int aud_stream_id=-1;
  
  //REV: can I read these from the decoded context?
  int64_t pts_timebase_hz_sec=-1;
  int64_t audio_rate_hz_sec=-1;
  int32_t audio_bytes_per_sample=-1;
  
  double video_fps=-1;
  
  std::mutex mu;
  
  std::uint64_t first_frame_pts=0;
  std::uint64_t last_frame_pts=0;
  std::uint64_t nframes=0;

  std::uint64_t first_aud_frame_pts=0;
  std::uint64_t last_aud_frame_pts=0;
  std::uint64_t naudframes=0;
  
  std::queue<AVFrame*> vid_frame_q;
  std::queue<AVFrame*> aud_frame_q;
  
  AVFrame matavframe;
  cv::Mat matwrapav;

  //std::mutex shutdown_mu;  
  
  ~mpegts_parser()
  {
    //const std::lock_guard<std::mutex> lock(shutdown_mu);
    
    while( vid_frame_q.size() > 0 )
      {
	auto avframe = vid_frame_q.front();
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	vid_frame_q.pop();
      }

    while( aud_frame_q.size() > 0 )
      {
	auto avframe = aud_frame_q.front();
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	aud_frame_q.pop();
      }
    
    sws_freeContext(swsContext);
    
    av_free(input_output_context->buffer); //REV: diff between AV_FREE and AV_FREEP
    fprintf(stdout, "Freed IO context buffer\n");

    //REV: do I need to free these separately or wil
    // avformat_free_context do it?
    avcodec_close( vid_codec_context );
    avcodec_free_context( &vid_codec_context );
    fprintf(stdout, "Freed vid ctx\n");

    avcodec_close( aud_codec_context );
    avcodec_free_context( &aud_codec_context );
    fprintf(stdout, "Freed aud ctx\n");
    
    avformat_close_input( &format_context );
    avformat_free_context( format_context );
    fprintf(stdout, "Freed format\n");
    
    av_free( input_output_context );
    fprintf(stdout, "Freed IO\n");
  }
  
  mpegts_parser( )
  {
  }

  double get_timebase_hz_sec( double timeoutsec = 2.0 )
  {
    std::unique_lock<std::mutex> lock(mu);
    Timer t;
    while( pts_timebase_hz_sec < 0 && t.elapsed()<timeoutsec)
      {
	lock.unlock();
	std::this_thread::sleep_for( std::chrono::milliseconds(50) );
	lock.lock();
      }
    lock.unlock();
    return pts_timebase_hz_sec; //assume it is set? :(
  }
  
  void init_decoding( mutexed_buffer<std::byte>& _mbuf, const size_t bufsize=100000 )
  {
    const std::lock_guard<std::mutex> lock(mu);
    
    //REV: can't use std::vector etc. container because avio will take control of it and will cause double free when
    // my container's auto destructor happens. I will free manually with av_free or something.
    std::byte* decoding_slurp_buffer = (std::byte*)av_malloc( bufsize + AV_INPUT_BUFFER_PADDING_SIZE );

    input_output_context =
      avio_alloc_context( reinterpret_cast<std::uint8_t*>(decoding_slurp_buffer),
			  bufsize,
			  0,                               // bWriteable (1=true,0=false) 
			  reinterpret_cast<void*>(&_mbuf),  // user data that will be passed to callback (opaque)
			  []( void* opaque, std::uint8_t* buf, int requestedbytes ) -> int
			  {
			    auto mymbufptr = reinterpret_cast<mutexed_buffer<std::byte>*>(opaque);
			    //fprintf(stdout, "In read func! Testing -- dsp->mbuf has [%ld]\n", dsp->mbuf->size() );
			    size_t popped = mymbufptr->popto( reinterpret_cast<std::byte*>(buf), requestedbytes ); //assume buf pointer has sufficient size!
			    return popped;
			  },
			  NULL,               // Write callback function (not needed)
			  NULL                // Seek callback funct (not needed?)
			  );



    fprintf(stdout, "Finished alloc IO context\n");
    
    if( nullptr == input_output_context )
      {
	fprintf(stderr, "Can't alloc Input/Output Context\n");
	exit(1);
      }

    // Allocate the AVFormatContext:
    format_context = avformat_alloc_context();
    if( nullptr == format_context )
      {
	fprintf(stderr, "ERR, can't alloc avformat codec ctx\n");
	exit(1);
      }

    //Format context
    //REV: does it work if I don't set this?
    format_context->pb = input_output_context;


    // Open the stream!
    fprintf(stdout, "Naive (automtic) probing for video/audio format\n");
    int err;
    size_t ntries = 50;
    size_t tried=0;

    
    
    while( avformat_open_input( &format_context, NULL, NULL, NULL ) < 0 && tried < ntries )
      {
	++tried;
	fprintf(stderr, "REV: failed avformat_open_input (try %ld/%ld)\n", tried, ntries);
	std::this_thread::sleep_for( std::chrono::milliseconds(20) );
      }
    if(ntries == 0 )
      {
	fprintf(stderr, "Final fail, avformat_open_input. Tried (%ld) times, failed to open.\n");
	exit(1);
      }

    //Get stream info
    err = avformat_find_stream_info( format_context, NULL );
    
    if( err < 0 )
      {
	fprintf(stderr, "REV: failed avformat_find_stream_info\n");
	exit(1);
      }

    //REV: should print info about available streams!
    av_dump_format( format_context, 0, NULL, 0 );



    fprintf(stdout, "Successfully probed format\n");


    
    //---- VIDEO
    {
      vid_stream_id = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO,
					  -1, -1, NULL, 0);
      
      fprintf(stdout, "Found best video stream [%d]\n", vid_stream_id );
      
      //if (-1 == vid_stream_id)
      if( vid_stream_id < 0 )
	{
	  fprintf(stderr, "REV: couldn't find video stream\n");
	  exit(1);
	}
      
      if( nullptr == vid_codec )
	{
	  fprintf(stderr, "Couldn't find codec for video stream from find_best_stream\n");
	  exit(1);
	}
      
      fprintf(stdout, "Trying to alloc video codec...\n");
      
      //vid_codec = const_cast<AVCodec*>(avcodec_find_decoder( format_context->streams[vid_stream_id]->codecpar->codec_id ));
      vid_codec = avcodec_find_decoder( format_context->streams[vid_stream_id]->codecpar->codec_id );
      vid_codec_context = avcodec_alloc_context3(vid_codec);
      
      int ret = avcodec_parameters_to_context(vid_codec_context, format_context->streams[vid_stream_id]->codecpar);
      
      fprintf(stdout, "Did params to context codec...\n");
      
      if(ret<0)
	{
	  fprintf(stderr, "REV: error, couldnt get params to context video...\n");
	  exit(1);
	}
      
      ret = avcodec_open2(vid_codec_context, vid_codec, NULL);
      if( ret < 0 )
	{
	  fprintf(stderr, "Error, couldn't open video codec context!\n");
	  exit(1);
	}

      //Get time base?!
      auto vidtb = format_context->streams[vid_stream_id]->time_base;
      fprintf(stdout, "Time base of video: [%d]/[%d]\n", vidtb.num, vidtb.den);
      if( vidtb.num != 1 )
	{
	  fprintf(stdout, "I don't know how to handle rational number timebases...\n");
	  exit(1);
	}
      pts_timebase_hz_sec = vidtb.den;

      //REV: convert rational number (Q) to floating point double real number (D)
      //video_fps = av_q2d( format_context->streams[vid_stream_id]->r_frame_rate );
      //int fnum = format_context->streams[vid_stream_id]->r_frame_rate.num;
      //int fden = format_context->streams[vid_stream_id]->r_frame_rate.den;
      //int fnum = vid_codec_context->framerate.num;
      //int fden = vid_codec_context->framerate.den;
      //video_fps = (double)fnum / fden;
      
      /*fprintf(stdout, "REV: VIDEO FPS=[%lf] (%d/%d)\n", video_fps, fnum, fden );
      if( video_fps <= 0 )
	{
	  fprintf(stderr, "REV: ERROR, could not get video FPS of stream?\n");
	  exit(1);
	  }*/

      
      fprintf(stdout, "Finished opening (video) codec\n");
    }

    //---- AUDIO
    //if( audio )
    {
      fprintf(stdout, "Locating audio decoder context stream\n");
      aud_stream_id = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO,
					  -1, -1, NULL, 0);
      
      fprintf(stdout, "Found best audio stream [%d]\n", aud_stream_id );
      
      if( nullptr == aud_codec )
	{
	  fprintf(stderr, "Couldn't find codec for audio stream from find_best_stream. Turning audio off (REV: maybe tobii version is <=1.14?)\n");
	}
      else
	{
	  if (aud_stream_id == -1)
	    {
	      fprintf(stderr, "REV: couldn't find audio stream\n");
	      exit(1);
	    }
	  
	  //aud_codec = const_cast<AVCodec*>(avcodec_find_decoder( format_context->streams[aud_stream_id]->codecpar->codec_id ));
	  aud_codec = avcodec_find_decoder( format_context->streams[aud_stream_id]->codecpar->codec_id );
	  aud_codec_context = avcodec_alloc_context3(aud_codec);
	  int ret = avcodec_parameters_to_context(aud_codec_context, format_context->streams[aud_stream_id]->codecpar);
	  if( ret<0 )
	    {
	      fprintf(stderr, "REV: error, couldnt get params to context audio...\n");
	      exit(1);
	    }
	  
	  ret = avcodec_open2(aud_codec_context, aud_codec, NULL);
	  if( ret < 0 )
	    {
	      fprintf(stderr, "Error, couldn't open aud codec context!\n");
	      exit(1);
	    }
	  
	  audio_rate_hz_sec = aud_codec_context->sample_rate;
	  audio_bytes_per_sample = av_get_bytes_per_sample(aud_codec_context->sample_fmt);
	  fprintf(stdout, "REV: audio codec sample rate (hz/sec)=[%d], bytes/sample=[%d]\n", audio_rate_hz_sec, audio_bytes_per_sample);
	}
      
      fprintf(stdout, "Finished init audio codec...\n");
    } //end AUDIO
    
    fprintf(stdout, "Finished init decoding!!...\n");
  } //end init_decoding

  
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
    av_freep(&(matavframe.data[0]));
    
    //Set the AVFrame's data to actually point to the cv::Mat memory chunk (.data())
    matavframe.data[0] = (std::uint8_t*)(matwrapav.data);
    
    //REV: not needed -- redundant?
    //REV: these fields were set with av_image_alloc -- I could have just used av_image_fill_arrays without av_image_alloc.
    //     It would fill in the required settings without actually allocating ->data[0] etc.
    //av_image_fill_arrays( avframeBGR->data, avframeBGR->linesize, (std::byte*)mat.data, AV_PIX_FMT_BGR24, avframeBGR->width, avframeBGR->height, linesize_align);

  } //end init_converter


  double get_fps()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return video_fps;
  }

  // probably a vector of uint16 PCM data?
  // Depends on the type of data stored in the thing (and the time base etc.)
  // So, I need to know all that shit to properly process it.
  bool process_pop_front_aud( std::uint64_t& pts )
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
	  aud_frame_q.pop();
	}
      }
    
    //Do something with it if we had one.
    if( nullptr != avframe )
      {
	av_frame_unref( avframe );
	av_frame_free( &avframe );
	return true;
      }

    return false;
  }
  
  bool process_pop_front_vid( cv::Mat& mat, std::uint64_t& pts )
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
	    
	    av_frame_unref( avframe );
	    av_frame_free( &avframe );
	    return true;
	  }
      }
    return false;
  }
  
  void matBGR24_copy_from_avframe( const AVFrame* inputframe, int dstX, int dstY, cv::Mat& result )
  {
    if( nullptr == swsContext )
      {
	init_converter( inputframe, dstX, dstY );
      }
    
    //REV: this will fill matavframe from inputframe
    sws_scale(swsContext, inputframe->data, inputframe->linesize, 0, inputframe->height, matavframe.data, matavframe.linesize);
    
    matwrapav.copyTo(result); //copy it over...in case line-size issues etc.? It's fine since I set to 1. But copy anyways...
    return;
    }
  
  void _estimate_set_fps()
  {
    double fps = -1.0;
    double min_sampletime_sec = 3.0;
    //uint64_t required_pts = min_sampletime_sec * pts_timebase_hz_sec;
    //const std::lock_guard<std::mutex> lock(mu);
    if( first_frame_pts < last_frame_pts && 
	(last_frame_pts - first_frame_pts)/(double)pts_timebase_hz_sec > min_sampletime_sec )
      {
	double elapsed_as_seconds = (double)(last_frame_pts - first_frame_pts)/(double)pts_timebase_hz_sec; //so, e.g. 90k will be 1 second
	fps = nframes / elapsed_as_seconds;
      }
    else
      {
	//fprintf(stderr, "REV: huge error? Should have >=2 frames, but something is fucked?\n");
      }
    video_fps = fps;
    return;
  }

  //REV: uses first N frames to estimate FPS?
  //REV: note first frames are usually bad?
  void decode_next_frames()
  {
    //To stop it from messing up while i'm destroying this lol.
    //const std::lock_guard<std::mutex> lock(shutdown_mu);
    
    AVPacket* avpkt = av_packet_alloc();
    
    //REV: this makes a pointer to an allocated STRUCT, which I will finally free with av_frame_free()
    // When I pass it to a function, it may set my data-pointers?
    //Let them worry about it? There are still pointers to it?
    
        
    //DOES NOT VALIDATE FRAMES FOR DECODER! But will read "frame worth" of data.
    // For video, always one frame. For audio, could be many (integer).
    //returned packet is ref counted, allocated by av_read_frame. I must call av_packet_unref() when I am done.
    int v = av_read_frame( format_context, avpkt );
    
    if( 0 == v )
      {
	if( vid_stream_id == avpkt->stream_index )
	  {
	    int sent = avcodec_send_packet( vid_codec_context, avpkt ); //SEND the mypacket to the decoder
	    //ownership of mypkt remains with me, and not written to. May create reference to it (or copy if not ref counted)
	    if( 0 == sent )
	      {
		/*
		  while(avcodec_receive_frame(m_status->videoCodecCtx, m_decodeFrame) == 0) {
		  convertAndPushPicture(m_decodeFrame);*/
		
		//avframe is 1) av_frame_unref() and then set pointer to point to a frame **allocated by decoder**.
		//Note, ref-counted. Am I guaranteed that the decoder will not write over it?
		int recv;
		

		do
		  {
		  bool pushed=false;
		  AVFrame* avframe = av_frame_alloc(); //I should dealloc it if I don't push it somewhere?

		  recv = avcodec_receive_frame( vid_codec_context, avframe ); //0 on success

		  if( recv >= 0 )
		    {
		      {
			const std::lock_guard<std::mutex> lock(mu);
		      
			if( 0==nframes )
			  {
			    int decode_error_flags = avframe->decode_error_flags;
			    if( 0 == decode_error_flags )
			      {
				pushed=true;
				//Don't let first frame have decode errors?
				vid_frame_q.push( avframe );
				first_frame_pts = avframe->pts;
				last_frame_pts = avframe->pts;
				++nframes;
			      }
			    //REV: will PTS ever reset??!
			  }
			else
			  {
			    pushed=true;
			    vid_frame_q.push( avframe );
			    last_frame_pts = avframe->pts;
			    ++nframes;
			  
			    _estimate_set_fps(); //internal
			  }
		      
		      }
		    
		      //Note, it's just a pointer to it? How do they know when I no longer use it? unref()
		      //Checked source, it is not ref() if error condition, only on success.
		      //http://ffmpeg.org/doxygen/3.4/decode_8c_source.html
		      //av_frame_unref( avframe ); //tell them I no longer reference it. Does it set if it fails?
		    }
		  else
		    {
		      if( !pushed )
			{
			  av_frame_unref(avframe);
			  av_frame_free(&avframe);
			}

		      //handle errors (receive_frame)
		    }
		} while( recv >= 0 );
		//Either way, unref the frame?! Even if it failed?
		
		
		//recv == AVERROR(EAGAIN) means it needs more data to get a frame...but it will fill up automatically.
		// AVERROR(EOF) means end of file? So just exit.
		//assert(recv != AVERROR(EINVAL));
		//if( AVERROR_EOF == state ) { }
		//assert(recv >= 0);
		//Frame f(std::move(frame), *pkt.get());
	      }
	    else
	      {
		//handle errors (send_packet)
	      }
	       
	  }
	else if( aud_stream_id == avpkt->stream_index )
	  {
	    //REV: do audio!!! (save it in packets? Note PTS etc. needs to be all lined up?)
	    //How is audio represented? Need to buffer it to the speakers via e.g. SDL2...
	    // Just buffer it as I get it in time? Does this avpkt have a PTS?
	    //fprintf(stdout, "START aud frame\n");
	    int sent = avcodec_send_packet( aud_codec_context, avpkt ); //SEND the mypacket to the decoder
	    //ownership of mypkt remains with me, and not written to. May create reference to it (or copy if not ref counted)
	    if( 0 == sent )
	      {
		/*
		  while(avcodec_receive_frame(m_status->videoCodecCtx, m_decodeFrame) == 0) {
		  convertAndPushPicture(m_decodeFrame);*/
		
		//avframe is 1) av_frame_unref() and then set pointer to point to a frame **allocated by decoder**.
		//Note, ref-counted. Am I guaranteed that the decoder will not write over it?
		int recv;
		

		do
		  {
		    bool pushed=false;
		    AVFrame* avframe = av_frame_alloc(); //I should dealloc it if I don't push it somewhere?

		    recv = avcodec_receive_frame( aud_codec_context, avframe ); //0 on success
		
		    if( recv >= 0 )
		      {
			{
			  const std::lock_guard<std::mutex> lock(mu);
		      
			  if( 0==nframes )
			    {
			      int decode_error_flags = avframe->decode_error_flags;
			      //fprintf(stdout, "FIRST (DECODED) AUD FRAME!\n");
			      if( 0 == decode_error_flags )
				{
				  //fprintf(stdout, "FIRST (GOOD) AUD FRAME!\n");
				  //Don't let first frame have decode errors?
				  pushed=true;
				  aud_frame_q.push( avframe );
				  first_aud_frame_pts = avframe->pts;
				  last_aud_frame_pts = avframe->pts;
				  ++naudframes;
				}
			  
			      //REV: will PTS ever reset??!
			    }
			  else
			    {
			      pushed=true;
			      aud_frame_q.push( avframe );
			      last_aud_frame_pts = avframe->pts;
			      ++naudframes;
			      //fprintf(stdout, "Got AN aud frame (%ld)!\n", naudframes);
			  
			      //_estimate_set_fps(); //internal
			    }
		      
			}
		    
			//Note, it's just a pointer to it? How do they know when I no longer use it? unref()
			//Checked source, it is not ref() if error condition, only on success.
			//http://ffmpeg.org/doxygen/3.4/decode_8c_source.html
			//av_frame_unref( avframe ); //tell them I no longer reference it. Does it set if it fails?
		      }
		    else
		      {
			//handle errors (receive_frame)
			if( !pushed )
			  {
			    av_frame_unref( avframe );
			    av_frame_free(&avframe);
			  }
		    
		      }
		  } while( recv>=0 );
		
	      } //end if sent successfully
	    else
	      {
		//handle errors (send_packet)
	      }

	    //fprintf(stdout, "END aud frame\n");
	  } //end if it is audio packet
	else
	  {
	    //some other type ofpacket?!
	  }
      } //end if vid stream
    else
      {
	//handle errors (packet reading)
      }
    
    av_packet_unref(avpkt);
    av_packet_free(&avpkt);
  } //end decode next frame
  
  
}; //end MPEGTS PARSER
