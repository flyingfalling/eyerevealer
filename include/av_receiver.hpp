
#pragma once

template <typename T>

struct av_receiver
  : public device_stream_publisher<T>
{
protected:
  std::string myurl; //This is constructed and opened depending on user thing?
  AVFormatContext* fmt_context; // = avformat_alloc_context();
  AVDictionary* codec_opts=NULL;  //Must be null to signal it needs to be written to fuck...
  bool codecs_initialized=false;
    
  bool shouldgo=true;
  std::mutex okmu;
  std::mutex cimu;
  std::mutex tmu;
  Timer t;
  bool aminit=false;
  
  Timer streamtime;

  bool eof=false;
  
public:
  
  std::mutex mu;
  

public:

  void set_eof( const bool _arg )
  {
    const std::lock_guard<std::mutex> lock(mu);
    eof = _arg;
  }
  
  bool get_eof()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return eof;
  }
  
  
  
  void reset_timer()
  {
    const std::lock_guard<std::mutex> lock(tmu);
    aminit = false;
    t.reset();
    return;
  }
  
  double get_elapsed()
  {
    const std::lock_guard<std::mutex> lock(tmu);
    if( aminit )
      {
	return -1; //or -1?
      }
    else
      {
	return t.elapsed();
      }
  }
  
  void set_timer_finished()
  {
    const std::lock_guard<std::mutex> lock(tmu);
    aminit = true;
    return;
  }
  
  //Use conditional w mutex, fuck it.
  void set_codecs_initialized()
  {
    const std::lock_guard<std::mutex> lock(cimu);
    codecs_initialized = true;
    return;
  }
  
  void unset_codecs_initialized()
  {
    const std::lock_guard<std::mutex> lock(cimu);
    codecs_initialized = false;
    return;
  }

  bool get_codecs_initialized()
  {
    const std::lock_guard<std::mutex> lock(cimu);
    return codecs_initialized;
  }
  
      
  av_receiver( const std::string& _myurl )
    : myurl(_myurl), fmt_context(NULL)
  {
    
  }
  
  ~av_receiver()
  {
    /*
    stop();
    
    fprintf(stdout, "AV_RECEIVER destructor -- stopped and now returning [%s]\n", myurl.c_str()); //REV: note this is the stop from above me? Super stop?
    
    return;*/
  }
    
  AVFormatContext* get_stream_format_info()
  {
    //REV: don't lock, sometimes user may want longer lock...
    //const std::lock_guard<std::mutex> lock(mu);
    return fmt_context;
  }

  AVDictionary* get_codec_opts()
  {
    //REV: it doesn't work here (ptr is fucked?)!!!
    
    return codec_opts;
  }
  
  void reset_streamtime()
  {
    streamtime.reset();
  }
  
  double get_streamtime()
  {
    return streamtime.elapsed();
  }
  
    
  void set_should_go( const bool s )
  {
    const std::lock_guard<std::mutex> lock(okmu);
    shouldgo = s;
  }

  bool get_should_go()
  {
    const std::lock_guard<std::mutex> lock(okmu);
    return shouldgo;
  }

  bool seek_first()
  {
    if( !fmt_context ) { return false; }

    for( int st=0; st<fmt_context->nb_streams; ++st )
      {
	av_seek_frame( fmt_context, st, 0, 0 );
      }
    return true;  
  }

  //REV: all have to impl this? Might as well ... not really used though...(i.e. each one calls its own...)
  virtual bool init() = 0;
  
};
