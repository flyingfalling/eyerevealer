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

#include <av_file_receiver.hpp> //looper
#include <rtsp_av_decoder.hpp> //NOT a looper







//REV: oops, deleted it all.

int main(int argc, char** argv)
{
  std::string packedfname;
  std::string h5fname;
  
  
  //Make the file receivers, etc. shared ptrs
  //then the decoders
  if( argc != 3 )
    {
      fprintf(stderr, "Incorrect argc [%d]\n", argc );
      return 1;
    }
  else
    {
      packedfname = std::string(argv[1]);
      h5fname = std::string(argv[2]);
    }
  
  auto file_rcvr = std::make_shared<av_file_receiver>(packedfname);
  //downcast it to proper (base) pointer type? Or will it work as-is?
  loopcond loop;
  loop.start();
  
  file_rcvr->start(loop);

  /*
  while( file_rcvr->islooping() ||
	 file_rcvr->mbuf.size() > 0 )
    {
      std::vector<tagged_avpkt_ptr> vec;
      auto npopped = file_rcvr->mbuf.popallto(vec);
      fprintf(stdout, "Processing [%lu] popped packets!\n", npopped);
      }*/
   
  //REV: need to start first or it will not init() and fmt_context will not be alloced.
  
  rtsp_av_decoder dec( file_rcvr );

  bool succdec = dec.init_decoding(); //defaults
  if( !succdec )
    {
      fprintf(stderr, "Something init dec failed\n");
      return 1;
    }

  h5VideoReader h5reader;

  bool success = h5reader.open( h5fname );
  if( !success ) { fprintf(stderr, "Failed to open h5 file?\n"); return 1; }

  auto h5vec = h5reader.readall();
  fprintf(stdout, "REV: got [%lu] h5 mats\n", h5vec.size() );
  
  
  size_t idx=1; //REV: h5 index 1 is same as 0th frame of the mkv file? Wtf? Because of my seek? But total frames is same...

  //REV: file_rcvr should stop looping when it detects "end of file" type shit? Should set it in the rtsp receiver shit too?
  while( file_rcvr->islooping() ||
	 file_rcvr->mbuf.size() > 0 )
    {
      
      //REV: get pkt from file_rcvr mbuf! Pop all.
      std::vector<tagged_avpkt_ptr> vec;
      auto npopped = file_rcvr->mbuf.popallto(vec);
      fprintf(stdout, "Processing [%lu] popped packets!\n", npopped);
      for( auto& tpkt : vec )
	{
	  //auto avpkt = tpkt.pkt;
	  dec.add_pkt_and_decode( tpkt );
	  av_packet_free(&tpkt.pkt);
	}
      
      AVFrame* frame=NULL;
      while( true == dec.pop_front_vid( frame ) )
	{
	  //convert to my type
	  cv::Mat check = unpack_aligned_nv12_from_avframe( frame );
	  av_frame_free(&frame);
	  
	  if( check.size() != h5vec[idx].size() )
	    {
	      fprintf(stderr, "Sizes do not match...\n");
	      std::cerr << check.size() << "   " << h5vec[idx].size() << std::endl;
	      return 1;
	    }
	  fprintf(stdout, "Comparing idx [%lu] Size (%d %d) vs (%d %d)\n", idx, check.size().width, check.size().height, h5vec[idx].size().width, h5vec[idx].size().height );
	  
	  cv::imshow("h5", h5vec[idx]);
	  cv::imshow("packed", check);
	  

	  cv::Mat dimg;
	  cv::absdiff(  h5vec[idx], check, dimg );
	  cv::imshow("diff", dimg);
	  char key = cv::waitKey(0);
	  
	  //reV: won't work they are unsigned ;(
	  cv::Mat img1, img2;
	  h5vec[idx].convertTo(img1, CV_32S, 1);
	  check.convertTo(img2, CV_32S, 1);
	  cv::Mat dmat = img1 - img2;
	  double min, max;
	  cv::minMaxLoc(dmat, &min, &max);
	  fprintf(stdout, "DIFF: Min: %lf   Max: %lf\n", min, max );

	  double min1,max1;
	  cv::minMaxLoc(img1, &min, &max);
	  cv::minMaxLoc(h5vec[idx], &min1, &max1);
	  fprintf(stdout, "H5: Min: %lf (%lf)   Max: %lf (%lf)\n", min, min1, max, max1 );
	  
	  cv::minMaxLoc(img2, &min, &max);
	  cv::minMaxLoc(check, &min1, &max1);
	  fprintf(stdout, "CK: Min: %lf (%lf)   Max: %lf (%lf)\n", min, min1, max, max1 );

	  //REV: no idea what is wrong. Either the h264 vaapi hardware encoding is fucking it up, or something else is...
	  size_t diffs = mat_are_equal(check, h5vec[idx]);
	  if( diffs > 0 )
	    {
	      fprintf(stderr, "Frame [%lu] is different (%lu/%lu pixels differ)\n", idx, diffs, h5vec[idx].size().width*h5vec[idx].size().height);
	      //return 1;
	    }
	    
	  ++idx;
	  
	}
    }
  
  
  return 0;
}
