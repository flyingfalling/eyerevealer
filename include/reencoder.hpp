
#pragma once

//REV: two re-encoders
//1) directly copy all aspects of a stream (and thus feed it raw AVPackets). I.e. no re-encoding. mkv container etc.
//2) encode from raw video frames (BGR24?) to target (mkv container, hevc etc.)


// I can't really lossfully encode hidden data (e.g. uint16 data in a stream of YUV444 etc -- just arbitrary size?)
//   Because lossfull encoding algorithms assume certain statistics of visual input (right?) and so the arbitrary shape of the
//   hidden input will not be "faithfully" reproduced closely even with high quality encoding... one bit off could be by chance the
//   "high" bit of one of the pixels or something, for example.


// REV: start of just losslessly (?) or lossily encoding HEVC hopefully via hardware, from raw images.
// REV: assume pixel format will be...what? If it is not RGB24 isn't it lossful? Fuck?
//      E.g. realsense frames are RGB I think...
//      TOBII3 and TOBII2 are H264 which are Y420 I believe...
//      Note 4:4:4 should be one byte for each (i.e. so 4x8 + 4x8 + 4x8 = 32*3 = 96 (bits) for 4 pixels? 96/4 is... 48/2 = 24!
//      ideally, RGB24->YUV444 should be (mostly) lossless conversion (assuming color space uses
//      all the bits! i.e. not the bullshit 16->235)...although slight bit of rounding may cause one off.
//Fuck 10 bit means it uses fixed representation, so instead of one byte being value 0-255, we do
// Note 422, 420, 444 are chroma subsampling representations, not depth per channel sample!

// REV: converting to RGB in first place is waste ;( Unless I do salmap shit? I could do YUV420 directly to color space shit?

//REV: note e.g. mkv is "container" (it contains vid, aud, subtitles etc.) multi streams. ANd tells e.g. size etc.
// Just like RTSP shit. I assume FFMPEG can make it for me. (header etc.).

//REV: note codec is just codec in which it is encoded in. I still need a DECODER to decode the data in codec format.
//AVCodecParserContext *parser;

//REV: note for running on android or apple, this may be an isolated linked library, not accessing typical systems and not for
// their chip.
// So, in that case, I'd have to offload the decoding/encoding up to the apple level, and use their stuff to use it, then pass it
// back down.

// So, I should provide an option (callback type thing) for when I receive data? But then, I should use their network stuff as well
// and not boost ASIO etc.? Fucking hell... why would FFMPEG people implement their shit anyways haha?

//https://www.ffmpeg.org/doxygen/trunk/transcoding_8c-example.html
//https://ffmpeg.org/doxygen/trunk/encode__audio_8c_source.html
//https://ffmpeg.org/doxygen/trunk/encode__video_8c_source.html

AVPixelFormat myfmt = AV_PIX_FMT_YUV420P;

struct StreamContext
{
  AVCodecContext* enc_ctx;
  //AVFrame* frame;
  
  //REV: destruct it...
};

//REV: encode separately than file writing? Meh too much work.

//REV: i need to specify each input stream
//REV: note copy audio directly? Note: what to do about time bases?
//REV: note what about different "types" (HEVC is lossy...? Usually?...and also YUV420 etc.)
//REV: what about when I want to encode


//REV: this just gives me correctly encoded (e.g. yuv420?) avpackets from the rgb frames.
//I can then e.g. send them over the network etc.
//REV: only issue is that I need to know the correct "format" for the frames if I will output to a given container (e.g. mkv?)
//REV: note, does SEEK only seek iframes (fuck?). 
struct hvec_cvmat_vid_encoder
{
private:
  const AVCodec* codec;
  AVCodecContext* codec_ctx=NULL;
  const AVCodecID codec_id = AV_CODEC_ID_HEVC;
  bool isopen=false;

  SwsContext* swsContext = nullptr;
  
public:

  hvec_cvmat_vid_encoder()
  { init_encoder(); }

  ~hvec_cvmat_vid_encoder()
  {
    if( swsContext ) sws_freeContext(swsContext);
    free_codecs();
  }

  //REV: codec_ctx->codec should point to codec?
  AVCodecContext* get_codec_context() const
  {
    return codec_ctx;
  }
  
