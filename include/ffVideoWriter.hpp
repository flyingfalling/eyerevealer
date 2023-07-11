
#pragma once

#include <sstream>
#include <vector>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <condition_variable>


//Other libs
//JSON
#include <nlohmann/json.hpp>


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

#ifdef USEH5
#include <opencv2/hdf/hdf5.hpp>
#endif


//My libs
#include <Timer.hpp>
#include <timed_buffer.hpp>
#include <utilities.hpp>


//-f: format (on input/output)
//-y: global option -- automatically answer "y" to overwrite existing files
//-q: quality (per stream) -- depends on codec?//-qscale
//-s XxY: size (per stream)
//-r FPS: fps (per stream **VIDEO**) -- as input, ignore timestamps, assume constant input rate fps 
//-framerate FPS: fps (per stream ***FOR IMAGE INPUTS***)

//-vf X : FILTER GRAPH specified by X. Same as -filter:v (cf -filter)

// FILTERGRAPH SYNTAX -- filterchain is a list of comma "," separated file descriptions
// filtergraph is sequence of filterchains. Separated by semicolon ";"


//-pix_fmt X: set's pixel format. -pix_fmts will print available!
//-vtag FOURCC/TAG -- forces a tag to file
//-i URL -- input file URL
//Note MiB (base 2), MB (base 10)

//-c:stream X or -codec:stream X -- DECODING codec to use for X if input or ENCODING if output
//  name of encoder or "copy" (for output) meaning to just copy it...

//-i - on the output side means stdout (pipe:1).
//-i - on input means stdin and can be written as (pipe:0). (so I read stdout from this pipe -- it becomes my stdin)

//-nostats turns off statistics

/*
  For example

  ffmpeg -i INPUT -map 0 -c:v libx264 -c:a copy OUTPUT

  encodes all video streams with libx264 and copies all audio
  streams.

  For each stream, the last matching "c" option is applied, so

  ffmpeg -i INPUT -map 0 -c copy -c:v:1 libx264 -c:a:137 libvorbis OUTPUT

  will copy all the streams except the second video, which will be
  encoded with libx264, and the 138th audio, which will be encoded
  with libvorbis.
*/

//REV: todo -- wait for FPS to settle in each streaming_XXX() in tool
//REV: todo -- make "writer" for variable length "bit" things (audio) -- length in bytes and timestamp for each, and size of each element, and size of "image"
// sample (if they are fixed, e.g. frame W/H/C). There has got to be a nice way to do it without worrying about HDF5 corruption -_-; Fuck. LibPNG?
//REV: todo -- encode passed RGB etc. images nicely (and losslessly?) using FFMPEG (not CMDline, but proper encoder). Need to write "file header" shit too (at sttart)
//REV: todo -- write passed "packets" to file directly without decoding/encoding. I should be able to get info directly from ->get_input_context type shit.


//REV: video writer wrapping some ffmpeg encoder?
struct ffVideoWriter
{
private:
  std::string fullpath;
  FILE* pipeout;
  std::string ffmpegcmd;
  int fps;
  
  
public:
  ffVideoWriter()
    : pipeout(nullptr), fps(-1)
  { }
  
  ~ffVideoWriter()
  {
    close();
  }
  
  bool is_open( )
  {
    return !(nullptr==pipeout);
  }
  
