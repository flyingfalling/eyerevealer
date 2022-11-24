#pragma once


#define GEN_OWN_PTS


// Note sample-rate and PTS what are the relationship? Fuck?
// They might not exactly line up?
//REV: double/float (i.e. floating point) formats return true, although it is irrelevant.
static bool av_sample_fmt_is_floating( const AVSampleFormat fmt )
{
  if( AV_SAMPLE_FMT_FLT == fmt ||
      AV_SAMPLE_FMT_DBL == fmt ||
      AV_SAMPLE_FMT_FLTP == fmt ||
      AV_SAMPLE_FMT_DBLP == fmt 
      )
    {
      return true;
    }
  else
    {
      return false;
    }
}

static bool av_sample_fmt_is_signed( const AVSampleFormat fmt )
{
  if( AV_SAMPLE_FMT_S16 == fmt ||
      AV_SAMPLE_FMT_S32 == fmt ||
      AV_SAMPLE_FMT_FLT == fmt ||
      AV_SAMPLE_FMT_DBL == fmt ||
      AV_SAMPLE_FMT_S64 == fmt ||
      AV_SAMPLE_FMT_S16P == fmt ||
      AV_SAMPLE_FMT_S32P == fmt ||
      AV_SAMPLE_FMT_FLTP == fmt ||
      AV_SAMPLE_FMT_DBLP == fmt ||
      AV_SAMPLE_FMT_S64P == fmt
      )
    {
      return true;
    }
  else
    {
      return false;
    }
}

bool info_from_codec_ctx( const AVCodecContext* aud_codec_context, int& sr, int& bps, int& nc, bool& isplanar, bool& issigned, bool& isfloating )
{
  if( !aud_codec_context ) { return false; }
    
  std::cout << "Sample format (of decoder!) : " << aud_codec_context->sample_fmt << std::endl;
  sr = aud_codec_context->sample_rate;
  bps = av_get_bytes_per_sample( aud_codec_context->sample_fmt );
  //nc = aud_codec_context->channels; //REV: deprecated!!!!
#ifdef FFMPEG_VERSION_GEQ_5 //FFMPEG_VERSION_GEQ( "5.0" )
  nc = aud_codec_context->ch_layout.nb_channels;
#else
  nc = av_get_channel_layout_nb_channels( aud_codec_context->channel_layout );
#endif
  //const char* fmtname = av_get_sample_fmt_name( aud_codec_context->sample_fmt );
  isplanar = (1==av_sample_fmt_is_planar( aud_codec_context->sample_fmt ));
  issigned = (1==av_sample_fmt_is_signed( aud_codec_context->sample_fmt ));
  return true;
}

AVSampleFormat av_sample_format_from_info( const int bps,
					   const bool issigned,
					   const bool isplanar,
					   const bool isfloating
					   )
{
  if( false == isfloating )
    {
      if( issigned )
	{
	  if( 2 == bps )
	    {
	      if( isplanar )
		{	return AV_SAMPLE_FMT_S16P; }
	      else
		{ 	return AV_SAMPLE_FMT_S16; }
	    }
	  else if( 4 == bps )
	    {
	      if( isplanar )
		{     return AV_SAMPLE_FMT_S32P; }
	      else
		{     return AV_SAMPLE_FMT_S32; }
	    }
	  else if( 8 == bps )
	    {
	      if( isplanar )
		{     return AV_SAMPLE_FMT_S64P; }
	      else
		{   	return AV_SAMPLE_FMT_S64; }
	    }
	}
      else //not signed
	{
	  if( 1 == bps )
	    {
	      if( isplanar )
		{	return AV_SAMPLE_FMT_U8P; }
	      else
		{ 	return AV_SAMPLE_FMT_U8; }
	    }
	}
    }
  else //floating point
    {
      if( 4 == bps )
	{
	  if( isplanar )
	    {     return AV_SAMPLE_FMT_FLTP; }
	  else
	    {     return AV_SAMPLE_FMT_FLT; }
	}
      else if( 8 == bps )
	{
	  if( isplanar )
	    {     return AV_SAMPLE_FMT_DBLP; }
	  else
	    {     return AV_SAMPLE_FMT_DBL; }
	}
    }
  return AV_SAMPLE_FMT_NONE;
}

