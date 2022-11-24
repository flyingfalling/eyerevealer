//REV: todo -- for save raw, simply serialize and save the RTSP packets I receive, as well as the SDP info in some way?
// The packets do not contain all the info I guess (or, they are intercepted by FFMPEG and processed in some way...fuck?)
// So, I can't self-contained reconstruct it without saving raw from the original SDP shit printed out from my successfully
// formed avformat_context object...i.e. the info about what is stored in those packets. Is there a readable way to print it
// (REV: from ffmpeg?).

// Saving raw much more efficient than decoding it online...in fact when they click "save all" I should probably stop live streaming?
// Or, reduce the update rate or some shit...

// (REV: save all RAW should be a special option)
// (REV: should also have options for streaming other shit like all data, and piping it to correct locations...)

#pragma once

#include <httprest.hpp>
#include <rtsp_receiver.hpp>
#include <pupilinvis_rtsp_vid_parser.hpp>
#include <pupilinvis_rtsp_gaze_parser.hpp>


//REV: pupil invis interface...discover avail pupils, etc.
//REV: note...keep track of "missing" things if they disappear?
//     Do they send out a thing to network? When they "join" or etc.

// executeQuery vs executeDiscovery? What is the difference!!??!!?! OK...discovery just says like,
// "yo there is some shit happening on _http.local or whatever?
// You gotta query that to figure out what it is tho? Bruh?

//REV: add a sanity check for every ffmpeg type I spoof, i.e. compare at least sizeof() my struct against their struct.
//REV: if they differ, that is an issue? Need to re-check.

//REV: other issue is that it seems the timing (rtsp server) STOPPED and RESTARTED the times they are fucking sending?

using namespace nlohmann;


//REV: OK, this is a single class interface... note that the query method has references to
// some static data elements, and so should only have one entry point in my
// code...I will do that in "available". I.e. have one point for mDNS available services, and then filter it...for tobii3 etc.

// Then, keep a queue of "available" tobii3 and pupil etc. things, dictionary by...IP?


//REV: then "parser" will take this? Shit, no it is supposed to take some other fucking shit, mbuf only!?!!?!?!!
//consumer parser consumes the produced stuff, and makes a timed buffer of useful shit ("outputs").
//note need to get time base and shit too.

// --> not all necessary info is stored in the mbuf (sometimes there is extra info needed?).
// --> Hm, can I handle decoding without the info? It's not an mpeg-ts stream so there is no header info in there? Fuck...
// --> Aight, assume there is no header into, find my own shit. I could make my own "parser" for data easily (since I Know format
//    from pupil invis). But, for the normal RTSP video shit...ugh?
//    How the fuck can I connect to the clock time? RTCP?
//    I can do it at *construction* time of the dsp shit? I.e. give it access to the format pointer. To let it decide what to do with
//    the different packet streams...OK!