  bool open( const std::string& fpath, const cv::Mat& mat, const int targfps )
  {
    fprintf(stdout, "Starting *OPEN* of ffVW (path=%s)\n", fpath.c_str() );
    if( is_open() )
      {
	fprintf(stdout, "Uh, wtf?\n");
	exit(1);
      }

    if( false == mat.isContinuous() )
      {
	std::fprintf(stderr, "Passed CV::MAT is not continuous...(probably an ROI from a larger matrix? Try clone()ing it)\n");
	exit(1);
      }
    
    fullpath = fpath;
    fps = targfps;
    
    std::string pxfmt = mat.channels() > 1 ? "bgr24" : "gray";
    std::string sizestr = tostr<int>(mat.size().width) + "x" + tostr<int>(mat.size().height);
    //export LIBVA_DRIVER_NAME=i965
    //https://github.com/Unmanic/unmanic/issues/167
    std::string vaapi_device = get_DRI_render_node(); //REV: can I discover this naturally some way?
    //vaapi_device = "/dev/dri/renderD129";
    
    if( fps <= 0 )
      {
	fprintf(stderr, "REV: huh, FPS <= 0? [%s]: [%d]\n", fullpath.c_str(), fps);
	return false;
      }

    //REV: glitches on newer intel processro VAAPI (11th and 12th gen) make corrputed HEVC output. Waiting for ffmpeg upstream patch...
    //Changed to h264 for now...
    const std::string encoder = "h264_vaapi"; //"hevc_vappi";
    
    std::string cmd;
    try{
      cmd = "ffmpeg";
#if (DEBUG_LEVEL < 20)
      cmd += " -loglevel quiet"; //suppress all output
      cmd += " -y -nostats"; //GLOBAL -y to overwrite existing files, -nostats for suppress stats
#endif
#if (DEBUG_LEVEL > 100)
      cmd += " -v verbose";
#endif
      //cmd += " -fflags +discardcorrupt";
      //fps=30;
      cmd += " -init_hw_device vaapi=foo:" + vaapi_device; //-hwaccel vaapi -vaapi_device " + vaapi_device;
      cmd += " -hwaccel vaapi -hwaccel_output_format vaapi -hwaccel_device foo";
      cmd += " -f rawvideo -vcodec rawvideo";//input options -- input will be raw video frames
      cmd += " -framerate " + tostr<int>(fps); //input options -- (input) assumed FPS fps
      cmd += " -pix_fmt " + pxfmt; //-- input pixel format (I will pass raw byte arrays so it needs to know pixel format and size)
      cmd += " -s " + sizestr;  //more input options -- input size
      cmd += " -i -"; //INPUT specification -- Read input from stdin (of my pipe) //cmd += " -i pipe:";
      cmd += " -flags -global_header";
      cmd += " -filter_hw_device foo";
      cmd += " -vf 'format=nv12|vaapi,hwupload' -c:v " + encoder;
      //REV: this was necessary for some CPU/GPU, but it breaks intel 12th gen and above is sufficient for AMD ryzen 7 6800U
      //cmd += " -vf 'format=nv12|vaapi,hwupload,scale_vaapi=format=nv12' -c:v " + encoder;
      cmd += " -qp 0"; //output options -- specify video filters and codec
      cmd += " \"" + fullpath + "\""; //Output path (filter specification blah blah [outputsink]?) -- WRAP IN QUOTES!
    }
    catch( std::exception& e )
      {
	std::cerr << "REV: wtf excepted?   " << e.what() << std::endl;
	std::fprintf(stderr, "My string is: [%s]\n", cmd.c_str());
	ffmpegcmd = std::move(cmd); //wtf does this do?
	exit(1);
      }
    
    ffmpegcmd = cmd;
    std::fprintf(stdout, "WILL OPEN pipeout with [%s]\n", ffmpegcmd.c_str());
    pipeout = popen( ffmpegcmd.c_str(), "w" );
    if( !pipeout )
      {
	std::fprintf(stderr, "REV: ERROR opening pipe with ffmpeg [%s]\n", ffmpegcmd.c_str());
	return false;
      }
    return true;
  }
  
  void close()
  {
    if( is_open() )
      {
	fflush(pipeout);
	pclose(pipeout);
      }
    pipeout = nullptr;
  }
  
  void write(  const cv::Mat& m )
  {
    if( false == m.isContinuous() )
      {
	fprintf(stderr, "Mat is not continuous (ff writer)\n");
	exit(1);
      }
    
    if( is_open() )
      {
	//REV: elemsize returns siz of single "element" in bytes (e.g. in 3-channel matrix of uint8, it will be 3)
	if( false == m.isContinuous() )
	  {
	    std::fprintf(stderr, "Passed CV::MAT is not continuous...(probably an ROI from a larger matrix? Try clone()ing it)\n");
	    exit(1);
	  }
	//fprintf(stdout, "Size of element (bytes): [%ld]\n", m.elemSize());
	size_t nbytes = m.rows * m.cols * m.elemSize();
	//fprintf(stdout, "Writing [%lu] bytes...\n", nbytes);
	size_t wrotebytes = fwrite(m.data, 1, nbytes, pipeout);
	if( wrotebytes != nbytes )
	  {
	    std::fprintf(stderr, "Some error fwrite of matrix to ffmpeg pipe? Tried [%ld] wrote [%ld]\n", nbytes, wrotebytes);
	    exit(1);
	  }
      }
    else
      {
	std::fprintf(stderr, "ERROR: trying to write to not-open ffVideoWriter\n");
	exit(1);
      }
    return;
  }
}; //end ffVideoWriter struct


//RV: insane/stupid method --
//    just resize to some arbitrarily sized "color" cv::Mat, and then write out (losslessly) to video file.
//    I need to remember true framesize though... from each "frame" I can then reshape it to my true shape.

// Need to handle "dregs" if it is e.g. float array ( Not divisible by 3?)
// E.g. 1920x1260 floats =

// REV: or...just treat it as black-and-white LOL, with 2x width (assuming width-first?)
// for float, treat it as 4x as wide...

