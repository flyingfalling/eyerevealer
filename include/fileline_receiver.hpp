
#pragma once

//25 mar 2022 -- updated to file receiver, read and recieve from file.
//For now, ignores timestamps in terms of real-time and just does ASAP.

//Note receives "messages"? Shit, in JSON, it is one per "line"...

struct fileline_receiver
  : public device_stream_publisher<std::byte>
{
protected:
  std::thread mythread;
  std::string fname;
  
public:
  fileline_receiver( const size_t mbufsize=0)
    : device_stream_publisher( mbufsize )
  {}
  
  fileline_receiver( const std::string& filename, const size_t mbufsize=0 )
    : device_stream_publisher( mbufsize ), fname(filename)
  {}

  ~fileline_receiver()
  {
    stop();
  }

  void stop()
  {
    std::fprintf(stdout, "IN STOP: FILELINE receiver loop thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: FILELINE receiver loop thread (Tag=[%s])\n", tag.c_str());
  }

  void doloop( loopcond& loop )
  {
    std::fprintf(stdout, "IN DOLOOP: FILELINE receiver loop thread (Tag=[%s])\n", tag.c_str());

    std::string str;
    std::vector<std::byte> vec;
    
    std::ifstream ifs( fname );
    //REV: read from file into vec, up to chunksize bytes... (or some timing info?)
    if( ifs )
      {
	std::getline( ifs, str );
	if( ifs )
	  {
	    //all correct, i.e. contains chunksize bytes.
	  }
	else
	  {
	    if( str.empty() )
	      {
		//Final thing had nothing (empty endl?)
	      }
	    else
	      {
		//else final line had something...but no endl character?
	      }
	    //exit/stop.
	    localloop.stop();
	  }
      }

    if( !str.empty() )
      {
	//Copy str to vec and add newline
	str += '\n'; //std::endl;
	vec = str_to_bytevec( str );
      }
    
    if( !vec.empty() )
      {
	mbuf.movefrom( vec ); //Will add call notifyall by default
      }
    
    fprintf(stdout, "UDP RECV: Got first data, will now communicate with same OTHER\n");
    while( localloop() && loop() )
      {
	vec.clear();
	
	//REV: read from file into vec, up to chunksize bytes... (or some timing info?)
	if( ifs )
	  {
	    std::getline( ifs, str );
	    if( ifs )
	      {
		//all correct, i.e. contains chunksize bytes.
	      }
	    else
	      {
		if( str.empty() )
		  {
		    //Final thing had nothing (empty endl?)
		  }
		else
		  {
		    //else final line had something...but no endl character?
		  }
		//exit/stop.
		localloop.stop();
	      }
	  }

	if( !str.empty() )
	  {
	    //Copy str to vec and add newline
	    str += '\n'; //std::endl;
	    vec = str_to_bytevec( str );
	  }
	
	if( !vec.empty() )
	  {
	    mbuf.movefrom( vec );
	    //mbuf.cv.notify_all(); //No need to wait() conditionally because I will just read no matter what haha.
	  }
	std::this_thread::sleep_for(std::chrono::microseconds(50));
	
      }
    std::fprintf(stdout, "OUT DOLOOP: FILELINE recv loop (Tag=[%s])\n", tag.c_str());
  }
  
  void start( loopcond& loop )
  {
    mythread = std::thread( &fileline_receiver::doloop, this, std::ref(loop) );
    //mythread.detach(); //No need to join. Resources released when execution stops.
  }
  
};