/* check that a given sample format is supported by the encoder */
static bool check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while(*p != AV_SAMPLE_FMT_NONE)
      {
        if(*p == sample_fmt)
	  { return true; }
        p++;
      }
    return false;
}

//REV: of a bit depth, etc.?
static AVSampleFormat get_first_supported_sample_fmt( const AVCodec *codec )
{
  const enum AVSampleFormat *p = codec->sample_fmts;
  return (*p); //REV: if there are no supported formats, it will be AV_SAMPLE_FMT_NONE, so whatever.
}

struct aud_encoder
{
private:
  const AVCodec* out_aud_codec=nullptr;
  AVCodecContext* out_aud_codec_context=nullptr;
  AVIOContext* out_io_context=nullptr;
  AVStream* out_stream = nullptr;
  AVFormatContext* out_format_context=nullptr;

  AVFrame* avframe = nullptr;
  AVFrame* avframe2 = nullptr;
  uint64_t pts = 0;

  AVSampleFormat srcfmt, dstfmt;
  
  std::string filename;
  int samplerate;
  int nchannels;
  int bytespersample;
  bool issigned;
  bool isplanar;
  bool isfloating;
  bool tb_hz_sec;
  
  SwrContext* swrctx=nullptr;
  
public:
  //REV: encode but don't write to file? Or...do both? lol
  //REV: yea, encode AVFrame -> AVPacket, and then write the packets to the file (if I want).
  aud_encoder( const std::string& _fn, const int _sr, const int _nc, const int _bps, const bool _signed, const bool _planar, const bool _floating, const int64_t timebase_hz_sec )
    : filename(_fn), samplerate(_sr),
      nchannels(_nc), bytespersample(_bps),
      issigned(_signed), isplanar(_planar), isfloating(_floating), tb_hz_sec(timebase_hz_sec),
      swrctx(nullptr)
  {
    //nthing...
  }

  ~aud_encoder()
  {
    deinit();
  }
    