struct ghettoVideoWriter
{
  //This treats input as uint8_t image with width (sizeof(element))
  //Then saves it (losslessly) to video file.

  //I must know the original type (which gives me size?)
private:
  std::string fullpath;
  int fps;
  cv::VideoWriter vw;
  

public:
  ghettoVideoWriter()
    : fps(-1)
  { }

  ~ghettoVideoWriter()
  {
    close();
  }

  bool is_open( )
  {
    return vw.isOpened();
  }

  bool open( const std::string& fpath, const cv::Mat& mat, const int targfps )
  {
    fprintf(stdout, "Starting *OPEN* of ghettoVW (path=%s)\n", fpath.c_str() );
    if( is_open() )
      {
	fprintf(stdout, "Uh, wtf?\n");
	exit(1);
      }

    
    fullpath = fpath;
    fps = targfps;
    
    int fourcc = cv::VideoWriter::fourcc('F', 'F', 'V', '1');
    //int fourcc = cv::VideoWriter::fourcc('H', 'E', 'V', 'C');
    bool iscolor = false;

    if( mat.channels() != 1 )
      {
	fprintf(stderr, "Only works for 1-channel (U16 or F32 or F64 etc.)\n");
	exit(1);
      }

    if( !mat.isContinuous() )
      {
	fprintf(stderr, "Only works continuous matrices (maybe an ROI of a larger matrix? Try .clone()ing it.\n");
	exit(1);
      }

    //REV: just make it 2 or 4 or etc. times as wide...
    cv::Size outsize( mat.elemSize() * mat.size().width ,
		      mat.size().height );
    
    
    vw.open( fullpath, fourcc, fps, outsize, iscolor );

    return true;
  }

  void close()
  {
    if( is_open() )
      {
	vw.release();
      }
  }

  void write(  const cv::Mat& mat )
  {
    if( is_open() )
      {
	cv::Size outsize( mat.elemSize() * mat.size().width ,
			  mat.size().height );

	//Mat (Size size, int type, void *data, size_t step=AUTO_STEP)
	cv::Mat reinterp = cv::Mat( outsize, CV_8UC1, mat.data );
	
	vw.write( reinterp );
      }
    else
      {
	std::fprintf(stderr, "ERROR: trying to write to not-open cvVideoWriter\n");
	exit(1);
      }
    return;
  }
  
  
};

//REV: reference to frame doesn't work...?
void dealloc_frame( AVFrame* frame )
{
  //REV: do I need to free all data[] or do they actually point to the same fucking structs? :(
  av_freep(&(frame->data[0])); //this is uint8_t*[N], with only [0] being actual alloc and others pointing to offsets in that data.
  av_frame_free(&frame);
}

//REV: origsize needs to be saved elsewhere (in file etc.)
AVFrame* pack_aligned_nv12_from_mat( const cv::Mat& mat )
{
  AVFrame* frame1 = av_frame_alloc(); //REV: this must be handled by user after return.
  if( !frame1 )
    {
      fprintf(stderr, "Out of memory\n");
      exit(1);
    }

  if( !mat.isContinuous() )
    {
      fprintf(stderr, "Not contig\n");
      exit(1);
    }
  
  frame1->format = AV_PIX_FMT_NV12;
  frame1->width  = 2 * mat.size().width;
  frame1->height =     mat.size().height;
  const int cvlinesize_align = 1;
   
  int ret = av_image_alloc( frame1->data, frame1->linesize,
			    frame1->width, frame1->height, (AVPixelFormat)frame1->format, cvlinesize_align );
  if( !ret )
    {
      fprintf(stderr, "Couldnt alloc new image for BGR conversion\n");
      exit(1);
    }
  
  std::memcpy( frame1->data[0], mat.data, mat.total() * mat.elemSize() );
  
  
  const std::size_t nbytes = frame1->width * frame1->height/2; //NV12 is 1 big chroma pixel for every 4 small luminance ones. 2 chroma channels (BY and RG i.e. U and V), thus 2 chroma pixels for every 4 small luminance ones. I.e. half the size.
  
  std::memset( frame1->data[1], 0, nbytes );
  
  AVFrame* frame = av_frame_alloc();
  if (!frame)
    {
      fprintf(stderr, "Could not allocate video frame\n");
      exit(1);
    }
    
  frame->format = frame1->format;
  frame->width  = frame1->width;
  frame->height = frame1->height;

  const int hvec_linesize_align = 32;
  ret = av_image_alloc( frame->data, frame->linesize, frame->width, frame->height,
		        (AVPixelFormat)frame->format, hvec_linesize_align );
  if (ret < 0)
    {
      fprintf(stderr, "Could not allocate raw picture buffer\n");
      exit(1);
    }
  
  //dst, dst, src, src.
  av_image_copy( frame->data, frame->linesize,
		 const_cast<const std::uint8_t**>(frame1->data), frame1->linesize,
		 (AVPixelFormat)(frame1->format),
		 frame1->width, frame1->height );
  
  dealloc_frame(frame1);
  
  return frame;
} //end pack_aligned...


