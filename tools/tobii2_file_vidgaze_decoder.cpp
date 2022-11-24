//Just decodes a tobii2 mpegts and .json file into twin files -- video file and gaze position?

//System
#include <iostream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <fstream>
#include <any>

//Other libs
//JSON
#include <nlohmann/json.hpp>

//STDUUID
#define UUID_SYSTEM_GENERATOR
#include <stduuid/uuid.h> //needs c++20 for fucking span


//FFMPEG
#ifdef __cplusplus
extern "C"{
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}
#endif

//OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/hdf/hdf5.hpp>

//REALSENSE
#ifdef WITH_REALSENSE
#include <librealsense2/rs.hpp>
#endif


//For the includes below lol
using namespace nlohmann;


//My files
#include <rteye2_defines.hpp>

#include <Timer.hpp>

#include <utilities.hpp>
#include <mutexed_buffer.hpp>
#include <timed_buffer.hpp>
#include <socket_udp6.hpp>
#include <ffVideoWriter.hpp>


#include <mpegts_parser.hpp>

#include <saver_thread_info.hpp> //depends timed buf
#include <looper.hpp> //contains producer?

#include <device_stream_publisher.hpp>

#include <device_stream_consumer_parser.hpp>

#include <file_receiver.hpp> //looper
#include <fileline_receiver.hpp> //looper

#include <tobii2_data_parser.hpp> //depends, looper, timed_buffer, mutexted_buffer
#include <tobii2_video_parser.hpp> //depends mpegts_parser, looper, timed_buffer, mutexted_buffer

//#include <tobii2_streaming_conn.hpp> //depends tobii2 video/data parser.






//REV: oops, deleted it all.