  //AVSampleFormat sampleformat
  bool init()
  {
    int error;
    /* Open the output file to write to it. */
    if((error = avio_open(&out_io_context, filename.c_str(),
                           AVIO_FLAG_WRITE)) < 0)
      {
	fprintf(stderr, "Could not open output file '%s' (error '%s')\n",
		filename.c_str(), av_error_str(error).c_str());
	deinit();
	return false;
      }
    else
      {
	fprintf(stdout, "REV: aud_encoder, writing wav file [%s]\n", filename.c_str());
      }
    
    /* Create a new format context for the output container format. */
    if(!(out_format_context = avformat_alloc_context()))
      {
        fprintf(stderr, "Could not allocate output format context (no mem?)\n");
	deinit();
        return false; //AVERROR(ENOMEM);
      }
    
    /* Associate the output file (pointer) with the container format context. */
    out_format_context->pb = out_io_context;
    
    /* Guess the desired container format based on the file extension. */
    if(!(out_format_context->oformat = av_guess_format(NULL, filename.c_str(),
							   NULL)))
      {
	fprintf(stderr, "Could not find output file format\n");
	deinit();
	return false;
	//goto cleanup;
      }
 
    if(!(out_format_context->url = av_strdup(filename.c_str())))
      {
        fprintf(stderr, "Could not allocate url (no mem?).\n");
        error = AVERROR(ENOMEM);
	deinit();
	return false;
        //goto cleanup;
      }
    
    /* Find the encoder to be used by its name. */
    //REV: not guaranteed to have PCM_S16... at least MP2 can take various inputs?
    //if(!(out_aud_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE)))
    //REV: ok then...I guess we need to do some kind of SWS conversion before we pass frames here haha.
    //Easy...just do it here?
    //How can I check endianness of input? I can't. Just always write LE. Always write PCM for now...wave (LE guaranteed to work?)
    //rEV: will it work for floats/doubles etc.?
    int bigendian=-1;
    
    srcfmt = av_sample_format_from_info(bytespersample, issigned, isplanar, isfloating);
    dstfmt = srcfmt;
    
    std::cout << "Sample format (resampled!) is : " << srcfmt << std::endl;
    AVCodecID id = av_get_pcm_codec( srcfmt,
				     bigendian );
    
    
    
    if(!(out_aud_codec = avcodec_find_encoder(id)))
      {
        fprintf(stderr, "Could not find an encoder for the codec type\n");
	deinit();
	return false;
        //goto cleanup;
      }

    if( !check_sample_fmt(out_aud_codec, srcfmt) )
      {
        fprintf(stderr, "Encoder does not support sample format %s\n",
                av_get_sample_fmt_name(srcfmt));
	dstfmt = get_first_supported_sample_fmt( out_aud_codec );
        fprintf(stderr, "Will use this instead: %s\n",
                av_get_sample_fmt_name(dstfmt));
      }
    
    if( isfloating )
      {
	fprintf(stderr, "REV: warning/error -- we are using raw PCM, ensure you use appropriate container (e.g. mkv) for holding floating point! WAV can not hold floating point PCM (REV: TODO -- ENSURE CONTAINER IS RIGHT TYPE?!)\n");
	deinit();
	return false;
      }
    
    /* Create a new audio stream in the output file container. */
    //REV: avformat_free_context frees this (it is just getting pointer to stream created inside out_format_context)
    if(!(out_stream = avformat_new_stream(out_format_context, NULL)))
      {
        fprintf(stderr, "Could not create new stream (no mem?)\n");
        error = AVERROR(ENOMEM);
	deinit();
	return false;
        //goto cleanup;
      }
    
    //He did this instead...
        
    out_aud_codec_context = avcodec_alloc_context3(out_aud_codec);
    if(!out_aud_codec_context)
      {
        fprintf(stderr, "Could not allocate an encoding context (no mem?)\n");
        error = AVERROR(ENOMEM);
	deinit();
	return false;
        //goto cleanup;
      }
    
        
    
    /* Set the basic encoder parameters.
     * The input file's sample rate is used to avoid a sample rate conversion. */
#ifdef FFMPEG_VERSION_GEQ_5
    av_channel_layout_default( &(out_aud_codec_context->ch_layout), nchannels );
    int succ = swr_alloc_set_opts2(&swrctx,
				   &(out_aud_codec_context->ch_layout), //const AVChannelLayout *out_ch_layout,
				   dstfmt, //enum AVSampleFormat out_sample_fmt,
				   samplerate, //int out_sample_rate,
				   &(out_aud_codec_context->ch_layout), //const AVChannelLayout *in_ch_layout,
				   srcfmt, //enum AVSampleFormat  in_sample_fmt,
				   samplerate, //int  in_sample_rate,
				   0, //int log_offset,
				   NULL); //void *log_ctx)
    if( 0 != succ )
      {
	fprintf(stderr, "REV: failed alloc swrctx (mem?)\n");
	exit(1);
      }

#else

    //int 	av_get_channel_layout_nb_channels(uint64_t layoutidx);
    out_aud_codec_context->channel_layout = av_get_default_channel_layout(nchannels);
    swrctx = swr_alloc_set_opts(NULL,  // we're allocating a new context
				out_aud_codec_context->channel_layout,  // out_ch_layout
				dstfmt,    // out_sample_fmt
				samplerate,                // out_sample_rate
				out_aud_codec_context->channel_layout, // in_ch_layout
				srcfmt,   // in_sample_fmt
				samplerate,                // in_sample_rate
				0,                    // log_offset
				NULL);                // log_ctx
    if( !swrctx )
      {
	fprintf(stderr, "REV: failed alloc swrctx (mem?)\n");
	exit(1);
      }

#endif

    
    swr_init( swrctx );
    
    
    out_aud_codec_context->sample_rate    = samplerate;
    out_aud_codec_context->sample_fmt     = dstfmt; //av_sample_format_from_info( bytespersample, issigned, isplanar, isfloating );
    
    int dstsr = -1, dstbps = -1, dstnc = -1;
    bool dstplanar = false, dstsigned = false, dstfloating = false;
    info_from_codec_ctx( out_aud_codec_context, dstsr, dstbps, dstnc, dstplanar, dstsigned, dstfloating );
    
    
    const int BITS_PER_BYTE = 8;
    int64_t bitrate = nchannels * samplerate * bytespersample * BITS_PER_BYTE; //REV: actually times 8?
    int64_t dstbittrate = dstnc * dstsr * dstbps * BITS_PER_BYTE;
    //out_aud_codec_context->bit_rate = bitrate;

    //REV: ->linesize is the number of BYTES inside a single PLANE for planar representation...
    
    //REV: wait, I want my time base to be in 90k...so that it goes 1-1 with video, right...?
    //I.e. the PTS time base...
    /* Set the sample rate for the container. */
    //out_stream->time_base = time_base;
#ifdef GEN_OWN_PTS
    if( dstsr != samplerate )
      {
	fprintf(stderr, "Smthing fucked\n"); exit(1);
      }
    out_stream->time_base.den = dstsr; //or sample rate?
#else
    out_stream->time_base.den = tb_hz_sec; //samplerate; //or sample rate?
#endif
    out_stream->time_base.num = 1;
    
    /* Some container formats (like MP4) require global headers to be present.
     * Mark the encoder so that it behaves accordingly. */
    if(out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
      {
	out_aud_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
      }
    
    /* Open the encoder for the audio stream to use it later. */
    if((error = avcodec_open2(out_aud_codec_context, out_aud_codec, NULL)) < 0)
      {
        fprintf(stderr, "Could not open output codec (error '%s')\n",
                av_error_str(error).c_str());
	deinit();
	return false;
        //goto cleanup;
      }
    
    error = avcodec_parameters_from_context(out_stream->codecpar, out_aud_codec_context);
    if(error < 0)
      {
        fprintf(stderr, "Could not initialize stream parameters\n");
	deinit();
	return false;
        //goto cleanup;
      }
    if( error < 0 )
      {
	fprintf(stderr, "REV: unknown error in aud_encoder init?\n");
	return false;
      }

    bool wroteheader = write_output_file_header();
    if( !wroteheader )
      {
	fprintf(stderr, "REV: couldn't write header wtf?\n");
      }

    fprintf(stderr, "REV: successfully initialized aud_encoder [%s]\n", filename.c_str());
    return true;
  }
  
  void deinit()
  {
    fprintf(stdout, "Deinit aud enc\n");
    flush_and_close();
    
    if( out_aud_codec_context )
      {
	avcodec_free_context(&out_aud_codec_context);
      }
    if( out_format_context )
      {
	if( out_format_context->pb )
	  {
	    avio_close(out_format_context->pb);
	  }
	avformat_free_context(out_format_context);
      }
    if( swrctx )
      {
	swr_free(&swrctx);
      }
    if( avframe )
      {
	//Unref? no... it is not ref counted.
	av_frame_free( &avframe );
      }
    if( avframe2 )
      {
	//Unref? no... it is not ref counted.
	av_frame_free( &avframe2 );
      }
    fprintf(stdout, "OUT Deinit aud enc\n");
  }
  
  
  //REV: write to file? Note pass it raw data and I'll figure out sizes etc.?
  bool write_output_file_header()
  {
    int error;
    if ((error = avformat_write_header(out_format_context, NULL)) < 0)
      {
	fprintf(stderr, "Could not write output file header (error '%s')\n",
		av_error_str(error).c_str());
	return false;
	//return error;
      }
    return true;
  }


  // REV: here nov!
  // "Input" frame is the input, which in my case is...always planar? Nah. Anyways, just do this shit. Note data[0] in non-planar will be everything (and will have
  // length of nchannels*samples per channel*bytespersample? in bytes.

  
  //REV: this is how many samples I will pass it at a time...
  //REV: I should just let it have its own PTS I guess, and I'll save separately in timestamps?
  //REV: frame*s* actually lol
  bool init_output_frame(const size_t nsamples_per_channel )
  {
    int error;
    
    /* Create a new frame to store the audio samples. */
    if (!(avframe = av_frame_alloc()))
      {
	fprintf(stderr, "Could not allocate output frame\n");
	return false;
      }

    if (!(avframe2 = av_frame_alloc()))
      {
	fprintf(stderr, "Could not allocate output frame2\n");
	return false;
      }
  
    /* Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity. */
    avframe->nb_samples     = nsamples_per_channel;
    avframe2->nb_samples     = nsamples_per_channel;
    
#ifdef FFMPEG_VERSION_GEQ_5 //( "5.0" )
    av_channel_layout_copy(&(avframe->ch_layout), &(out_aud_codec_context->ch_layout));
    av_channel_layout_copy(&(avframe2->ch_layout), &(out_aud_codec_context->ch_layout));
#else
    avframe->channel_layout = out_aud_codec_context->channel_layout;
    avframe2->channel_layout = out_aud_codec_context->channel_layout;
#endif
    avframe->format         = srcfmt;
    avframe2->format         = dstfmt;

    avframe->sample_rate    = out_aud_codec_context->sample_rate;
    avframe2->sample_rate    = out_aud_codec_context->sample_rate;
    
    /* Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified. */
    if ((error = av_frame_get_buffer(avframe, 0)) < 0)
      {
	fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
		av_error_str(error).c_str());
	av_frame_free(&avframe);
	return false;
      }

    if ((error = av_frame_get_buffer(avframe2, 0)) < 0)
      {
	fprintf(stderr, "Could not allocate output frame2 samples (error '%s')\n",
		av_error_str(error).c_str());
	av_frame_free(&avframe2);
	return false;
      }
    return true;
  }
  