//REV: I really should ref/unref frames (rather than alloc/free)
//REV: not some functions will "up" the reference counter, e.g. copying over... (to another AVFrame*)



//How the fuck do I know how much it added? I know I padded out to 32 I guess.
cv::Mat unpack_aligned_nv12_from_avframe( AVFrame* frame ) //, const cv::Size origsize )
{
  if( !frame )
    {
      fprintf(stderr, "REV: AVFrame empty\n");
      exit(1);
    }

  //REV: must be wid/2
  if( frame->width % 2 != 0 )
    {
      fprintf(stderr, "Whoa major problem...mod 2 is failure?\n");
      exit(1);
    }
  const cv::Size origsize( frame->width/2, frame->height ); //in pixels...assume this deals with step, alignment, etc. correctly.
  
  const int hvec_linesize_align = 32;
  if( frame->linesize[0] % hvec_linesize_align != 0 )
    {
      fprintf(stderr, "REV: Alignment not 32 as expected\n");
      exit(1);
    }
  
  
  AVFrame* frame1 = av_frame_alloc();
  if( !frame1 )
    {
      fprintf(stderr, "REV: Failed to alloc frame1\n");
      exit(1);
    }
  
  frame1->format = AV_PIX_FMT_NV12;
  frame1->width  = 2 * origsize.width;
  frame1->height =     origsize.height;
  const int cvlinesize_align = 1;
  
  int ret = av_image_alloc( frame1->data, frame1->linesize,
			    frame1->width, frame1->height, (AVPixelFormat)frame1->format, cvlinesize_align );
  if( !ret )
    {
      fprintf(stderr, "Couldnt alloc new image for BGR conversion\n");
      exit(1);
    }

  //REV: this...may not work? Actually it can be figured out what the buffer extra is from the wid/hei and the linesize!
  //REV: and the format.

  //dst, dst, src, src.
  av_image_copy( frame1->data, frame1->linesize,
		 const_cast<const std::uint8_t**>(frame->data), frame->linesize,
		 (AVPixelFormat)(frame->format),
		 frame->width, frame->height );

  //REV: now I have the right data.

  cv::Mat mat( origsize, CV_16UC1 );
  if( !mat.isContinuous() )
    {
      fprintf(stderr, "REV: newly made mat not contig\n");
      exit(1);
    }
  
  //REV: check size of data[0]?
  std::memcpy( mat.data, frame1->data[0], mat.total() * mat.elemSize() );

  dealloc_frame(frame1);
  
  return mat;
}

size_t mat_are_equal( const cv::Mat& a, const cv::Mat& b )
{
  if( a.size() != b.size() )
    {
      fprintf(stderr, "Comparing diff sized mats\n");
      std::cout << a.size() << "   " << b.size() << std::endl;
      exit(1);
    }
  cv::Mat diff = (a != b);
  // Equal if no elements disagree
  size_t diffs = cv::countNonZero(diff);
  
  return diffs;
}

//This treats input WxH U16 image as YUV420 image with empty chroma (waste of space), of 2*W x H.
  //Then saves it (losslessly) to video file using h264_vaapi (qp=0).
struct mockDepthVideoWriter
{
private:
  std::string fullpath;
  FILE* pipeout;
  std::string ffmpegcmd;
  int fps;
  
  
public:
  mockDepthVideoWriter()
    : pipeout(nullptr), fps(-1)
  { }
  
  ~mockDepthVideoWriter()
  {
    close();
  }
  
  bool is_open( )
  {
    return !(nullptr==pipeout);
  }
  
