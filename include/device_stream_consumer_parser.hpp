
#pragma once


#include <reencoder.hpp>

#include <aud_encoder.hpp>

//Rotating Buffer Consumer -> Will be specialized
//Basically user implements a start(loop) of (one or more) doloop(loop) type functions, which
// -> CONSUME from mutexed_buf<uint8_t>
// -> PRODUCE something, into one or more timed_buffer<T>, in map<str,shared_ptr<timed_buffer>>. Map keys are "names"

// -> These are accessed via 
//　std::any get_buffer("string")　-> cast to shared_ptr<T> that user knows corresponds to that "name"
// Not ideal, but easy to work with

struct device_stream_consumer_parser_base
  : public looper
{
protected:
  std::map< std::string, std::any > outputs;
  std::map< std::string, std::any > active_outputs;
  
  bool saving_raw;
  uint64_t saving_idx;
  bool dropmem;
  std::string saving_raw_dir;

  double timebase_hz_sec; //assume all outputs share a timebase...? That may not be correct?
  
  std::mutex mu;

public:
  
    
  //Will derived classes call this constructor? YES
  device_stream_consumer_parser_base( const bool _dropmem=true )
    : saving_raw(false), saving_idx(0), dropmem(_dropmem)
  {   }
  
  ~device_stream_consumer_parser_base()
  {
    stop(); //just in case?
    //const std::lock_guard<std::mutex> lock(shutdown_mu);
  }
  
  virtual double get_timebase_hz_sec()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return timebase_hz_sec; //assume it is set? :(
  }

  void set_timebase_hz_sec( const double tb )
  {
    const std::lock_guard<std::mutex> lock(mu);
    timebase_hz_sec = tb;
  }

  //Use filesystem::path or some shit?
  void start_saving_raw( const std::string& rawdir )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !saving_raw )
      {
	saving_raw_dir = rawdir;
	saving_idx += 1;
	saving_raw = true;
      }
    else
      {
	fprintf(stderr, "WARNING: you tried to start saving, but you are already saving... (tag=[%s])\n", tag.c_str() );
      }
    //else, do nothing?
  }
  
  void stop_saving_raw()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( saving_raw )
      {
	saving_raw = false;
      }
    else
      {
	fprintf(stderr, "WARNING: you tried to stop saving, but you are not saving yet... (tag=[%s])\n", tag.c_str() );
      }
  }

  bool is_saving_raw()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return saving_raw;
  }

  std::set<std::string> possible_outputs()
  {
    std::set<std::string> result;
    for( auto& x : outputs )
      {
	result.insert( x.first );
      }
    return result;
  }

  std::set<std::string> get_active_outputs()
  {
    const std::lock_guard<std::mutex> lock(mu);
    std::set<std::string> result;
    for( auto& x : active_outputs )
      {
	result.insert( x.first );
      }
    return result;
  }
  
  bool activate_output( const std::string& op )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( outputs.contains( op ) )
      {
	active_outputs[op] = outputs[op];
	return true;
      }
    return false;
  }

  void deactivate_all_outputs()
  {
    const std::lock_guard<std::mutex> lock(mu);
    active_outputs.clear();
    return;
  }
  
  bool deactivate_output( const std::string& op )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( outputs.contains( op ) && active_outputs.contains( op ) )
      {
	//active_outputs[op] = outputs[op];
	auto it = active_outputs.find( op );
	if( it != active_outputs.end() )
	  {
	    it = active_outputs.erase( it );
	    return true;
	  }
	else
	  {
	    fprintf(stderr, "REV: something is wrong you are insane and the world has gone batshit\n");
	    exit(1);
	    return false; //this should NEVER HAPPEN (contains(op) should cover it)
	  }
      }
    return false;
  }

  void activate_all_outputs()
  {
    const std::lock_guard<std::mutex> lock(mu);
    active_outputs = outputs;
    return;
  }

  bool isactive( const std::string& s )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return active_outputs.contains(s);
  }
  
  //REV; return ANY output (not just active)...
  std::any get_active_output( const std::string& s )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( active_outputs.contains(s) )
      {
	return active_outputs[s];
      }
    else
      {
	std::any tmp; //can check has_value(); Will be nullany
	return tmp;
      }
  }

  std::any get_output( const std::string& s )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( outputs.contains(s) )
      {
	return outputs[s];
      }
    else
      {
	std::any tmp; //can check has_value(); Will be nullany
	return tmp;
      }
  }

  void stop()
  {
    localloop.stop();
  }
};