  bool fill_frame( const std::vector< std::vector<std::byte> >& data, const uint64_t ts, const int _nchannels=0 )
  {
    int64_t nchan=data.size();
    if( nchan < 1 )
      {
	fprintf(stderr, "REV: passed data has 0 channesl?\n");
	return false;
      }

    bool tmpisplanar = true;
    if( _nchannels > nchan )
      {
	nchan = _nchannels;
	tmpisplanar = false;
	//It must be non-planar... (interlaced/interleaved?)
      }

    int64_t ctxnchan =
#ifdef FFMPEG_VERSION_GEQ_5
      out_aud_codec_context->ch_layout.nb_channels;
#else
    out_aud_codec_context->channels;
#endif
      if( nchan != ctxnchan )
      {
	//REV: error doesn't match up..
	//REV: I will not check channel layout because fuck it.
	fprintf(stderr, "Passed vector of bytes does not have correct #channels? Got [%lu] expected [%lu]\n", nchan, ctxnchan );
	return false;
      }
    
    size_t nbytes = data[0].size();
    for( size_t c=0; c<data.size(); ++c)
      {
	if( data[c].size() != nbytes )
	  {
	    fprintf(stderr, "Some channel is of different size [%lu] vs channel 0 [%lu]\n", data[c].size(), nbytes );
	    return false;
	  }
      }
    
    int64_t aud_bytes_per_sample = av_get_bytes_per_sample( out_aud_codec_context->sample_fmt );
    //bool isplanar = (1==av_sample_fmt_is_planar( out_aud_codec_context->sample_fmt ));
    //bool issigned = (1==av_sample_fmt_is_signed( out_aud_codec_context->sample_fmt ));
    
    if( aud_bytes_per_sample != bytespersample )
      {
	fprintf(stderr, "Something broken/not init yet?\n");
	return false;
      }

    size_t nsamps_per_chan;
    if( isplanar )
      {
	nsamps_per_chan = nbytes / aud_bytes_per_sample;
      }
    else
      {
	nsamps_per_chan = (nbytes / aud_bytes_per_sample) / nchan;
      }
    
    //write to appropriate channels etc. from a data array...
    if( !avframe || !avframe2 )
      {
	bool succ = init_output_frame( nsamps_per_chan );
	if( false == succ )
	  {
	    fprintf(stderr, "Output frame not init yet?!\n");
	    return false;
	  }
      }
    
    //Interleave c1s1 c2s1 c1s2 c2s2 c1s3 c2s3 etc.?
    if( !tmpisplanar )
      {
	std::memcpy( avframe->data[0], (const void*)data[0].data(), data[0].size() );
      }
    else
      {
	//REV: memcopy or set? I'll just memcpy...
	for( size_t c=0; c<data.size(); ++c )
	  {
	    //REV: memmove?
	    std::memcpy( avframe->data[c], (const void*)data[c].data(), data[c].size() );
	  }
      }

#ifdef GEN_OWN_PTS
    avframe->pts = pts;
    avframe2->pts = pts; //No?
    pts += avframe->nb_samples;
#else
    avframe->pts = ts;
    avframe2->pts = ts; //No?
#endif

    //REV: resample to frame2!

    //REV: fuck, post sampled may be different based on delay shit?
    const int out_num_samples = av_rescale_rnd( swr_get_delay(swrctx, samplerate) + avframe->nb_samples, samplerate, samplerate, AV_ROUND_UP );
    if( out_num_samples != avframe2->nb_samples )
      {
	fprintf(stderr, "REV: ugh, estimated rescaled rnd samples [%d] is not equal to expected (preallocated) num samples in avframe2 [%d]\n", avframe2->nb_samples);
	exit(1);
      }

    //int nsamples = swr_get_out_samples(swsctx, int insamples )
    
    //swr_config_frame(swctx, outframe, infrmame) //could init from 2 avframes?
    
    //uint8_t* out_samples = NULL;
    //av_samples_alloc(&out_samples, NULL, out_num_channels, out_num_samples, AV_SAMPLE_FMT_FLTP, 0);
    //int converted = swr_convert(swrctx, &(avframe2->data[0]), out_num_samples, (const uint8_t**)&(avframe->data[0]), avframe->nb_samples);
    int err = swr_convert_frame( swrctx, avframe2, (const AVFrame*)avframe );
    if( 0 != err )
      {
	fprintf(stderr, "REV: wtf, swr convert failed\n");
	exit(1);
      }
     
    return true;
  }
  

