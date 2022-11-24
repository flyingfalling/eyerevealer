#pragma once

#include <sstream>
#include <vector>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <condition_variable>

std::string bytevec_to_str( const std::vector<std::byte>& vec )
{
  std::string str( reinterpret_cast<char*>( const_cast<std::byte*>(vec.data()) ), vec.size() );
  return str;
}


//REV: does ***NOT*** include null terminator!!!!!!
std::vector<std::byte> str_to_bytevec( const std::string& msg )
  {
    //REV: strings contain ending null values?
    //std::string.end() returns iter to past last character. std::string does not contain null itself?
    std::vector<std::byte> vec( msg.size() );
    size_t cnt=0;
    std::transform( msg.begin(), msg.end(), vec.begin(),
		    [&] (char c) { ++cnt; return std::byte(c); });
    if( cnt != msg.size() )
      {
	std::fprintf(stderr, "WTF std::string::size() and std::string::begin() - std::string::end() are different?! (implicit null?)\n");
	exit(1);
      }
    //vec.back() = std::byte(0);
    
    auto result = bytevec_to_str( vec );
    //std::fprintf(stdout, "My result: [%s]\n", result.c_str() );
    
    return vec;
  }


std::string pop_line_from_ss( std::istringstream& iss ) //std::vector<std::uint8_t>& vec )
{
  std::string line;
  //std::string str = std::string(reinterpret_cast<std::vector<char>>(vec));
  //std::istringstream iss( std::string(vec.begin(), vec.end()) ); //I hope this won't copy it!
  std::getline( iss, line );
  return line;
}

bool shouldloop( const std::atomic<bool>& loop )
{
  return (true == loop.load(std::memory_order_relaxed));
}

struct loopcond
{
  std::atomic<bool> loop;
  std::condition_variable cv;
  std::mutex mu;
  const std::uint64_t wakeuptime_ns;
  const std::string tag;

    
  loopcond( const std::string& mytag="", const std::uint64_t _wakeuptime_ns=1e9 )
    : loop(true), wakeuptime_ns(_wakeuptime_ns), tag(mytag)
  {  }
  
  bool operator()( ) const
  {
    return shouldloop(loop);
  }
      
  void stop()
  {
    //std::fprintf(stderr, "\n\n SOMEONE SET LOOP COND [%s] FALSE!\n\n\n", tag.c_str());
    loop = false;
    cv.notify_all();
  }

  void start()
  {
    loop = true;
    cv.notify_all();
  }
  
  void sleepfor( const double& secs )
  {
    Timer t;
    //fprintf(stdout, "Will sleep for [%lf]\n", secs );
    std::unique_lock<std::mutex> lk( mu );
    while( shouldloop(loop) &&
	   t.elapsed() < secs )
    {
      cv.wait_for( lk, std::chrono::nanoseconds( wakeuptime_ns ) );
    }
  }
  
};



bool strcontains( const std::string& mystring, const std::string& probe )
{
  return (mystring.find(probe) != std::string::npos);
}



//This will wait UNTIL lambda is TRUE
//Will loop while lambda returns FALSE;
void wait_or_timeout_cond( auto&& lambda, loopcond& loop, std::condition_variable& cv, std::mutex& mu, const uint64_t sleep_ns)
{
  std::unique_lock<std::mutex> lk( mu );
  while( !lambda() && loop() ) //This will resolve to:   !(saveloop && buf>0) && loop .. I want to specify the condition that must be
    //MET BEFORE I CONTINUE
    {
      cv.wait_for( lk, std::chrono::nanoseconds( sleep_ns ) );
    }
}

template <typename T>
std::string tostr( const T v )
{
  std::stringstream ss;
  ss << v;
  return ss.str();
}




struct timed_mat
{
public:
  cv::Mat mat;
  double timestamp; //seconds

  timed_mat(const cv::Mat& m, const double ts )
    : mat(m), timestamp(ts)
  {}
  
  timed_mat()
  {}
};


std::string cvtype2str(int type)
{
  std::string r;
  
  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);
  
  switch ( depth )
    {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
    }
  
  r += "C";
  r += (chans+'0');
  
  return r;
}

std::vector<std::string> split_string( const std::string& input, const char delim)
{
  std::vector<std::string> result;
  auto e = input.end();
  auto i = input.begin();
  while (i != e)
    {
      i = std::find_if_not(i, e, [delim](char c) { return c == delim; });
      if (i == e) { break; }
      auto j = std::find(i, e, delim);
      result.emplace_back(i, j);
      i = j;
    }
  return result;
}


#include <algorithm>
#include <string>
#include <iostream>

const std::string forbiddenChars = "\\/:?\"<>|()[]*^$@ ";

static char clear_forbidden_chars(char toCheck)
{
  if(forbiddenChars.find(toCheck) != std::string::npos)
    {
      return '_';
    }

  return toCheck;
}

std::string clean_fname( const std::string& fname )
{
  std::string result = fname;
  std::transform( result.begin(), result.end(), result.begin(), clear_forbidden_chars );
  return result;
}