  //REV: should point to codec?
  const AVCodec* get_codec() const
  {
    return codec;
  }

  void free_codecs()
  {
    avcodec_close( codec_ctx );
    avcodec_free_context( &codec_ctx );
    isopen=false;
  }
  
  AVFrame* to_fmt( const AVFrame* frame, const AVPixelFormat fmt )
  {
    if( !frame )
      {
	return NULL;
      }
    if( !swsContext )
      {
	
	swsContext = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
				    frame->width, frame->height,
				    fmt, SWS_AREA, NULL, NULL, NULL);
      }
    AVFrame* result = av_frame_alloc();
    if (!result)
      {
        fprintf(stderr, "Could not allocate video frame\n");
        return NULL;
      }
    
    result->format = fmt;
    result->width  = frame->width;
    result->height = frame->height;
    const int required_align = 32;
    int ret = av_image_alloc(result->data, result->linesize, result->width, result->height, (AVPixelFormat)result->format, required_align);
    if (ret < 0)
      {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
	av_frame_free(&result);
        return NULL;
      }

    const int stride=0;
    sws_scale(swsContext, frame->data, frame->linesize, stride, frame->height, result->data, result->linesize);

    return result;
  }
  
  //REV: no need to SWS convert, encoder can take raw BGR24...? (BGR0 is BGRA with all A set to 0...)
  bool init_encoder()
  {
    isopen=false;
    
    if( codec_ctx && codec )
      {
	return true;
      }

    //codec = avcodec_find_encoder_by_name( "hevc_vaapi" ); //codec_id );
    codec = avcodec_find_encoder( codec_id );
    if (!codec)
      {
	fprintf(stderr, "REV Codec not found\n");
	return false;
      }
    
    fprintf(stdout, "HVEC encoder: using encoder [%s]\n", codec->name );
    
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
      {
        fprintf(stderr, "REV: Could not allocate video codec context\n");
	return false;
      }
    
    return true;
  } //end init_encoder


  // I could move the mat, fps stuff here?! Would make it "easier" (one call..)
  bool open_encoder( const cv::Mat& mat, const int framerate )
  {
    
    if( !codec || !codec_ctx )
      {
	fprintf(stdout, "Something is wrong, you need to init codecs first!\n");
	return false;
      }

    if( isopen )
      {
	return true;
      }

    isopen = false;
    
    codec_ctx->width = mat.size().width;
    codec_ctx->height = mat.size().height;
    codec_ctx->time_base = (AVRational){1,framerate}; //typical timebase...? Same as my other guy used?
    
    //REV: codec-specific params...(options for size of compression etc.)
    //Note, these apply to other codecs as well, they are general options for lossy encoding...
    //codec_ctx->framerate = AVRational(25, 1);
    //codec_ctx->bitrate = 192000;
    //codec_ctx->gop_size = 10;
    //codec_ctx->max_b_frames = 1;
    //codec_ctx->keyint_min = 0;
    
    //https://x265.readthedocs.io/en/stable/presets.html
    av_opt_set(codec_ctx->priv_data, "preset", "medium", 0);
    
    //REV: no need to set frame rate (REV: will set in container...?)
    
    if( mat.type() == CV_8UC3 ) //3 channel BGR (assumed)
      {
	codec_ctx->pix_fmt = myfmt;
      }
    else if( mat.type() == CV_8UC1 ) //1 channel grayscale
      {
	codec_ctx->pix_fmt = AV_PIX_FMT_GRAY8;
      }
    else //unsupported...
      {
	fprintf(stderr, "REV: unsupported CV pix format (only acceptable are BGR24 and GRAY8, i.e. 8UC3 and 8UC1\n");
	return false;
      }
    
    int ret = avcodec_open2(codec_ctx, codec, NULL);
    
    if( ret < 0 )
      {
        fprintf(stderr, "Could not open codec [%s]\n", av_error_str(ret).c_str());
        return false;
      }
    
    isopen = true;
    return isopen;
    
  } //end open_encoder


  //av_frame_from_mat
  //Does one full copy of image data using av_image_copy
  //passed cv::Mat keeps its memory and needs to dealloc itself.
  //Returns newly allocated memory AVFrame* that will need to be dealloced appropriately.
  //matwrapav should not be changed during this function.

  //REV: this only works for single-plane, "interleaved" BGR24 or gray8 data.
  //Other stuff like YUV etc. is gonna get all fucky with data[0] and data[1] etc...
  AVFrame* av_frame_from_mat( const cv::Mat& _matwrapav )
  {
    cv::Mat matwrapav = _matwrapav; //pointers are same, no deep copy though.

    
    //REV: first, create an AVframe with no alignment offset (mapping my contiguous cv::Mat)
    AVFrame* matavframe = av_frame_alloc();
    if( !matavframe )
      {
	fprintf(stderr, "Could not alloc av frame\n");
	return NULL;
      }
    
    if( CV_8UC3 == matwrapav.type() )
      {
	matavframe->format = AV_PIX_FMT_BGR24;
      }
    else if( CV_8UC1 == matwrapav.type() )
      {
	matavframe->format = AV_PIX_FMT_GRAY8;
      }
    else
      {
	fprintf(stderr, "REV: error, mat->ffmpeg frame only work for BGR24 and GRAY8\n");
	av_frame_free(&matavframe);
	return NULL;
      }
    
    matavframe->width  = matwrapav.size().width;
    matavframe->height = matwrapav.size().height;
    
    //enum AVPixelFormat src_pixfmt = (enum AVPixelFormat)inputframe->format;
    //REV: wtf? SHouldn't it be 32 or some shit?
    //REV: problem, if e.g. I use 3 bytes per element, but I have e.g. 3x3 array, it will be 3*3*8 = 9x8 = 72, not evenly div by 32.
    //  av_image_copy(unalignedFrame->data, unalignedFrame->linesize, const_cast<const uint8_t**>(alignedFrame->data), alignedFrame->linesize, pix_fmt, width, height)

    //RV: fuck it make one AVFrame pointing to the CVMat data with linesize = 1, then do av_copy, with my other guy.

    //https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    const int cvlinesize_align = 1;
    if( !matwrapav.isContinuous() )
      {
	fprintf(stderr, "Warning: You passed an input BGR/gray CV::MAT that was not contiguous...I made it continuous with wasteful copies\n");
	matwrapav = matwrapav.clone(); //REV: this should deep copy it to my own new memory?
      }
    
    
    //REV: this "fills" linesize etc. correctly based on format and &c? (and align)
    //REV: note, did this allocate any dynamic things? For example, linesize is a dynamic array?
    //REV: Nope, it's static (int blah[NDATAPTRS]), so we are OK. Will be dealloc on stack.
    //REV: but there are quite a few other things ... extended data and other shit. :(
    //REV: cast to pixel format because we know it is vid, if aud we do AVSampleFormat
    int ret = av_image_alloc( matavframe->data, matavframe->linesize,
			      matavframe->width, matavframe->height, (AVPixelFormat)matavframe->format, cvlinesize_align );
    
    if( !ret )
      {
	fprintf(stderr, "Couldnt alloc new image for BGR conversion\n");
	av_frame_free(&matavframe);
	return NULL;
      }
    
    //This may cause issues at destruction.
    //"The allocated image buffer must be freed by calling av_freep(&pointers[0])"
    //REV: delete it now, I won't use it. I will use CV instead.
    //av_freep(&(matavframe.data[0]));
    //av_free(matavframe->data[0]);
    //auto tmpptr = matavframe->data[0];
    
    //Set the AVFrame's data to actually point to the cv::Mat memory chunk (.data())
    std::memcpy( matavframe->data[0], matwrapav.data, matwrapav.total() * matwrapav.elemSize() );
    //matavframe->data[0] = (std::uint8_t*)(matwrapav.data);
    
    //REV: we now have an AVFrame with proper params set,
    //who points to the cv::Mat data. Note the cv::data will be destroyed elsewhere whenever it is deallocated. We don't care.

    //Now, create a new avframe, with our required parameters, and copy over (will align it for us).
    AVFrame* frame = NULL;
    frame = av_frame_alloc();
    if (!frame)
      {
        fprintf(stderr, "Could not allocate video frame\n");
	av_frame_free(&matavframe);
	return NULL;
      }
    
    frame->format = matavframe->format;
    frame->width  = matavframe->width;
    frame->height = matavframe->height;

    const int hvec_linesize_align = 32;
    ret = av_image_alloc(frame->data, frame->linesize, frame->width, frame->height,
                         (AVPixelFormat)frame->format, hvec_linesize_align);
    if (ret < 0)
      {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
	av_frame_free(&matavframe);
	av_frame_free(&frame);
        return NULL;
      }

    av_image_copy( frame->data, frame->linesize,
		   const_cast<const std::uint8_t**>(matavframe->data), matavframe->linesize,
		   (AVPixelFormat)(matavframe->format),
		   matavframe->width, matavframe->height );


    //REV: properly deallocate the dummy avframe I made...
    //REV: Note this only works for data[0] type things (single "plane"?)
    //matavframe->data[0] = tmpptr;
    //av_frame_free(&matavframe);
    
    
    //REV: get my NV12 frame
    
    AVFrame* nv12frame = to_fmt( frame, myfmt );
    av_frame_free(&frame);
    
    return nv12frame;
  } //end av_frame_from_mat

  
  
  //REV: will encode a frame...
  std::vector<AVPacket*> encode( const cv::Mat& mat, const int fps )
  {
    std::vector<AVPacket*> result;
    
    AVFrame* frame = NULL; //flush if NULL
    
    if( false == mat.empty() )
      {
	frame = av_frame_from_mat( mat );
      }
    
    auto success = open_encoder(mat, fps);
    if( false == success ) //Could it ever be that I never put anything in?
      {
	//fprintf(stderr, "REV: issue initializing encoder...\n");
	fprintf(stderr, "REV: encoder not open yet (you need to call init_encoder(mat, fps))!\n");
	//free_codecs();
	//REV: I may get multiple "packets" per frame?
	return result;
      }

    
    int ret;
    
    ret=0;
    ret = avcodec_send_frame(codec_ctx, frame);
    if(ret < 0)
      {
        fprintf(stderr, "Error sending a frame for encoding: [%s]\n", av_error_str(ret).c_str());
      }
    
    ret=0;
    while(ret >= 0)
      {
	AVPacket* pkt = av_packet_alloc(); //REV: OK, this needs to take an ALLOCATED PACKET. (not e..g null pointer)
	ret = avcodec_receive_packet(codec_ctx, pkt);
	
	if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	  {
	    av_packet_free( &pkt );
	    break;
	  }
	
	if( ret < 0 )
	  {
	    fprintf(stderr, "Encoding error: [%s]\n", av_error_str(ret).c_str());
	    av_packet_free( &pkt );
	    break;
	  }
	
	result.push_back( pkt );

      } //end while ret>=0
    
    av_frame_free(&frame);
    return result;
  }
  
};