int main(int argc, char** argv)
{
  
  std::string jsonfname;
  std::string mpegfname;

  std::string vidoutfn;
  std::string vidtsfn;
  std::string gazeoutfn;
    
  //Make the file receivers, etc. shared ptrs
  //then the decoders
  if( argc != 6 )
    {
      return 1;
    }
  else
    {
      mpegfname = std::string(argv[1]);
      jsonfname = std::string(argv[2]);
      vidoutfn = std::string(argv[3]);
      vidtsfn = std::string(argv[4]);
      gazeoutfn = std::string(argv[5]);
    }
  
  size_t chunksize=4*4096;
  auto mpegrecv = std::make_shared<file_receiver>( mpegfname, chunksize );
  auto jsonrecv = std::make_shared<fileline_receiver>( jsonfname );
  
  tobii2_video_parser mpegparser;
  tobii2_data_parser jsonparser;
  
  loopcond loop;
  
  mpegrecv->start(loop);
  mpegparser.set_loop_sleep_msec(15);
  mpegparser.start( loop, mpegrecv );

  std::string videooutfname = vidoutfn; //"tobii2video2.mkv"; //REV: with timestamps (pts? Or?)
  std::string videotimestampfname = vidtsfn; //"tobii2video.ts";
  
  auto vidany = mpegparser.get_output( "scene" );
  mpegparser.activate_output("scene");
  std::shared_ptr<timed_buffer<cv::Mat,tobii2time_t>> vidptr;
  if( vidany.has_value() )
    {
      vidptr = std::any_cast<std::shared_ptr<timed_buffer<cv::Mat,tobii2time_t>>>( vidany  );
    }
  else
    {
      return 1;
    }
  
  //fps is?
  
  bool iscolor = true;
  //int fourcc = cv::VideoWriter::fourcc('F', 'F', 'V', '1');
  int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');
  cv::VideoWriter vw;
  
  uint64_t frameidx=0;

  std::ofstream vidtsof( videotimestampfname );
  vidtsof << "FRAME TSEC" << std::endl;

  //REV: final error, is that mbuf may have some bytes left (e.g. 79405), but its not enough to be "slurped"?
  //REV: but it still should slurp...implying my fence is the problem
  //REV: problem does it does not detect that the filereader (dsp) is finished
  while( (mpegrecv->islooping() && mpegparser.islooping()) ||
	 ( mpegrecv->mbuf.size() > 0 && mpegparser.islooping() ) ||
	 ( vidptr->size() > 0 )
	 )
    {
      
      std::this_thread::sleep_for( std::chrono::milliseconds(20) );
      fprintf(stdout, "VIDPTR [%5ld]  MBUF [%6ld]\n", vidptr->size(), mpegrecv->mbuf.size() );
      //fprintf(stdout, "looping\n");
      double mytimebase = mpegparser.get_timebase_hz_sec();
      //fprintf(stdout, "Got timebase %lf\n", mytimebase);
      if( mytimebase < 0 )
	{
	  fprintf(stderr, "Error? tb<0?\n");
	  exit(1);
	}
      vidptr->set_timebase_hz_sec( mytimebase ); //sleeps 2 realtime seconds or until pts_timebase is set.
      
      //fprintf(stdout, "tried to set, now will zero\n");
      //REV: need to zero everything, set timebase, etc.
      vidptr->zero_from_first_if_unzeroed();
      
      //fprintf(stdout, "zeroed\n");
      
      
      //Get mpeg video frame,
      //fprintf(stdout, "Size of VID PTR: %ld\n", vidptr->size() );
      int fps = mpegparser.mpegparser.get_fps() + 0.1;
      if( fps > 0 )
	{
	  auto maybe = vidptr->get_first();
	  if( maybe.has_value() )
	    {
	      auto ts = maybe.value().timestamp;
	      auto m = maybe.value().payload;
	  
	      if( !vw.isOpened() )
		{
		  fprintf(stdout, "Opening file with fps=[%ld]\n", fps);
		  auto viddims = m.size();
		  vw.open( videooutfname, fourcc, fps, viddims, iscolor);
		}
	      vw.write( m );
	      
	      auto maybesec = vidptr->clocktime_sec_of_timestamp( ts );
	      if( maybesec.has_value() )
		{
		  //if( maybesec.value() > 5 )
		  //  {break;}
		  fprintf(stdout, "Got video frame TS=[%010u]   SEC=[%06.3lf]\n", ts, maybesec.value() );
		  
		  auto timestamp = maybesec.value();
		  vidtsof << frameidx << " "
			  << timestamp << std::endl;
		}
	      else
		{
		  fprintf(stderr, "WTF?\n");
		  exit(1);
		}
	      vidptr->drop_first();
	      frameidx++;
	    }
	}
      
    } //end while looping vid etc.
  
  mpegparser.stop();
  
  
  /////////////////////////// JSON ////////////////////////
  
  jsonrecv->start(loop);
  jsonparser.start( loop, jsonrecv );

  
  auto gpany = jsonparser.get_output( "gp" );
  jsonparser.activate_output( "gp" );
  std::shared_ptr<timed_buffer<json,tobii2time_t>> gpptr;
  if( gpany.has_value() )
    {
      gpptr = std::any_cast<std::shared_ptr<timed_buffer<json,tobii2time_t>>>( gpany  );
    }
  else
    {
      return 1;
    }

  auto ptsany = jsonparser.get_output( "pts" );
  jsonparser.activate_output( "pts" );
  std::shared_ptr<timed_buffer<json,tobii2time_t>> ptsptr;
  if( ptsany.has_value() )
    {
      ptsptr = std::any_cast<std::shared_ptr<timed_buffer<json,tobii2time_t>>>( ptsany  );
    }
  else
    {
      return 1;
    }

  //REV: wait...how can I stop it? Will it stop it when there are no other things to get? How can I tell...
  //After jsonrecv finishes, THEN I loop parser until...the recv queue is empty (all read?)
  while( (jsonrecv->islooping() && jsonparser.islooping()) ||
	 (jsonrecv->mbuf.size() > 0 && jsonparser.islooping() )
	 )
    {
      gpptr->set_timebase_hz_sec( jsonparser.get_timebase_hz_sec() );
      ptsptr->set_timebase_hz_sec( jsonparser.get_timebase_hz_sec() );
    }
  
  std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  
  //jsonparser.stop();
  //I now have a timed buffer that will return PTS for a given TS (sync package).
  //So, for each gaze sample, use TS to find closest sync package, and use it to compute PTS.
  //Then, each PTS can be zeroed to the first MPEGTS video sample (PTS) to get time in seconds.    
  auto ts_pts = jsonparser.invert_for_ts_to_pts_buffer();
  
  jsonparser.stop();
  
  
  std::string gazeoutfname = gazeoutfn; //"tobii2gaze.gaze"; //Just timestamp, xpos, ypos.
  std::ofstream of(gazeoutfname);
  of << "TSEC X Y" << std::endl;
  
  while( gpptr->size() > 0 )
    {
      gpptr->set_timebase_hz_sec( jsonparser.get_timebase_hz_sec() );

      //Set time of PTS target from vid.
      
      //I should zero against...what? doesn't matter, just convert first PTS to blah.
      //Then, convert every single one to PTS...? And first of those is zero.
      
      auto maybegp = gpptr->get_first();
      if( maybegp.has_value() )
	{
	  gpptr->drop_first();
	  
	  //Convert to PTS and then SEC using ts_pts
	  auto ts = maybegp.value().timestamp;
	  auto gx = (float)maybegp.value().payload["gp"][0];
	  auto gy = (float)maybegp.value().payload["gp"][1];
	  
	  //Convert TS to PTS via ts_pts.
	  auto mpts = jsonparser.pts_from_ts( ts, mpegparser.get_timebase_hz_sec(), ts_pts );
	  if( mpts.has_value() )
	    {
	      auto pts = mpts.value();
	      auto maybezerosec = vidptr->clocktime_sec_of_timestamp( pts );
	      if( maybezerosec.has_value() )
		{
		  double assec = maybezerosec.value();
		  fprintf(stdout, "TS=[%10ld]  PTS=[%10ld] (SEC=[%6.3f])    X=[%3.2f] Y=[%3.2f]\n", ts, pts, assec, gx, gy );
		  of << assec << " "
		     << gx << " "
		     << gy << std::endl;
		}
	    }
	  
	}
    }

  jsonparser.stop();

  //REV: note tobii gaze starts from X=0, Y=0 at top-left.
  
  return 0;
}
