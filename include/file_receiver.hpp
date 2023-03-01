
#pragma once

//25 mar 2022 -- updated to file receiver, read and recieve from file.
//For now, ignores timestamps in terms of real-time and just does ASAP.

//Note receives "messages"? Shit, in JSON, it is one per "line"...

struct file_receiver
  : public device_stream_publisher<std::byte>
{
protected:
  std::thread mythread;
  std::string fname;
  size_t chunksize;
    
public:
  file_receiver( const size_t mbufsize=0)
    : device_stream_publisher( mbufsize )
  {}
  
  file_receiver( const std::string& filename, const size_t _chunksize, const size_t mbufsize=0 )
    : device_stream_publisher( mbufsize ), fname(filename), chunksize(_chunksize)
  {}

  ~file_receiver()
  {
    stop();
  }

  void stop()
  {
    std::fprintf(stdout, "IN STOP: FILE receiver loop thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: FILE receiver loop thread (Tag=[%s])\n", tag.c_str());
  }

  void doloop( loopcond& loop )
  {
    std::fprintf(stdout, "IN DOLOOP: FILE receiver loop thread (Tag=[%s])\n", tag.c_str());
    std::vector<std::byte> vec(chunksize);

    std::ifstream ifs( fname, std::ifstream::binary );
    //REV: read from file into vec, up to chunksize bytes... (or some timing info?)
    if( ifs )
      {
	vec.resize( chunksize );
	ifs.read( reinterpret_cast<char*>(vec.data()), vec.size() );
	if( ifs )
	  {
	    //fprintf(stdout, "Read from file %s  %ld bytes\n", fname.c_str(), chunksize );
	    //all correct, i.e. contains chunksize bytes.
	  }
	else
	  {
	    auto bytesread = ifs.gcount();
	    //fprintf(stdout, "Read from (END OF) file %s  %ld bytes\n", fname.c_str(), bytesread );
	    vec.resize( bytesread );
	    //exit/stop.
	    localloop.stop(); //This will try to join this own thread....fuck that. Need a better way. Set localloop=false;
	  }
      }
        
    if( !vec.empty() )
      {
	mbuf.movefrom( vec ); //Will add call notifyall by default
      }
    
    fprintf(stdout, "FILE RECV: Got first data, will now communicate with same OTHER\n");
    while( localloop() && loop() )
      {
	//REV: read from file into vec, up to chunksize bytes... (or some timing info?)
	if( ifs )
	  {
	    vec.resize( chunksize );
	    ifs.read( reinterpret_cast<char*>(vec.data()), vec.size() );
	    if( ifs )
	      {
		//fprintf(stdout, "Read from file %s  %ld bytes\n", fname.c_str(), chunksize );
		//all correct, i.e. contains chunksize bytes.
	      }
	    else
	      {
		auto bytesread = ifs.gcount();
		fprintf(stdout, "Read from (END OF) file %s  %ld bytes (will STOP())\n", fname.c_str(), bytesread );
		vec.resize( bytesread );
		//exit/stop.
		localloop.stop();
		fprintf(stdout, "STOPPED()\n");
	      }
	  }
	
	
	if( !vec.empty() )
	  {
	    mbuf.movefrom( vec );
	    //mbuf.cv.notify_all(); //No need to wait() conditionally because I will just read no matter what haha.
	  }
	
	//REV: need to sleep or it will hang? (reads data too fast?)
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

      } //end while loop() and  localloop()
    std::fprintf(stdout, "OUT DOLOOP: FILE recv loop (Tag=[%s])\n", tag.c_str());
  }
  
  void start( loopcond& loop )
  {
    mythread = std::thread( &file_receiver::doloop, this, std::ref(loop) );
    //mythread.detach(); //No need to join. Resources released when execution stops.
  }
  
};