template <typename T>
struct device_stream_consumer_parser
  : public device_stream_consumer_parser_base
{
  //User will probably make outputs contain std::shared_ptr<T> at intialization.
  //Note, those shared ptrs will be class internal in the derived cls.

  
public:
  //Will derived classes call this constructor? YES
  device_stream_consumer_parser( const bool _dropmem=true )
    : device_stream_consumer_parser_base( _dropmem )
  {  return; }
  
  ~device_stream_consumer_parser()
  {
    stop(); //just in case?
    //const std::lock_guard<std::mutex> lock(shutdown_mu);
  }
  
  //REV: uhh..I won't change what the pointer points to...
  virtual void start( loopcond& loop, std::shared_ptr<device_stream_publisher<T>> dsp ) = 0;

  
  template <typename ST, typename STT>
  void rawsaveloop_json( loopcond& loop, loopcond& saveloop, timed_buffer<ST,STT>& tbuf, const std::string& fname, const std::string& tsfname, loopcond& working )
  {
    wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 1e7 );
        
    tsWriter<STT> tsfp;
    std::ofstream fp;
    
    const std::string tspath = saving_raw_dir + "/" + tsfname;
    const std::string path = saving_raw_dir + "/" + fname;
        
    tsfp.open( tspath );
    fp.open( path );
    
    size_t iters=0;
    while( (loop() && saveloop()) || tbuf.size()>0 )
      {
	wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 5e6 );
	auto result = tbuf.get_consume_first();
	if( result.has_value() )
	  {
	    auto ts = result.value().timestamp;
	    auto val = result.value().payload;
	    
	    fp << val << std::endl;
	    tsfp.write(ts);
	  }
	
	if( iters%100==0 && tbuf.size() > 50 )
	  { std::fprintf(stdout, "Still writing raw to [%s] (%ld elements left)\n", fname.c_str(), tbuf.size()); }
	++iters;
      }

    if( !working() )
      {
	fprintf(stdout, "What.the.fuck workin not on?\n");
	exit(1);
      }
    
    tsfp.close();
    fp.close();

    fprintf(stdout, "JSON SAVER -- LEAVING THREAD! [%s]\n", fname.c_str() );

    working.stop();
  }

  
  template <typename ST, typename STT>
  void rawsaveloop_vec2f( loopcond& loop, loopcond& saveloop, timed_buffer<ST,STT>& tbuf, const std::string& fname, const std::string& tsfname, loopcond& working )
  {
    wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 1e7 );
        
    tsWriter<STT> tsfp;
    std::ofstream fp;
    
    const std::string tspath = saving_raw_dir + "/" + tsfname;
    const std::string path = saving_raw_dir + "/" + fname;
    //tsfp.open( tspath, std::ofstream::binary );
    
    tsfp.open( tspath );
    fp.open( path, std::ofstream::binary );
    
    size_t iters=0;
    while( (loop() && saveloop()) || tbuf.size()>0 )
      {
	wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 5e6 );
	auto result = tbuf.get_consume_first();
	if( result.has_value() )
	  {
	    auto ts = result.value().timestamp;
	    auto val = result.value().payload;
	    
	    //fp.write( reinterpret_cast<char*>( val.v ), sizeof(val.v)/sizeof(val.v[0]) );
	    fp.write( reinterpret_cast<char*>( &(val.x) ), sizeof(val.x) );
	    fp.write( reinterpret_cast<char*>( &(val.y) ), sizeof(val.y) );
	    //fp.write( reinterpret_cast<char*>( val.v ), sizeof(val.v)/sizeof(val.v[0]) );
	    tsfp.write(ts);
	  }
	
	if( iters%100==0 && tbuf.size() > 50)
	  { std::fprintf(stdout, "Still writing raw to [%s] (%ld elements left)\n", fname.c_str(), tbuf.size()); }
	++iters;
      }

    if( !working() )
      {
	fprintf(stdout, "What.the.fuck workin not on?\n");
	exit(1);
      }
    
    tsfp.close();
    fp.close();

    fprintf(stdout, "VEC2F SAVER -- LEAVING THREAD! [%s]\n", fname.c_str() );

    working.stop();
  }
  
  template <typename ST, typename STT>
  void rawsaveloop_vec3f( loopcond& loop, loopcond& saveloop, timed_buffer<ST,STT>& tbuf, const std::string& fname, const std::string& tsfname, loopcond& working )
  {
    wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 1e7 );
        
    tsWriter<STT> tsfp;
    std::ofstream fp;
    
    const std::string tspath = saving_raw_dir + "/" + tsfname;
    const std::string path = saving_raw_dir + "/" + fname;
    //tsfp.open( tspath, std::ofstream::binary );
    
    tsfp.open( tspath );
    fp.open( path, std::ofstream::binary );
    
    size_t iters=0;
    while( (loop() && saveloop()) || tbuf.size()>0 )
      {
	wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 5e6 );
	auto result = tbuf.get_consume_first();
	if( result.has_value() )
	  {
	    auto ts = result.value().timestamp;
	    auto val = result.value().payload;
	    
	    //fp.write( reinterpret_cast<char*>( val.v ), sizeof(val.v)/sizeof(val.v[0]) );
	    fp.write( reinterpret_cast<char*>( &(val.x) ), sizeof(val.x) );
	    fp.write( reinterpret_cast<char*>( &(val.y) ), sizeof(val.y) );
	    fp.write( reinterpret_cast<char*>( &(val.z) ), sizeof(val.z) );
	    //fp.write( reinterpret_cast<char*>( val.v ), sizeof(val.v)/sizeof(val.v[0]) );
	    tsfp.write(ts);
	  }
	
	if( iters%100==0 && tbuf.size() > 50)
	  { std::fprintf(stdout, "Still writing raw to [%s] (%ld elements left)\n", fname.c_str(), tbuf.size()); }
	++iters;
      }

    if( !working() )
      {
	fprintf(stdout, "What.the.fuck workin not on?\n");
	exit(1);
      }
    
    tsfp.close();
    fp.close();

    fprintf(stdout, "VEC3F SAVER -- LEAVING THREAD! [%s]\n", fname.c_str() );

    working.stop();
  }

  
  template <typename ST, typename STT>
  void rawsaveloop_depth( loopcond& loop, loopcond& saveloop, timed_buffer<ST,STT>& tbuf, const std::string& fname, const std::string& tsfname, const int fps, loopcond& working )
  {
    //REV: wait for first data
    wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 1e7 );

    const std::string path = saving_raw_dir + "/" + fname;