  //REV: when should I call this? After I want to exit...this thing should keep spinning even with no input?
  //Make this thing spin and just feed shit in?
  void flush_and_close()
  {
    fprintf(stdout, "Will flush and close...[%s]\n", filename.c_str());
    AVFrame* tmp = avframe2;
    avframe2 = NULL;
    
    bool succ = true;
    succ = encode_audio_frame();
    
    
    avframe2 = tmp;
    succ = write_output_file_trailer();
    
    fprintf(stdout, "END Will flush and close...[%s]\n", filename.c_str());
    return;
  }
  
    
  
  
  bool encode_audio_frame()
  {
    
    int error=0;
    
    /* Send the audio frame stored in the temporary packet to the encoder.
     * The output audio stream encoder is used to do this. */
    error = avcodec_send_frame(out_aud_codec_context, avframe2);
    
    /* Check for errors, but proceed with fetching encoded samples if the
     *  encoder signals that it has nothing more to encode. */
    if(error < 0 && error != AVERROR_EOF)
      {
	fprintf(stderr, "Could not send frame for encoding (error '%s')\n",
		av_error_str(error).c_str());
	return false;
      }
    
    bool gotdat=true;
    bool wrote=true;
    
    while( wrote && gotdat )
      {
	wrote = encode_sent_audio_frame( gotdat );
      }
    
    return wrote;
  }

  
  bool encode_sent_audio_frame( bool& data_present )
  {
    
    int error=0;
    data_present = false;
    
    AVPacket *output_packet = av_packet_alloc();
    if( !output_packet )
      {
	fprintf(stderr, "Couldn't alloc packet? (nomem?)\n");
	return false;
      }
    
    /* Receive one encoded frame from the encoder. */
    //REV: can one send_frame get multiple receive_packet?
    error = avcodec_receive_packet(out_aud_codec_context, output_packet);
    /* If the encoder asks for more data to be able to provide an
     * encoded frame, return indicating that no data is present. */
    if(error == AVERROR(EAGAIN))
      {
	error = 0;
	av_packet_free(&output_packet);
	//fprintf(stderr, "REV: needs more data!\n");
	return true;
	/* If the last frame has been encoded, stop encoding. */
      }
    else if(error == AVERROR_EOF)
      {
	fprintf(stderr, "REV: EOF!\n");
	av_packet_free(&output_packet);
	return true;
      }
    else if(error < 0)
      {
	fprintf(stderr, "Could not encode frame (error '%s')\n",
		av_error_str(error).c_str());
	av_packet_free(&output_packet);
	return false;
	/* Default case: Return encoded data. */
      }
    else
      {
	//fprintf(stderr, "Got data (error=[%d], [%s])!!\n", error, av_error_str(error).c_str());
	data_present = true;
      }
    
    
    /* Write one audio frame from the temporary packet to the output file. */
    error = av_write_frame(out_format_context, output_packet);
    if( data_present &&
	error < 0 )
      {
	fprintf(stderr, "Could not write frame (error '%s')\n",
		av_error_str(error).c_str());
	av_packet_free(&output_packet);
	return false;
      }
    
    //fprintf(stdout, "Finished writing packet...\n");
    av_packet_free(&output_packet);
    return true;
  }

  
  bool write_output_file_trailer()
  {
    int error;
    if ((error = av_write_trailer(out_format_context)) < 0)
      {
	fprintf(stderr, "Could not write output file trailer (error '%s')\n",
		av_error_str(error).c_str());
	return false;
      }
    
    return true;
  }
  
  
};