//REV: hold info about each one separately? Unique...names?
//Make it a looper...
struct pupilinvis_interface
  : public looper
{

private:
  std::thread mythread;
  std::mutex gaze_mu;
  std::mutex vid_mu;
      
  std::shared_ptr<rtsp_receiver> gaze_stream=nullptr;
  std::shared_ptr<rtsp_receiver> vid_stream=nullptr;
  
  std::shared_ptr<pupilinvis_rtsp_vid_parser> vid_parser=nullptr; //should include some info about known e.g. visual angle of things?
  std::shared_ptr<pupilinvis_rtsp_gaze_parser> gaze_parser=nullptr;

  
public:
  //REV: this is the ip/port for querying services
  std::string ipaddr;
  int port;
  json status;
  bool goodstatus;
  std::mutex mu;
  std::shared_ptr<zeroconf_service_reply_struct> info;
  
  std::shared_ptr<pupilinvis_rtsp_vid_parser> get_vid_parser()
  {
    //fprintf(stdout, "Waiting for lock...\n");
    const std::lock_guard<std::mutex> lock(vid_mu);
    //fprintf(stdout, "DONE Waiting for lock...\n");
    return vid_parser;
  }

  std::shared_ptr<pupilinvis_rtsp_gaze_parser> get_gaze_parser()
  {
    //fprintf(stdout, "Waiting for lock...\n");
    const std::lock_guard<std::mutex> lock(gaze_mu);
    //fprintf(stdout, "DONE Waiting for lock...\n");
    return gaze_parser;
  }

  std::shared_ptr<rtsp_receiver> get_vid_stream()
  {
    //fprintf(stdout, "Waiting for lock...\n");
    const std::lock_guard<std::mutex> lock(vid_mu);
    //fprintf(stdout, "DONE Waiting for lock...\n");
    return vid_stream;
  }

  std::shared_ptr<rtsp_receiver> get_gaze_stream()
  {
    //fprintf(stdout, "Waiting for lock...\n");
    const std::lock_guard<std::mutex> lock(gaze_mu);
    //fprintf(stdout, "DONE Waiting for lock...\n");
    return gaze_stream;
  }

    
  
  pupilinvis_interface( const std::shared_ptr<zeroconf_service_reply_struct> _info )
    : ipaddr(_info->srcaddr), port(_info->dstport), goodstatus(false), info(_info)
  {  }

  pupilinvis_interface( const zeroconf_service_reply_struct& _info )
    : ipaddr(_info.srcaddr), port(_info.dstport), goodstatus(false), info(std::make_shared<zeroconf_service_reply_struct>(_info))
  {  }
    
  pupilinvis_interface( const std::string& _ipaddr, const int _port )
    : ipaddr(_ipaddr), port(_port), goodstatus(false)
  { fprintf(stderr, "REV: ERROR: Should not use this constructor pupil_invis!\n"); exit(1); }
  

  //REV: status of "PHONE" contains "time_echo_port" (as well as other shit like battery info?).
  // Patch for ffmpeg to get RTCP time of (each?) packet
  //  https://stackoverflow.com/questions/20265546/reading-rtcp-packets-from-an-ip-camera-using-ffmpeg

  // Ghetto way is just offset from "stream" start time... may "drift" due to imperfect integer representation of seconds?
  // (timebase 90k and pts? Assume it will correctly convert from NTP real clock time for us and not just blindly add 3300 each time
  // for example...
  /*
    AVFormatContext* ifmt_ctx = avformat_alloc_context();
    AVStream * st = xx; // select stream (from avformatcontext)
    double timebase = av_q2d(st->time_base);
    streamStartTime  = ifmt_ctx->start_time_realtime;
    streamStartTime + (1000000 * pkt->pts * time_base) ;
   */
  
  
  //Python rtsp lib...
  //https://github.com/marss/aiortsp
  
  
  //once I have the ip and port, I can use that shit directly...
  void query_status( )
  {
    
    //What to do if there is an error in get_json?
    std::string apiaction = "/api/status"; //do I need the question mark?
    json ret = get_json( ipaddr, apiaction, std::to_string(port) );
    //std::cout << ret << std::endl;
    if( !ret.contains("message") )
      {
	//REV: failed HTTP request etc.? Bad JSON?
	goodstatus = false;
	return;
      }
    
    if( std::string(ret["message"]).compare("Success") != 0 )
      {
	fprintf(stderr, "REV: Pupil Invis -- Something wrong in JSON request over HTTP REST...setting goodstatus = false\n");
	goodstatus = false;
	//exit(1);
      }

    const std::lock_guard<std::mutex> lock(mu);
    goodstatus = true;
    status = ret["result"]; //Include the result, not the json includeing result and message etc...
    return;
  }

  ~pupilinvis_interface()
  {
    fprintf(stdout, "ENDING -- in destructor of PUPILINVIS INTERFACE -- Will deconstruct... (should stop RTSP streams for gaze and world video)\n");
    stop();
    fprintf(stdout, "**FINISHED** ENDING -- in destructor of PUPILINVIS INTERFACE -- Will deconstruct... (should stop RTSP streams for gaze and world video)\n");
    return;
  }

  void stop()
  {
    fprintf(stdout, "START -- STOPPING PUPIL INVISIBLE\n");
    
    localloop.stop();
    
    //Will this break anything if one is waiting for conditional signal/mutex from another?

    {
      const std::lock_guard<std::mutex> lock(gaze_mu);
      if( gaze_stream )
	{
	  gaze_stream->stop();
	}
    
      if( gaze_parser )
	{
	  gaze_parser->stop();
	}
    }

    {
      const std::lock_guard<std::mutex> lock(vid_mu);
      if( vid_stream )
	{
	  vid_stream->stop();
	}

      if( vid_parser )
	{
	  vid_parser->stop();
	}
    }
    
    fprintf(stdout, "JOINING pupil invis thread...\n");
    if(mythread.joinable())
      {
	mythread.join();
      }
    fprintf(stdout, "DONE JOINING pupil invis thread...\n");
    fprintf(stdout, "DONE -- STOPPING PUPIL INVISIBLE\n");
  }

  //REV: this will start/stop streams as necessary I guess?
  bool check_connection( const std::string& streamtype  )
  {
    
    const std::string conn_type = "DIRECT";
    if( streamtype != "gaze" && streamtype != "world" )
      {
	fprintf(stderr, "Only sensor type of [gaze] and [world] are acceptable\n");
	exit(1);
      }

    //fprintf(stdout, "Checking connection status of [%s]\n", streamtype.c_str() );
    
    const std::lock_guard<std::mutex> lock(mu);
    if( !goodstatus )
      {
	fprintf(stderr, "Wtf, [%s] CHECK CONNECTION -- status is not set or some shit?\n", streamtype.c_str());
	return false;
      }
    
    const std::string sensor = streamtype; //"world";
    const std::string model = "Sensor";
    
    std::string vidaddr;
    int vidport;
    std::string vidproto;
    std::string vidparams;
    bool found=false;
    bool connected = false;
    for( auto& j2 : status )
      {
	if( j2["model"] == model )
	  {
	    auto j = j2["data"];
	    
	    if( j["sensor"] == sensor && j["conn_type"] == conn_type )
	      {
#if DEBUG_LEVEL > 10
		std::cout << j << std::endl;
#endif
		if( true == found )
		  {
		    fprintf(stderr, "REV: WARNING -- found more than one sensor and stream of desired type...\n");
		    exit(1);
		  }
		found=true;
		if( true == j["connected"] )
		  {
		    vidaddr = j["ip"];
		    vidport = j["port"];
		    vidproto = j["protocol"];
		    vidparams = j["params"];
		    connected = true;
		  }
		else
		  {
		    //fprintf(stderr, "WARNING, found an appropriate sensor, but it is not connected?\n");
		    connected = false;
		  }
	      }
	  }
      }

    if( !found )
      {
	fprintf(stderr, "REV: found no appropriate [%s] sensors!\n", streamtype.c_str());
	//exit(1);
	connected = false;
      }
    
    return connected;
  }
  
  //void stream_gaze_vid_rtsp( loopcond& availloop )
  void start( loopcond& loop )
  {
    startlooping();
    mythread = std::thread( &pupilinvis_interface::doloop, this, std::ref(loop) );
    return;
  }

  
  bool init_gaze( loopcond& availloop )
  {
    //for query
    const std::lock_guard<std::mutex> lock(gaze_mu);
    
    //loop if I still exist of course...when I am deconstructed, I will set to false
    gaze_stream = make_rtsp_receiver_for_streamtype( "gaze" );
    
    //This means they are not null right?
    if( gaze_stream )
      {
	//gaze_stream->start( availloop );
	gaze_parser = std::make_shared<pupilinvis_rtsp_gaze_parser>( gaze_stream ); //REV: use the denom of timebase for gaze parser timestamps? Or is it contained per-packet? Is PTS of gaze packets same as PTS of decoded video frames?

	//REV: stop the parser (just set variables looping to false)
	gaze_parser->stop();
	
	/*if( gaze_stream->islooping() && !gaze_parser->islooping() )
	  {
	    gaze_parser->start(availloop, gaze_stream);
	  }
	else
	  {
	    fprintf(stderr, "REV: couldn't start the gaze parser (gaze rtsp stream is not yet looping)\n");
	  }*/
      }
    else
      {
	//fprintf(stderr, "REV: couldn't get any type of gaze stream wtf? (got nullptr!)\n");
	return false;
      }

    fprintf(stdout, "FINISHED MAKING GAZE!\n");
    return true;
  }

  bool init_world( loopcond& availloop )
  {
    const std::lock_guard<std::mutex> lock(vid_mu);
    vid_stream = make_rtsp_receiver_for_streamtype( "world" );
        
    if( vid_stream )
      {
	//vid_stream->start( availloop );
	vid_parser = std::make_shared<pupilinvis_rtsp_vid_parser>( vid_stream );
	
	//REV: stop the parser (just set variables looping to false)
	vid_parser->stop();
	/*if( vid_stream->islooping() && !vid_parser->islooping() )
	  {
	    vid_parser->start(availloop, vid_stream);
	  }
	else
	  {
	    fprintf(stderr, "REV: couldn't start the vid parser (vid rtsp stream is not yet looping)\n");
	  }*/
      }
    else
      {
	//fprintf(stderr, "REV: couldn't get any type of vid stream wtf? (got nullptr!)\n");
	return false;
      }
    //fprintf(stdout, "FINISHED MAKING VID!\n");
    return true;
  } //end init pupil_invis

  void doloop( loopcond& availloop )
  {
    //REV: God.Fucking.Damnit fuck you pupil labs, just make it available in a normal way not in the raw RTP packets.
    //     Anyways, I now have to make a custom input/output and handle that I guess.
    //     Basically receive using a normal UDP packet receiver and then let it handle everything for me since I don't know the type.
    //     But that way I get the fucking timestamps at least.
    //     Since I know I have separate RTSP streams for gaze/video, I can at least separate them... but I need to differentiate
    //     different types of packets, e.g. if there are non-data packets in there...
    // REV: lol icing on the cake, FFMPEG has a AVClass* named class (thanks obama). So, it won't even compile in C++ if included
    // as headers lol.
        
    while( availloop() && localloop() )
      {
	query_status();
	
	{
	  const std::lock_guard<std::mutex> lock(gaze_mu);
	  
	  //REV: could it ever see this? This should be locked...
	  if( gaze_stream )
	    {
	      auto gazeconn = check_connection("gaze");
	      //REV: stop may be called while the other guy is in some weird ass state.
	      if( !gazeconn ) //&& gaze_stream->islooping() )
		{
		  
		  //fprintf(stdout, "PI Detected gazeconn disconnected, stopping thread!\n");
		  gaze_stream->stop(); //Vid parser also needs to stop? Nah...mbuf will be same.
		  gaze_stream->set_should_go(false);

		  //REV: I should really wait for mux here...oh well! Otherwise I might start it and immediately stop it.
		  if( gaze_parser ) // && gaze_parser->islooping() )
		    {
		      //REV: I shold really flush it first....
		      //gaze_parser->stop();
		    }
		}
	      
	      if( gazeconn && !gaze_stream->islooping() )
		{
		  fprintf(stdout, "PI Detected gaze reconnected...reconnecting!\n");
		  gaze_stream->set_should_go(true);
		  gaze_stream->start(availloop);
		  if( gaze_stream->islooping() && !gaze_parser->islooping() )
		    {
		      //REV; should un-zero the gaze parser...?
		      fprintf(stdout, "Now able to initiate PI GAZE parsing loop!\n");
		      gaze_parser->start(availloop, gaze_stream);
		    }
		}
	    }
	}

	
	{
	  const std::lock_guard<std::mutex> lock(vid_mu); 
	  
	  if( vid_stream )
	    {
	      auto worldconn = check_connection("world");
	      if( !worldconn && vid_stream->islooping() )
		{
		  //fprintf(stdout, "PI Detected worldconn disconnected, stopping thread\n");
		  
		  vid_stream->stop();
		  vid_stream->set_should_go(false);

		  if( vid_parser ) // && vid_parser->islooping() )
		    {
		      //vid_parser->stop();
		    }
		}

	      if( worldconn && !vid_stream->islooping() )
		{
		  fprintf(stdout, "PI Detected vid reconnected...reconnecting!\n");
		  vid_stream->set_should_go(true);
		  vid_stream->start(availloop);
		  if( vid_stream->islooping() && !vid_parser->islooping() )
		    {
		      fprintf(stdout, "Now able to initiate PI VIDEO parsing loop!\n");
		      vid_parser->start(availloop, vid_stream);
		    }
		}
	    }
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
      }
    
    return;
  } //end doloop
  
  std::shared_ptr<rtsp_receiver> make_rtsp_receiver_for_streamtype( const std::string& streamtype )
  {
    if( !goodstatus )
      {
	//fprintf(stderr, "REV: you must get good status before connecting...?! (i.e. get JSON info from the URL published) [%s:%d]\n", ipaddr.c_str(), port);
	return nullptr;
      }
    
    //const std::string conn_type = "WEBSOCKET";
    const std::string conn_type = "DIRECT";
    if( streamtype != "gaze" && streamtype != "world" )
      {
	fprintf(stderr, "Only sensor type of [gaze] and [world] are acceptable\n");
	//exit(1);
	return nullptr;
      }
    const std::string sensor = streamtype; //"world";
    const std::string model = "Sensor";
    
    std::string vidaddr;
    int vidport;
    std::string vidproto;
    std::string vidparams;
    bool found=false;
    bool connected=false;
    for( auto& j2 : status )
      {
	if( j2["model"] == model )
	  {
	    auto j = j2["data"];
	    
	    if( j["sensor"] == sensor && j["conn_type"] == conn_type )
	      {
		vidaddr = j["ip"];
		vidport = j["port"];
		vidproto = j["protocol"];
		vidparams = j["params"];
		found=true;
		connected=j["connected"];
		if( connected )
		  {
		    break; //take a connected one, if we can...
		    //otherwise, we keep trying anyways, will get the last unconnected, but don't care...
		  }
	      }
	  }
      }
    
    if( !found )
      {
	//fprintf(stderr, "REV: found no appropriate [%s] sensors (RETURNING NULLPTR!)!\n", streamtype.c_str());
	return nullptr;
      }
    
    //REV: fuck, what if it is IP6? Shouldn't I include them in squre brackets or some shit?
    //return f"{self.protocol}://{self.ip}:{self.port}/?{self.params}"
    const std::string apiaction = "/?" + vidparams;
    const std::string myurl = vidproto + "://" + vidaddr + ":" + std::to_string(vidport) + apiaction;

    
    const bool usecustomts = true;
    fprintf(stdout, "Creating RTSP ptr [%s]\n", myurl.c_str());
    auto myret = std::make_shared<rtsp_receiver>(myurl, usecustomts);
    fprintf(stdout, "In MAKE_RTSP_RECEIVER for [%s], will stop it\n", myurl.c_str());
    
    myret->stop();
    
    //Note major issue is that the video parser needs data from the loop to be initiated, i.e. to bootstrap itself...
    if( !connected )
      {
	fprintf(stdout, "Stream type [%s], found one, but not connected...(will automatically connect when it is connected)\n", streamtype.c_str() );
	//myret->stop();
      }
        
    return myret;
  } //end make_rtsp_stream_for_type
   
};