  bool open( const std::string& fpath, const cv::Mat& mat, const int targfps )
  {
    fprintf(stdout, "Starting *OPEN* of mockDepthVW (path=%s)\n", fpath.c_str() );
    if( is_open() )
      {
	fprintf(stdout, "Uh, wtf?\n");
	exit(1);
      }

    if( false == mat.isContinuous() )
      {
	std::fprintf(stderr, "Passed CV::MAT is not continuous...(probably an ROI from a larger matrix? Try clone()ing it)\n");
	exit(1);
      }
    
    fullpath = fpath;
    fps = targfps;
    
    int mockwid = 2 * mat.size().width; //2x because we are saving 16bit instead 8bit.
    int mockhei =     mat.size().height;
    
    std::string pxfmt = "nv12";
    std::string sizestr = tostr<int>(mockwid) + "x" + tostr<int>(mockhei);
    std::string vaapi_device = get_DRI_render_node(); //REV: can I discover this naturally some way?
    
    if( fps <= 0 )
      {
	fprintf(stderr, "REV: huh, FPS <= 0? [%s]: [%d]\n", fullpath.c_str(), fps);
	return false;
      }

    //REV: glitches on newer intel processro VAAPI (11th and 12th gen) make corrputed HEVC output. Waiting for ffmpeg upstream patch...
    //Changed to h264 for now...
#ifdef USE_HW_ACCEL
    fprintf(stdout, "MOCKDEPTH -- USING HW ACCEL ENCODING! (REV: this may not work, since writing lossless is not possible by most hw encoders?)\n");
    const std::string encoder = "h264_vaapi"; //"hevc_vappi";
    const std::string encodepreset = "fast";
#else
    fprintf(stdout, "MOCKDEPTH -- USING SOFTWARE ENCODING! (REV: this should work, as lossless should be possible)\n");
    const std::string encoder = "libx264"; //software encoding...ugh.
    //const std::string encodepreset = "-x265-params \"lossless=1:preset=ultrafast\"";
    const std::string encodepreset = "ultrafast"; //REV: something tells me this will not do well...filesize on order of 500MB/min
    //Note we are maybe wasting 50% of our space?
    //Note with ultrafast, we see roughly 1gb/minute (i.e. 15mb/sec).
    //Note write speed of disk is on the order of...? 100s of mb/sec? More a memory problem? Maybe better doing half-size.
#endif

    
    std::string cmd;
    try{
      cmd = "ffmpeg";
#if (DEBUG_LEVEL < 20)
      cmd += " -loglevel quiet"; //suppress all output
      cmd += " -y -nostats"; //GLOBAL -y to overwrite existing files, -nostats for suppress stats
#endif
#if (DEBUG_LEVEL > 100)
      cmd += " -v verbose";
#endif
      //cmd += " -fflags +discardcorrupt";
      //fps=30;
#ifdef USE_HW_ACCEL
      cmd += " -init_hw_device vaapi=foo:" + vaapi_device; //-hwaccel vaapi -vaapi_device " + vaapi_device;
      cmd += " -hwaccel vaapi -hwaccel_output_format vaapi -hwaccel_device foo";
#endif
      cmd += " -f rawvideo -vcodec rawvideo";//input options -- input will be raw video frames
      cmd += " -framerate " + tostr<int>(fps); //input options -- (input) assumed FPS fps
      cmd += " -pix_fmt " + pxfmt; //-- input pixel format (I will pass raw byte arrays so it needs to know pixel format and size)
      cmd += " -s " + sizestr;  //more input options -- input size
      cmd += " -i -"; //INPUT specification -- Read input from stdin (of my pipe) //cmd += " -i pipe:";
      cmd += " -flags -global_header";
      
      //cmd += " -vf 'format=nv12|vaapi,hwupload' -c:v " + encoder;
#ifdef USE_HW_ACCEL
      cmd += " -filter_hw_device foo";
      cmd += " -vf 'format=nv12|vaapi,hwupload,scale_vaapi=format=nv12'";
#else
      cmd += " -vf 'format=nv12'";
#endif
      cmd += " -c:v " + encoder;
      cmd += " -preset " + encodepreset;
      cmd += " -qp 0"; //output options -- specify video filters and codec
      cmd += " \"" + fullpath + "\""; 

    }
    catch( std::exception& e )
      {
	std::cerr << "REV: wtf excepted?   " << e.what() << std::endl;
	std::fprintf(stderr, "My string is: [%s]\n", cmd.c_str());
	ffmpegcmd = std::move(cmd); //wtf does this do?
	exit(1);
      }
    
    ffmpegcmd = cmd;
    std::fprintf(stdout, "WILL OPEN pipeout with [%s]\n", ffmpegcmd.c_str());
    pipeout = popen( ffmpegcmd.c_str(), "w" );
    if( !pipeout )
      {
	std::fprintf(stderr, "REV: ERROR opening pipe with ffmpeg [%s]\n", ffmpegcmd.c_str());
	return false;
      }
    return true;
  }
  
  void close()
  {
    if( is_open() )
      {
	fflush(pipeout);
	pclose(pipeout);
      }
    pipeout = nullptr;
  }
  