//REV: need to make a filewriter, which just gets packets.



//REV: notes about avpacket reference, free, unref, etc.
//av_packet_unref()  different than av_packet_free(). I guess free deallocs regardless of ref()/unref() state?
//Note av_packet_ref() may copy sometimes?
//Ah the packet may be stored in some buffer (in which case this is just a reference to it).
//So, if the buf field is not set, av_packet_ref() will copy it!
//Valid indefinitely in the buffer if buf is set, until unref reduces to 0...
//So, when I get a pointer to a pkt in a buffer, I need to make sure to av_packet_ref() it? Which copies from src to dst
// AVPacket*
//Otherwise it does a deep copy of it! Cool!
//So, I should never do pkt=otherpacket, I should do this ref shit. av_packet_clone(pkt);
//_clone() Create a new packet that references the same data as src!!! FUck.
// shortcut for: av_packet_alloc()+av_packet_ref().


struct hevc_cvmat_vid_filewriter
{
private:
  std::string filename;
  AVFormatContext* ofmt_ctx = NULL;
  hvec_cvmat_vid_encoder myenc;
  AVCodecContext* enc_ctx = NULL;
  StreamContext stream_ctx;
  int myfps=-1;
  
public:
  hevc_cvmat_vid_filewriter( const std::string& fname )
    : filename(fname)
  {
    enc_ctx = myenc.get_codec_context();
  }