#ifdef USEH5
    h5VideoWriter h5vw;
    h5vw.open(path);
#endif
#ifdef USEMOCKDEPTH
    //ghettoVideoWriter h5vw;
    mockDepthVideoWriter mdvw;
#endif
    
    
    
    tsWriter<STT> tsfp;
    const std::string tspath = saving_raw_dir + "/" + tsfname;
    tsfp.open( tspath );
    size_t iters=0;
    
    while( (loop() && saveloop()) || tbuf.size()>0 )
      {
	//double loop check :(
	wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 5e6 );
	
	//fprintf(stdout, "TBUF: [%ld] elements\n", tbuf.size() );
	
	//auto result = tbuf.get_consume_first();
	//std::optional<timed_buffer_element<ST,STT>> result = std::nullopt;
	auto result = tbuf.get_consume_first();
	
	//Don't even get value or do anything -- *STILL HAVE MEMORY LEAK
	if( result.has_value() )
	  {
	    auto ts = result.value().timestamp;
	    auto val = result.value().payload;

#ifdef USEMOCKDEPTH
	    if( !mdvw.is_open() )
	      {
		std::string tmppath = path + RTEYE_VID_EXT;
		fprintf(stdout, "REV: OPENING DEPTH (OR MKV) FILE (%s)!!!\n", tmppath.c_str());
		mdvw.open( tmppath
			   , val, fps
			   );

	      }
#endif
	    
	    //REV: DEBUG -- don't write to HDF5 see memory. -- MEMORY LEAK STILL HAPPENS!
#ifdef USEH5
	    h5vw.write( val );
#endif
#ifdef USEMOCKDEPTH
	    mdvw.write( val );
#endif
	    
	    tsfp.write( ts );
	  }
	//if( (!loop()||!saveloop()) && tbuf.size()%30==0 )
	if( iters%100==0 && tbuf.size() > 50 )
	  { std::fprintf(stdout, "Still writing raw to [%s] (%ld elements left)\n", fname.c_str(), tbuf.size()); }
	++iters;
      }

    fprintf(stdout, "H5 (MD) -- CLOSING! [%s]\n", fname.c_str() );
    
    tsfp.close();
#ifdef USEH5
    h5vw.close();
#endif
#ifdef USEMOCKDEPTH
    mdvw.close();