  void write(  const cv::Mat& m )
  {
    if( false == m.isContinuous() )
      {
	fprintf(stderr, "Mat is not continuous (ff writer)\n");
	exit(1);
      }
    
    if( is_open() )
      {
	//REV: elemsize returns siz of single "element" in bytes (e.g. in 3-channel matrix of uint8, it will be 3)
	if( false == m.isContinuous() )
	  {
	    std::fprintf(stderr, "Passed CV::MAT is not continuous...(probably an ROI from a larger matrix? Try clone()ing it)\n");
	    exit(1);
	  }
	
	//REV: problem is I can't "copy" from the mat? As stride/alignment at end may be problem? Nah should be OK.
	//REV: these calls must be leaking somehow...
	
	// -rc_mode CQP (constant quality)
	//	 ffmpeg -help encoder=hevc_vaapi
	AVFrame* nv12frame = pack_aligned_nv12_from_mat( m );
	
	/*cv::Mat check = unpack_aligned_nv12_from_avframe( nv12frame );
	  
	size_t ndiffs = mat_are_equal( check, m );
	if( ndiffs > 0 )
	  {
	    fprintf(stderr, "Encoded (packed) and decoded mat are not same (%lu/%lu) diffs!\n", ndiffs,
		    m.size().width*m.size().height);
	    exit(1);
	    }*/
	
	//REV: NV12 is XXXX   and  UV (not XXXX  U V
	if( !nv12frame->linesize[0] ||
	    !nv12frame->linesize[1] ||
	    !nv12frame->data[0] ||
	    !nv12frame->data[1] )
	  {
	    fprintf(stderr, "REV: nv12 frame isnot in the format we expect, something is null?\n");
	    for( int x=0; x<2; ++x )
	      {
		if( !nv12frame->data[x] || !nv12frame->linesize[x] )
		  {
		    fprintf(stderr, "Fprintf [%d] is missing/NULL\n", x);
		  }
	      }
	    
	    exit(1);
	  }
	
	//fprintf(stdout, "Size of element (bytes): [%ld]\n", m.elemSize());
	size_t elemsize_bytes = 1;
	size_t nbytesy = elemsize_bytes *
	  nv12frame->linesize[0] * nv12frame->height;
	size_t nbytesuv = elemsize_bytes *
	  
	  nv12frame->linesize[1] * nv12frame->height/2;
	
	size_t wrotey = fwrite( nv12frame->data[0],
				elemsize_bytes,
				nbytesy,
				pipeout );
	size_t wroteuv = fwrite( nv12frame->data[1],
				 elemsize_bytes,
				 nbytesuv,
				 pipeout );
	
	
	//av_frame_free( &nv12frame );
	dealloc_frame( nv12frame );
	
	if( wrotey != nbytesy || wroteuv != nbytesuv )
	  {
	    fprintf(stderr, "Didn't write all bytes...\n");
	    exit(1);
	  }
      } //if is_open()
    else
      {
	std::fprintf(stderr, "ERROR: trying to write to not-open mockDepthVideoWriter\n");
	exit(1);
      }
    return;
  }
}; //end mockDepth







//REV: make depth saver to YUV420P (?)
//REV: ensure FFMPEG does no conversion? What is NV12 and shit?

//1) Get U16 which is of WxH. Turn it into a single 1D array, which will be 2*W*H.
//2) "Mock" it as a

//I can just pass it nv12 I guess...
// Easiest is just store the luminance data in the dense part at double the width, and add "empty" chroma? Waste of space.
// 4:2:0 basically means that we use 1 byte of each color for every 4 bytes of lum (so 6 bytes for 4 pixels)

// So, for an image of size WxH, it will take up WxH * 1.5 total pixels. Note, U and V will be interleaved in nv12?
// REV: waste 50% space but doesn't matter, try it out at least! Every 2 pixels, then re-interpret as a cv::Mat of depth 16...

// REV: anyways, create an NV12 format image of the correct type, with correct alignment,
// REV: for sound, output it as like, an AVI file or some shit (PCI?) with time-stamps?
// Time-stamp and #of bytes, etc.. Just store it :)

//https://trac.ffmpeg.org/wiki/Hardware/VAAPI
// The encoders only accept input as VAAPI surfaces. If the input is in normal memory, it will need to be uploaded before giving the frames to the encoder - in the ffmpeg utility, the hwupload filter can be used for this. It will upload to a surface with the same layout as the software frame, so it may be necessary to add a format filter immediately before to get the input into the right format (hardware generally wants the nv12 layout, but most software functions use the yuv420p layout).

#ifdef USEH5


//REV: is H5 access unsave from opencv? FUCK
//REV: compresses INTRA-frame. I could "buffer" frames, and compress interframe too, with chunksize.
struct h5VideoWriter
{
  const std::string dataset_name = "/data";
  cv::Ptr<cv::hdf::HDF5> h5io;
  std::string fullpath;
  uint64_t frameidx=0;
  static const int n_dims = 3; 
  int chunks[n_dims];
  const int compresslevel = 3; //I should tune this based on current CPU speed etc... //0 - 9, 9 is highest. (GZIP)
  const int chunksize = 1;