bool createdir( const std::string& fullpath )
{
  bool exists=std::filesystem::exists( fullpath );
  if( exists )
    {
      std::fprintf( stderr, "WARNING/ERROR, your requested path [%s] already exists (would overwrite)\n", fullpath.c_str() );
      return false;
    }
  else
    {
      bool created = std::filesystem::create_directory(fullpath);
      if( !created )
	{
	  std::fprintf(stderr, "ERROR, failed to create dir [%s]\n", fullpath.c_str());
	}
      return created;
    }
}

std::string join_strings_vec( const std::vector<std::string>& v, const std::string& c )
{
  std::string s;
  
  for (std::vector<std::string>::const_iterator p = v.begin(); p != v.end(); ++p)
    {
      s += *p;
      if ( p != v.end() - 1)
	{ s += c; }
    }

  return s;
}

std::string get_datetimestamp_now()
{
  std::time_t rawtime;
  std::tm* timeinfo;
  char buffer[80];
      
  std::time(&rawtime);
  timeinfo = std::localtime(&rawtime); //REV: not thread-safe...but whatever?
      
  std::strftime(buffer, 80 ,"%Y-%m-%d-%H-%M-%S", timeinfo);
  std::puts(buffer);
  return std::string(buffer);
}

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

std::string get_user_home()
{
  const char *homedir;
  if((homedir = getenv("HOME")) == NULL)
    {
      homedir = getpwuid(getuid())->pw_dir;
    }
  return std::string(homedir);
}


std::string get_DRI_render_node()
{
  const char* node;
  if((node = getenv("DRI_RENDER_NODE")) == NULL)
    {
      //e.g. /dev/dri/renderD128
      //check by doing vainfo --display drm --device /dev/dri/renderD128
      // if it errors, bad. If not, good (use that one -- will print lots of info)
      node = "/dev/dri/renderD128";
    }
  return std::string(node);
}


std::string av_error_str( const int averr )
{
  std::string ret;
  ret.resize(AV_ERROR_MAX_STRING_SIZE);
  int success = av_strerror( averr, static_cast<char*>(ret.data()), ret.size() );
  if( 0 == success )
    {
      return ret;
    }
  else
    {
      fprintf(stderr, "REV: wtf unknown AVERROR [%d]\n", averr);
      return std::string();
    }
}


std::vector<json> parse_multiline_json( const std::string& datstr )
{
  std::vector<json> result;
  
  
  std::istringstream iss(datstr);
  std::string line = pop_line_from_ss( iss );
  while( !line.empty() )
    {
      auto j = json::parse( line );
      
      result.push_back(j);
      
      line = pop_line_from_ss( iss );
    }
  return result;
}

#ifdef WITH_REALSENSE
static uint64_t rsframesize( const rs2::frame& fr )
{
  return (uint64_t)fr.get_data_size();
}
#endif


static uint64_t avpktsize( const AVPacket* pkt )
{
  if( pkt )
    {
      return pkt->size;
    }
  else
    {
      fprintf(stderr, "REV: avpktsize of null ptr?!\n");
      return 0;
    }
}

static uint64_t avframesize_vid( const AVFrame* frame )
{
  //return pkt.size;
  //AVClass* cl = avcodec_get_class( frame );
  //avcodec_get_frame_class();
  if( !frame )
    {
      fprintf(stderr, "Video AV Frame is null, no size\n");
      return 0;
    }
  //int sz = av_image_get_buffer_size( (AVPixelFormat)(frame->format), frame->width, frame->height, frame->align);
  const int align=32;
  int sz = av_image_get_buffer_size( (AVPixelFormat)(frame->format), frame->width, frame->height, align);
  return sz;
}

static uint64_t avframesize_aud( const AVFrame* frame )
{
  if( !frame )
    {
      fprintf(stderr, "Audio AV Frame is null, no size\n");
      return 0;
    }

  //int64_t sz = frame->ch_layout.nb_channels * frame->nb_samples * frame->bytes
  const int align=32;
#ifdef FFMPEG_VERSION_GEQ_5 //( "5.0" )
  int nchannels=frame->ch_layout.nb_channels;
#else
  int nchannels=av_get_channel_layout_nb_channels(frame->channel_layout);
#endif
  
  int sz = av_samples_get_buffer_size( (int*)(frame->linesize), nchannels, frame->nb_samples, (AVSampleFormat)(frame->format), align );
  #
  return sz;
}

template <typename T>
static uint64_t vecvecbytesize( const std::vector<std::vector<T>>& vec )
{
  uint64_t size=0;
  for( size_t x=0; x<vec.size(); ++x )
    {
      for( size_t y=0; y<vec[x].size(); ++y )
	{
	  size += sizeof(T);
	}
    }
  return size;
}

static uint64_t cvmatsize( const cv::Mat& mat )
{
  if( mat.isContinuous() )
    {
      return mat.total() * mat.elemSize();
    }
  else
    {
      return mat.step[0] * mat.rows;
    }
}




#ifdef USE_LIBCURL

//libcurl...
#include <curl/curl.h>

std::string encode_url( const std::string& url )
{
  std::string result;
  CURL *curl = curl_easy_init();
  if(curl)
    {
      char* output = curl_easy_escape(curl, url.c_str(), url.size());
      if(output)
	{
	  //printf("Encoded: %s\n", output);
	  result = std::string(output).c_str();
	  curl_free(output);
	}
      curl_easy_cleanup(curl);
    }
  
  return result;
}
#endif