#endif
    
    fprintf(stdout, "H5 (MD) -- LEAVING THREAD! [%s]\n", fname.c_str() );
    if( !working() )
      {
	fprintf(stdout, "What.the.fuck workin not on?\n");
	exit(1);
      }
    working.stop();
  }

  template <typename ST, typename STT>
  void rawsaveloop_aud( loopcond& loop, loopcond& saveloop, timed_buffer<ST,STT>& tbuf, const std::string& fname,
			const std::string& tsfname, const int samprate, const int numchan, const int bps,
			const bool issigned, const bool isplanar, const bool isfloating, const int64_t timebase,
			loopcond& working )
  {
    wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 1e7 );
    
    const std::string path = saving_raw_dir + "/" + fname;
    const std::string tspath = saving_raw_dir + "/" + tsfname;
    
    aud_encoder enc( path, samprate, numchan, bps, issigned, isplanar, isfloating, timebase );
    bool succ = enc.init(); //opens file etc.
    if( !succ )
      {
	fprintf(stderr, "rawsave loop aud, could not open file! [%s]\n", fname.c_str());
	working.stop();
	return;
      }

    tsWriter<STT> tsfp;
    tsfp.open( tspath );

    size_t iters=0;
    
    //fprintf(stderr, "REV -- NOT IMPL -- RAWSAVELOOP -- AUD\n");
    //REV: DO STUFF
    while( (loop() && saveloop()) || tbuf.size()>0 )
      {
	wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 5e6 );
	
	auto result = tbuf.get_consume_first();
	
	//auto result = tbuf.get_first();
	//auto result = tbuf.drop_first();
	if( result.has_value() )
	  {
	    auto ts = result.value().timestamp;
	    //fprintf(stdout, "Audio ts [%lu]\n", ts);
	    auto val = result.value().payload;

	    bool filled = enc.fill_frame( val, ts );
	    bool wrote = enc.encode_audio_frame();
	    if( !wrote )
	      {
		fprintf(stderr, "Finished writing...?\n");
		break;
	      }
	      
	    tsfp.write( ts );
	  }
	  
	
	if( iters%100 == 0 && tbuf.size() > 50 )
	  { std::fprintf(stdout, "Still writing raw to [%s] (%ld elements left)\n", fname.c_str(), tbuf.size()); }
	++iters;
      }
    
    std::fprintf(stdout, "Finished writing raw to [%s], closing/flushing\n", fname.c_str() );
    
    
    
    tsfp.close();
    
    if( !working() )
      {
	fprintf(stdout, "What.the.fuck workin not on?\n");
	exit(1);
      }
    working.stop();
    
  }
      


    
  template <typename ST, typename STT>
  void rawsaveloop_vid( loopcond& loop, loopcond& saveloop, timed_buffer<ST,STT>& tbuf, const std::string& fname, const std::string& tsfname, const int fps, loopcond& working )
  {
    wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 1e7 );
    
    const std::string path = saving_raw_dir + "/" + fname;
    const std::string tspath = saving_raw_dir + "/" + tsfname;
    tsWriter<STT> tsfp;
    tsfp.open( tspath );
    
    
#if (ENCTYPE==VW)
    //int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '5'); //*SLOW* software encoder ;(
    int fourcc = cv::VideoWriter::fourcc('F', 'F', 'V', '1');
    cv::VideoWriter vw;
#elif (ENCTYPE==FFVW)
    ffVideoWriter ffvw;
#else // (ENCTYPE==FW)
    hevc_cvmat_vid_filewriter fw( fname );
#endif
    
    uint64_t iters=0;
    while( (loop() && saveloop()) || tbuf.size()>0 )
      {
	wait_or_timeout_cond( [&tbuf,&saveloop]() { return (!saveloop() || tbuf.size()>0); }, loop, tbuf.cv, tbuf.pmu, 5e6 );
	auto result = tbuf.get_consume_first();
	//auto result = tbuf.get_first();
	//auto result = tbuf.drop_first();
	if( result.has_value() )
	  {
	    auto ts = result.value().timestamp;
	    auto val = result.value().payload;

#if (ENCTYPE==VW)
	    if (!vw.isOpened() )
	      {
		bool iscolor = val.channels() > 1 ? true : false;
		vw.open( path, fourcc, fps, val.size(), iscolor );
	      }
	    if( vw.isOpened() )
	      {
		vw.write( val );
	      }
#elif (ENCTYPE==FFVW)
	    if ( !ffvw.is_open() )
	      {
		bool opened = ffvw.open( path, val, fps );
		if(!opened)
		  {
		    fprintf(stderr, "Wtf ffvw not opened?!\n");
		    //exit(1);
		  }
	      }
	      ffvw.write( val );
#else // (ENCTYPE==FW)
	    bool succ = fw.encode_write_frame( val, fps );
	    if( !succ )
	      {
		fprintf(stderr, "REV: some error writing to hvec_cvmat_video_writer?\n");
	      }
#endif
	    
	    tsfp.write( ts );
	  }
	  
	
	if( iters%100 == 0 && tbuf.size() > 50 )
	  { std::fprintf(stdout, "Still writing raw to [%s] (%ld elements left)\n", fname.c_str(), tbuf.size()); }
	++iters;
      }
    
    std::fprintf(stdout, "Finished writing raw to [%s], closing/flushing\n", fname.c_str() );
    
#if ENCTYPE==VW
    vw.release();
#elif ENCTYPE==FFVW
    ffvw.close();
#else // ENCTYPE==FW
    fw.close();
#endif
    
    tsfp.close();

    fprintf(stdout, "FFVW SAVER -- LEAVING THREAD! [%s]\n", fname.c_str() );

    if( !working() )
      {
	fprintf(stdout, "What.the.fuck workin not on?\n");
	exit(1);
      }
    working.stop();
  } //end rawsave vidloop

  
  
};