  //REV: I require HDF5 library to be compiled with --threadsafe...
  
  //REV: require threadsafe version -- hdf5-openmpi?
  static std::mutex h5mu; //REV: because, system library (context) has global variables...WHAT.THE.FUCK.
  
  h5VideoWriter()
    : frameidx(0)
  {
  }

  ~h5VideoWriter()
  {
    close();
  }

  void close()
  {
    const std::lock_guard<std::mutex> lock(h5mu);
    if( h5io ) //assume this checks pointer is legal or not...?
      {
	
	h5io->close();
      }
    h5io.release(); //should set to null...if no one else is point to target?
  }

  bool is_open()
  {
    return !(nullptr==h5io);
  }
  
  bool open( const std::string& fname )
  {
    const std::lock_guard<std::mutex> lock(h5mu);
    if( !is_open() )
      {
	fullpath = fname;

	h5io = cv::hdf::open(fullpath);
	if( h5io )
	  {
	    return true;
	  }
      }
    return false;
  }

  void _init( const cv::Mat&  mat )
  {
    const std::lock_guard<std::mutex> lock(h5mu);

    //REV: is hlexists thread-safe?
    if(!h5io->hlexists(dataset_name))
      {
	fprintf(stdout, "INITIALIZING DATASET (HDF5) (%s)\n", fullpath.c_str());
	
	const int dsdims[n_dims] = { cv::hdf::HDF5::H5_UNLIMITED,
	  mat.size().height, 
	  mat.size().width 
	};
	
	if( frameidx != 0 )
	  {
	    fprintf(stderr, "Huh, fidx!=0\n");
	    exit(1);
	  }
	chunks[0]=chunksize;
	chunks[1]=mat.size().height;
	chunks[2]=mat.size().width;

	{
	  
	  h5io->dscreate( n_dims,
			  dsdims,
			  mat.type(),
			  dataset_name,
			  //cv::hdf::HDF5::H5_NONE,
			  compresslevel,
			  chunks );
	}
	fprintf(stdout, "h5VideoWriter (%s): Created dataset [%s]\n", fullpath.c_str(), dataset_name.c_str() );
      }
  }

  bool write( const cv::Mat& mat )
  {
    if( !is_open() )
      {
	return false;
      }

    if( false == mat.isContinuous() )
      {
	fprintf(stderr, "Mat is not continuous (h5 writer)\n");
	exit(1);
      }
    
    _init( mat );
    
    //fprintf(stdout, "Insert (%s). Framen= [%d] (dims input = [%d])\n", fullpath.c_str(), frameidx, mat.dims);
    
#if DEBUG_LEVEL > 0
    fprintf(stdout, "Insert. Framen= [%d] (dims input = [%d])\n", frameidx, mat.dims);
    for( int a=0; a<mat.dims; ++a)
      {
	fprintf(stdout, "Dim [%d]: [%d]\n", a, mat.size[a] );
      }
#endif
    
    std::vector<int> newshape = { 1, mat.size().height, mat.size().width };

    const int nchan=0; //0 means leave channels the same
    cv::Mat mat1d = mat.reshape( nchan, newshape );
    
    int offset[n_dims] = { frameidx, 0, 0 }; //REV: wait...this works? Should I not set offset to frameidx*w*h?
    
    {
      //REV: what the fuck system-level globals of HDF5?!
      const std::lock_guard<std::mutex> lock(h5mu);
      h5io->dsinsert( mat1d, dataset_name, offset );
    }   
    ++frameidx;
    
    return true;
  }
  
}; //h5VideoWriter

//REV: don't need to declare static?
std::mutex h5VideoWriter::h5mu;


struct h5VideoReader
{
  cv::Ptr<cv::hdf::HDF5> h5io=nullptr;
  const std::string dataset_name = "/data";
  std::string fullpath;
  static const int n_dims = 3; 
  const int chunks[n_dims] = { 1, 1, 1 };
  
  h5VideoReader()
  {
  }

  ~h5VideoReader()
  {
    close();
  }

  void close()
  {
    if( h5io ) //assume this checks pointer is legal or not...?
      {
	h5io->close();
      }
    h5io.release(); //should set to null...if no one else is point to target?
  }

  bool is_open()
  {
    return !(nullptr==h5io);
  }
  