  ~hevc_cvmat_vid_filewriter()
  {
    bool success = close();
    if( !success )
      {
	fprintf(stderr, "Some error closing/flushing [%s]\n", filename.c_str());
      }
  }

  void free_contexts()
  {
    if( ofmt_ctx )
      {
	//free ofmt_ctx
	if( ofmt_ctx->pb )
	  {
	    fprintf(stderr, "Closing AVIO!\n");
	    avio_close(ofmt_ctx->pb);
	  }

	avformat_close_input( &ofmt_ctx );
      }
  }

  bool close()
  {
    if( !ofmt_ctx )
      {
	return true;
      }

    // FLUSH ENCODER AND WRITE TO FILE
    if (!(enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
      { return true; } //not able to?
    
    auto result = encode_write_frame( cv::Mat(), myfps );
        
    free_contexts();
    return result;
  }
  
  bool open_output( const cv::Mat& mat, const int fps )
  {
    //REV: already open, I guess?
    if( ofmt_ctx )
      {
	return true;
      }
    
    
    int ret;
    ofmt_ctx=NULL;
    avformat_alloc_output_context2( &ofmt_ctx, NULL, NULL, filename.c_str() );
    
    if (!ofmt_ctx)
      {
	//av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
	fprintf(stdout, "Could not create output context (encode)\n");
	return false;
      }
    
    
    //REV: make streams (i need it right?)
    auto out_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_stream)
      {
	av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
	free_contexts();
	return false;
      }

    //REV: fuck should do this before I open the encoder?!?!?!?! FUck!
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
      {
	enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
      }
    
    myenc.open_encoder( mat, fps );
    
    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0)
      {
	av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #0\n");
	free_contexts();
	return false;
      }
    
    out_stream->time_base = enc_ctx->time_base;
    //stream_ctx[i].enc_ctx = enc_ctx;
    //Just one stream here (for testing). In future maybe put audio too..
    
    stream_ctx.enc_ctx = enc_ctx;
    
    
    //REV: this is just printing to stdout the details of my output format context.
    av_dump_format(ofmt_ctx, 0, filename.c_str(), 1);

    //If there is a file (i.e. we didn't fail to make it or some shit -- checking bit)
    if(!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
      {
	ret = avio_open(&ofmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
	if(ret < 0)
	  {
	    av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
	    free_contexts();
	    return false;
	  }
      }
    
    /* init muxer, write output file header */
    //REV: after this the file should be "legal"? Even if we fuck shit up partway through?
    ret = avformat_write_header(ofmt_ctx, NULL);
    if(ret < 0)
      {
	av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
	free_contexts();
	return false;
      }

    return true;
  } //end open_output
  
  
  
  
  bool encode_write_frame( const cv::Mat& mat, const int fps )
  {
    if( myfps < 0 )
      {
	if( fps <= 0 )
	  {
	    fprintf(stderr, "REV: You are setting to <= 0 FPS (=[%d]), something will be fucked up?\n", fps);
	    return false;
	  }
	myfps = fps;
      }
    
    if( fps != myfps )
      {
	fprintf(stderr, "You are adding a frame to a video filewriter with FPS set to [%d], but you are passing a different fps [%d] (note [%s])\n", myfps, fps, filename.c_str());
	return false;
      }
    
    bool success = open_output(mat, fps);
    if( false == success )
      {
	return false;
      }
    
    auto pkts = myenc.encode(mat, fps);
    
    const int streamidx = 0; //only one stream here brah.
    
    for( auto& pkt : pkts )
      {
	pkt->stream_index = streamidx;
	av_packet_rescale_ts(pkt,
			     stream_ctx.enc_ctx->time_base,
			     ofmt_ctx->streams[streamidx]->time_base); //REV: shouldn't these be the same...?
	
	int ret = av_interleaved_write_frame(ofmt_ctx, pkt);
	if( ret < 0 )
	  {
	    fprintf(stderr, "Error writing interleaved frame?! [%s]\n", av_error_str(ret).c_str() );
	    return false; //I should re-try? But I don't know how much I didn't write wtf?
	  }
	
	av_packet_unref( pkt );
      }
    return true;
  } //end encode_write_frame
  
};

