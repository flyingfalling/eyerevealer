#pragma once

#include <CompareVersion/CompareVersion.hpp>

#define FRSKIP 150
#define DEBUG_LEVEL 11
#define HTTP_DEBUG_LEVEL 0

//Use HDF5 (with global static mutex in h5VideoWriter) for raw saving depth
//#define USEH5
#define USEMOCKDEPTH

//resize all depth images by 2... (for raw saving?)
//REV: fullsize, with ffmpeg with wasted u/v channels = 
#define HALFDEPTH
#define HALFDEPTH_INTERPO cv::INTER_AREA


//Use opencv VideoWriter (vs ghetto piping ffmpeg version...)
//REV: video writer?
#define VW 0

//Use ffVideoWriter (via pipes) -- uses VAAPI
#define FFVW 1

//Use ffmpeg (avcodec etc.) i.e. C++ implementation (currently software x265)
//hevc_cvmat_vid_filewriter
#define FW 2

#define ENCTYPE FFVW


//Options to save raw other than just color and depth (and IMU/eye/etc. data)

// For example, save infrared camera frames or disparity used to calculate depth

// ITD depth camera stereo options
#define ITD_SAVEMONOR
#define ITD_SAVEDISPAR


// REALSENSE depth cam  options
#define RS_SAVEINFRAR
#define RS_SAVEINFRAL


//If undefined, realtime display will show multiple gaze points (80 msec worth before frame?)
//#define SINGLE_GAZE

#define STR_(x) #x
#define STR(x) STR_(x)

#define FFMPEG_VERSION (STR(FFMPEG_LIB_VERSION))

//REV; lol this can't happen in the preprocessor anyways so what's the point?
//REV: I have no way to comp strings or double anyways...fuck
//REV: use different specific things like channel_layout_changed
static bool FFMPEG_VERSION_GEQ( const std::string& v )  { return ( CompareVersion(v) <= CompareVersion(FFMPEG_VERSION) ); }
static bool FFMPEG_VERSION_G( const std::string& v )  { return ( CompareVersion(v) < CompareVersion(FFMPEG_VERSION) ); }
static bool FFMPEG_VERSION_LEQ( const std::string& v )  { return ( CompareVersion(FFMPEG_VERSION) <= CompareVersion(v) ); }
static bool FFMPEG_VERSION_L( const std::string& v )  { return ( CompareVersion(FFMPEG_VERSION) < CompareVersion(v) ); }
static bool FFMPEG_VERSION_EQ( const std::string& v )  { return ( CompareVersion(FFMPEG_VERSION) == CompareVersion(v) ); }




//////////// TIME TYPES
typedef double rstime_t;
typedef double itdtime_t;
typedef std::uint64_t tobii2time_t;
typedef std::uint64_t tobii3time_t;
typedef std::uint64_t pupilinvistime_t;


//REV: do not put line commends in defines it will copy in place lol
//no limit
const uint64_t MAX_MBUF_ELEMSIZE = 0;

//500 MB
const uint64_t MAX_MBUF_BYTESIZE = 200000000;

//1 byte
const uint64_t DEFAULT_MBUF_PERELEMSIZE_BYTES = 1;

//no limit
const uint64_t MAX_TBUF_ELEMSIZE = 0;

//1000 MB
const uint64_t MAX_TBUF_BYTESIZE = 200000000;

//1 byte
const uint64_t DEFAULT_TBUF_PERELEMSIZE_BYTES = 1;



//////////// MACROS

#define GET_CAST( obj, op, paytype, timetype )  \
  std::any_cast< std::shared_ptr<timed_buffer<paytype,timetype>> >( obj.get_output( #op ) );

#define JOIN( j )   if( j.joinable() ) { j.join(); }

#define ERASEFINISHED( tag ) for( auto it= tag##savet.begin(); it!=tag##savet.end(); ) \
    {									\
      if( !((*it)->working)() )						\
	{								\
	  fprintf(stdout, "WILL ERASE! [%s]\n", #tag);			\
	  (*it)->t.join(); /*REV: must wait for join*/			\
	  it = tag##savet.erase(it);					\
	  fprintf(stdout, "ERASED! [%s]\n", #tag);			\
	}								\
      else								\
	{								\
	  ++it; /*fprintf(stdout, "STILL WORKING! [%s]\n", #tag);*/	\
	}								\
    }