  bool open( const std::string& fname )
  {
    if( !is_open() )
      {
	fullpath = fname;
	//REV: Need to check in FS first for file's existence...
	
	h5io = cv::hdf::open(fullpath); //REV: fuck it will make it if it does not exist?

	if( h5io ) { return true; }
	else {return false;}
	/*if(!h5io->hlexists(dataset_name))
	  {
	    return true;
	  }
	else
	  {
	    fprintf(stderr, "Data set does not exist?\n");
	    close();
	    //Fuck it created it ?
	    return false;
	  }
	*/
      }
    else
      {
	fprintf(stderr, "Already open?!\n");
      }
    return false;
  }
  
  std::vector<cv::Mat> readall()
  {
    std::vector<cv::Mat> vec;
    if( !is_open() )
      {
	return vec;
      }

    std::vector<int> sizes = h5io->dsgetsize( dataset_name );
    size_t nsavedframes = sizes[0];
    for( size_t x=0; x<nsavedframes; ++x )
      {
	int offset[n_dims] = { x, 0, 0 };
	int counts[n_dims] = { 1, sizes[1], sizes[2] };
	cv::Mat retmat;
	h5io->dsread( retmat, dataset_name, offset, counts );
	retmat = retmat.reshape( 0, std::vector<int>( { sizes[1], sizes[2]} ) );
	vec.push_back(retmat);
      }
    return vec;
  }
}; //h5VideoReader

#endif // ifdef WITH_CV_HDF5



enum TS_TYPE
  {
    UINT64 = 0,
    FLOAT64 = 1,
    UINT32 = 2,
    FLOAT32 = 3
  };






//Put 1 char at beginning indicating type
// 0 = uint64_t (native?)
// 1 = float64_t (native?)
template <typename T>
struct tsWriter
{
private:
  std::ofstream fp;
  std::string fullpath;

public:
  tsWriter() { }

  ~tsWriter() { close(); }
  
  void close()
  {
    if( fp.is_open() )
      {
	fp.close();
      }
  }
  
  void open( const std::string& fname )
  {
    fullpath = fname;

    if( fp.is_open() )
      {
	fprintf(stderr, "Wtf fp already open ts writer? [%s]\n", fname.c_str());
	exit(1);
      }
    
    fp.open(fullpath, std::ofstream::binary );
    
    uint8_t tagbyte;
    if( std::is_same<T, double>::value )
      {
	tagbyte = (uint8_t)(TS_TYPE::FLOAT64);
      }
    else if( std::is_same<T, uint64_t>::value )
      {
	tagbyte = (uint8_t)(TS_TYPE::UINT64);
      }
    else if( std::is_same<T, float>::value )
      {
	tagbyte = (uint8_t)(TS_TYPE::FLOAT32);
      }
    else if( std::is_same<T, uint32_t>::value )
      {
	tagbyte = (uint8_t)(TS_TYPE::UINT32);
      }
    else
      {
	fprintf( stderr, "Not recognized type for ts writer\n");
	exit(1);
      }

    fp.write( reinterpret_cast<char*>(&tagbyte), sizeof(tagbyte) );
  } //end open
  
  void write( const T& val )
  {
    //REV: stupid to have so many interpret casts...
    fp.write( reinterpret_cast<char*>(const_cast<T*>(&val)), sizeof(val) );
  }
    
};







//Wrapper around cvVideoWriter
struct cvVideoWriter
{
private:
  std::string fullpath;
  cv::VideoWriter vw;
  int fps;
  
  
public:
  cvVideoWriter()
    : fps(-1)
  { }
  
  ~cvVideoWriter()
  {
    close();
  }
  
  bool is_open( )
  {
    return vw.isOpened();
  }
  
  bool open( const std::string& fpath, const cv::Mat& mat, const int targfps )
  {
    fprintf(stdout, "Starting *OPEN* of cvVW (path=%s)\n", fpath.c_str() );
    if( is_open() )
      {
	fprintf(stdout, "Uh, wtf?\n");
	exit(1);
      }

    int fourcc = cv::VideoWriter::fourcc('F', 'F', 'V', '1');
    //int fourcc = cv::VideoWriter::fourcc('X', '2', '6', '5');
    //int fourcc = cv::VideoWriter::fourcc('X', '2', '6', '4');
    //int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');
    //int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '5');
    
    fullpath = fpath;
    fps = targfps;
    
    bool iscolor = mat.channels() > 1 ? true : false;
    vw.open( fullpath, fourcc, fps, mat.size(), iscolor );
      
    return true;
  }
  
  void close()
  {
    if( is_open() )
      {
	vw.release();
      }
  }
  
  void write(  const cv::Mat& m )
  {
    if( is_open() )
      {
	vw.write(m);
      }
    else
      {
	std::fprintf(stderr, "ERROR: trying to write to not-open cvVideoWriter\n");
	exit(1);
      }
    return;
  }
}; //end ffVideoWriter struct
