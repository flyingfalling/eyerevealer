//REV: removes it if disconnects -- but stream is still "true" so shit fucked?

//REV: TODO:
//   (all sorts of things --  sensor fusion, between device calibration, timing etc.)

// 1) If guys are not mbuf -> tbuf fast enough it will fill up (maybe when saving/battery?)
// 2) print msec time in corner of each window
// 3) auto-layout (and size) windows
// 4) auto-resize window content when user resizes it (or disallow resize?)



//REV: includes #defines etc for options from CMAKE
#include <rteye2_config.hpp>

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

//comp version
//#include <CompareVersion/CompareVersion.hpp>

//STDUUID
#define UUID_SYSTEM_GENERATOR
#include <stduuid/uuid.h> //needs c++20 for fucking span


//FFMPEG
#ifdef __cplusplus
extern "C"{
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
}
#endif

#ifdef WITH_REALSENSE
//REV: need this here for utilities...
#include <librealsense2/rs.hpp>
#endif


//OPENCV
#include <opencv2/opencv.hpp>
//#include <opencv2/hdf/hdf5.hpp>



//For the includes below lol
using namespace nlohmann;


//My files
#include <rteye2_defines.hpp>

#include <Timer.hpp>

#include <utilities.hpp>

#include <vec3f.hpp>

#include <mutexed_buffer.hpp>
#include <timed_buffer.hpp>
#include <socket_udp6.hpp>
#include <ffVideoWriter.hpp>


// MDNS CPP (zero conf service discovery...)
//#include <mdns_cpp/mdns.hpp>

#include <tobii2rest.hpp>
#include <tobii3rest.hpp>

#include <mpegts_parser.hpp>

#include <aud_encoder.hpp>

#include <saver_thread_info.hpp> //depends timed buf
#include <looper.hpp> //contains producer?

#include <device_stream_publisher.hpp>
#include <device_stream_consumer_parser.hpp>

//ITD
#ifdef WITH_ITD
#include <itd_wrapper.hpp>
#include <itd_usb_receiver.hpp> //depends looper, mutex buf
#include <itd_parser.hpp>
#endif

//REALSENSE
#ifdef WITH_REALSENSE
//#include <librealsense2/rs.hpp>
#include <realsense_usb_receiver.hpp> //depends looper, realsense, mutex buf
#include <realsense_parser.hpp>
#endif


#include <zeroconf_service_discoverer.hpp>
#include <pupilinvis_interface.hpp>

#include <avails.hpp> //depends looper, itd_wrapper...

#include <udp_regular_sender.hpp> //depends socket_udp6, looper
#include <udp_receiver.hpp> //depends socket_udp6, looper

#include <tobii2_data_parser.hpp> //depends, looper, timed_buffer, mutexted_buffer
#include <tobii2_video_parser.hpp> //depends mpegts_parser, looper, timed_buffer, mutexted_buffer
#include <tobii2_streaming_conn.hpp> //depends tobii2 video/data parser.

#include <tobii3_parser.hpp>


//With glfw, imgui.

// Allow saving window position, size, layout (make things easier for myself).

// Allow saving connected devices? (but it needs to wait to see if they are available)

// Use that basic file dialog thing to set where to save to?
// -> For save_raw, set it to SERIAL or something (Set it when I make the parser?)

//config (always the same config file? Or can specify it when I run?) -- OK


//Minimum:
// -- search for and display each different "type" of input (conn avail) on left panel (?). (allow collapse?)
// -- allow "switch" for connecting to device (= start streaming any of its streams)
// -- Switch on certain streams to view
// -- "overlay eye position" (e.g. from eyetracking source (tobii))
// -- button to calibrate tobii (on tobii stream thing). (print subject name calib name etc.) "CALIBRATED"

// -- Button to "start saving raw" (specify source directory?)
// -- "start saving ALL raw" (i.e. all currently streaming devices).


// -- ux_window  (whole window, glfw context etc.?)
// -- device panel (left side) --
// -- stream viewer (right side)


// -- first step -- connect in background (automatically), and show all streams from CV in viewport type thing.

// -- each "stream" (individual output) can have its own display thing -- with a width/height.
// --      It will lay them out in columns/rows.

// Does ordering matter (cluster streams?)
// -> colorize depth? log to distance (dist+1)?, where dist [0,infinity) (or use min dist?)
// Write out "parameters" of realsense? (depth units, etc.)

//Helper for OPENGL bindings?
#include <glad/glad.h>

//GLFW (OS window/mouse control)
#include <GLFW/glfw3.h>

//IMGUI (immediate mode GUI)
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <rect.hpp>


static double lasttime=0;

static double BIGNUM=66666666666;


// Helper function to get window rect from GLFW
rect get_window_rect(GLFWwindow* window)
{
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  int xpos, ypos;
  glfwGetWindowPos(window, &xpos, &ypos);
  
  return{ (float)xpos, (float)ypos,
    (float)width, (float)height };
}

// Helper function to get monitor rect from GLFW
rect get_monitor_rect(GLFWmonitor* monitor)
{
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  int xpos, ypos;
  glfwGetMonitorPos(monitor, &xpos, &ypos);

  return{ (float)xpos, (float)ypos,
    (float)mode->width, (float)mode->height };
}

// Select appropriate scale factor based on the display
// that most of the application is presented on
int pick_scale_factor(GLFWwindow* window)
{
  auto window_rect = get_window_rect(window);
  int count;
  GLFWmonitor** monitors = glfwGetMonitors(&count);
  if (count == 0) return 1; // Not sure if possible, but better be safe
  
  // Find the monitor that covers most of the application pixels:
  GLFWmonitor* best = monitors[0];
  float best_area = 0.f;
  for (int i = 0; i < count; i++)
    {
      auto int_area = window_rect.intersection(get_monitor_rect(monitors[i])).area();
      if (int_area >= best_area)
	{
	  best_area = int_area;
	  best = monitors[i];
	}
    }

  int widthMM = 0;
  int heightMM = 0;
  glfwGetMonitorPhysicalSize(best, &widthMM, &heightMM);

  // This indicates that the monitor dimentions are unknown
  if (widthMM * heightMM == 0) return 1;

  // The actual calculation is somewhat arbitrary, but we are going for
  // about 1cm buttons, regardless of resultion
  // We discourage fractional scale factors
  float how_many_pixels_in_mm =
    get_monitor_rect(best).area() / (widthMM * heightMM);
  float scale = sqrt(how_many_pixels_in_mm) / 5.f;
  if (scale < 1.f) return 1;
  return (int)(floor(scale));
}

void terminate(int errorCode)
{
  std::cout << "Closing application";
  //Close GLFW
  glfwTerminate();
  //Exit
  exit(errorCode);
}


/**
 * A callback function for GLFW to execute when an internal error occurs with the
 * library.
 */
void error_callback(int error, const char* description)
{
  fprintf(stderr, "Error: %s\n", description);
}


void setStyle()
{
  ImGuiStyle* style = &ImGui::GetStyle();
  ImVec4* colors = style->Colors;

  colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.53f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.81f, 0.83f, 0.81f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.93f, 0.65f, 0.14f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

  style->FramePadding = ImVec2(4, 2);
  style->ItemSpacing = ImVec2(10, 2);
  style->IndentSpacing = 12;
  style->ScrollbarSize = 10;

  style->WindowRounding = 4;
  style->FrameRounding = 4;
  style->ScrollbarRounding = 6;
  style->GrabRounding = 4;

  style->WindowTitleAlign = ImVec2(1.0f, 0.5f);

  style->DisplaySafeAreaPadding = ImVec2(4, 4);

}







// Function turn a cv::Mat into a texture, and return the texture ID as a GLuint for use
static void reloadTexture( const GLuint textureID, const cv::Mat& mat )
{
  //REV: need to keep track of old size -- I can't change it..
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Set texture interpolation methods for minification and magnification
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Set texture clamping method
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  
  GLenum inputColourFormat = GL_BGR;
  if (mat.channels() == 1)
    {
      inputColourFormat = GL_LUMINANCE;
    }

  if( false == mat.isContinuous() )
    {
      fprintf(stderr, "Is not continuous!\n");
      exit(1);
    }
  
  // Create the texture
  glTexImage2D(GL_TEXTURE_2D,     // Type of texture
	       0,                 // Pyramid level (for mip-mapping) - 0 is the top level
	       GL_RGB,            // Internal colour format to convert to
	       mat.cols,          // Image width  i.e. 640 for Kinect in standard mode
	       mat.rows,          // Image height i.e. 480 for Kinect in standard mode
	       0,                 // Border width in pixels (can either be 1 or 0)
	       inputColourFormat, // Input image format (i.e. GL_RGB, GL_RGBA, GL_BGR etc.)
	       GL_UNSIGNED_BYTE,  // Image data type
	       mat.ptr());
  
  
  glBindTexture(GL_TEXTURE_2D, 0);
}

//REV: this only maps uint8_t based images (i.e. can't handle UINT16, FLOAT32, etc.?
static GLuint matToTexture(const cv::Mat& mat )
{
  // Generate a number for our textureID's unique handle
  GLuint textureID;
  glGenTextures(1, &textureID);
	
  // Bind to our texture handle
  glBindTexture(GL_TEXTURE_2D, textureID);
	
  /*// Catch silly-mistake texture interpolation method for magnification
  if (magFilter == GL_LINEAR_MIPMAP_LINEAR ||
      magFilter == GL_LINEAR_MIPMAP_NEAREST ||
      magFilter == GL_NEAREST_MIPMAP_LINEAR ||
      magFilter == GL_NEAREST_MIPMAP_NEAREST)
    {
      std::cout << "You can't use MIPMAPs for magnification - setting filter to GL_LINEAR" << std::endl;
      magFilter = GL_LINEAR;
      }*/
  
  // Set texture interpolation methods for minification and magnification
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  // Set texture clamping method
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  

  if( false == mat.isContinuous() )
    {
      fprintf(stderr, "Is not continuous!\n");
      exit(1);
    }

  // Set incoming texture format to:
  // GL_BGR       for CV_CAP_OPENNI_BGR_IMAGE,
  // GL_LUMINANCE for CV_CAP_OPENNI_DISPARITY_MAP,
  // Work out other mappings as required ( there's a list in comments in main() )
  GLenum inputColourFormat = GL_BGR;
  if (mat.channels() == 1)
    {
      inputColourFormat = GL_LUMINANCE;
    }

  // Create the texture
  glTexImage2D(GL_TEXTURE_2D,     // Type of texture
	       0,                 // Pyramid level (for mip-mapping) - 0 is the top level
	       GL_RGB,            // Internal colour format to convert to
	       mat.cols,          // Image width  i.e. 640 for Kinect in standard mode
	       mat.rows,          // Image height i.e. 480 for Kinect in standard mode
	       0,                 // Border width in pixels (can either be 1 or 0)
	       inputColourFormat, // Input image format (i.e. GL_RGB, GL_RGBA, GL_BGR etc.)
	       GL_UNSIGNED_BYTE,  // Image data type
	       mat.ptr());        // The actual image data itself

  /*
  // If we're using mipmaps then generate them. Note: This requires OpenGL 3.0 or higher
  if (minFilter == GL_LINEAR_MIPMAP_LINEAR ||
      minFilter == GL_LINEAR_MIPMAP_NEAREST ||
      minFilter == GL_NEAREST_MIPMAP_LINEAR ||
      minFilter == GL_NEAREST_MIPMAP_NEAREST)
    {
      glGenerateMipmap(GL_TEXTURE_2D);
      }
  */


  //REV: Make sure to unload it...?
  glBindTexture(GL_TEXTURE_2D, 0);
  
  return textureID;
}

void delTexture( const GLuint textureID )
{
  //Destroy the last texture
  if(textureID) { glDeleteTextures(1, &textureID); }
  return;
}

/*static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}


static void resize_callback(GLFWwindow* window, int new_width, int new_height)
{
  glViewport(0, 0, _window_width = new_width, _window_height = new_height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, _window_width, _window_height, 0.0, 0.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
}


static void init_opengl(int w, int h)
{
  glViewport(0, 0, w, h); // use a screen size of WIDTH x HEIGHT
	
  glMatrixMode(GL_PROJECTION);     // Make a simple 2D projection on the entire window
  glLoadIdentity();
  glOrtho(0.0, w, h, 0.0, 0.0, 100.0);

  glMatrixMode(GL_MODELVIEW);    // Set the matrix mode to object modeling

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClearDepth(0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the window
}
*/




// 1) Layout of streaming windows (auto-layout in rows/columns, scale size based on window size).

// 2) Do side panel (list available, choose which to show -- = start streaming). Run avail_itd, avail_rs, avail_tobii etc.
//    in loop and present as options (how to know if e.g. ITD camera has IMU or not?! I'd have to query device...
//     device type doesn't offer stream (on the params[]) side.
// "available" and "running" are different? I could calibrate without "connecting"... (just send HTTP)

// 3) show/calculate eye position (tobii) -- as a stream?
// 4) *CALIBRATE* tobii (button) -- starts calibration.

// 5) "Start saving" (individual?) -- "start saving" (all currently streaming). -- auto-generate directory in default
// location?

// 6) Save layout / configuration (easy start).


//REV: simple class to wrap:
// GL texture pointer
// size? position? (use getCursorPos?) --  and info about containing window. Subtract the status/control side panel
// other stuff

//Window resize callback sets window size obviously -- also resize all active streams (and change layout...?)

//auto pos = ImGui::GetCursorScreenPos();
//auto h = ImGui::GetWindowHeight();

//setcursorscreenpos vs setcursorpos?
//ImGui::PushStyleColor(ImGuiCol_Text, text_color);
//       ImGui::Text("%s", text);
//        ImGui::PopStyleColor();
//ImGui::SetNextWindowPos({ panel_width, 0 });
//        ImGui::SetNextWindowSize({ window.width() - panel_width, panel_y });
//Keep my width/height?
//Auto-layout me?
struct stream_view_subwindow
{
  cv::Mat mat;
  GLuint texture; //REV: I need to initialize this to 0 or it will do all sorts of whacky shit?
  std::string tag;

  //REV: wtf? default argument to put it in container? :( Need "move" constructor? texture OK to move?
  stream_view_subwindow( const std::string& mytag="" )
    : texture(0), tag(mytag)
  {
  }
  
  ~stream_view_subwindow( )
  {
    delTexture( texture );
  }

  void update(const cv::Mat& m)
  {
    if( m.type() != CV_8UC3 && m.type() != CV_8UC1 )
      {
	if( CV_16UC1 == m.type() )
	  {
	    //fprintf(stdout, "Converting!\n");
	    //convert to target type? With multiplier?
	    //need to divide? Yea, saturate cast will just prevent overflow, not scale.
	    double mi, ma;
	    cv::minMaxLoc( m, &mi, &ma );
	    if( ma/256.0 > 255 )
	      {
		fprintf(stderr, "REV: error min [%lf], max [%lf] all fucked up?!\n", mi, ma );
		exit(1);
	      }
	    double scale = 1.0/256.0; //255? 256?
	    m.convertTo( mat, CV_8UC1, scale );
	  }
	else if( CV_32FC1 == m.type() )
	  {
	    fprintf(stderr, "Error can't handle 32 bit float? Convert?\n");
	    exit(1);
	  }
	else
	  {
	    
	    fprintf(stdout, "Other unknown type? [%s]\n", cvtype2str( m.type() ).c_str() );
	    exit(1);
	  }
      }
    else
      {
	mat = m.clone(); //ensure continuous.
      }

    if( !texture )
      {
	texture = matToTexture( mat ); //REV: will it work with a depth stream?!?!?! Covert to 8bit (for display only?)
      }
    else
      {
	reloadTexture( texture, mat );
      }
    return;
  }


  //REV: *place* it in the missing space (over the top...?)
  void show_in_imgui()
  {
    if( texture )
      {
	auto flags = 
	  ImGuiWindowFlags_NoSavedSettings;
	
	const double DISPLAYSCALE=0.5;
	int wid = mat.size().width * DISPLAYSCALE;
	int hei = mat.size().height * DISPLAYSCALE;
	
	//ImGui::SetNextWindowPos({ 0, 0 }); //top-left
	//ImGui::SetNextWindowSize( { wid, hei } );
	// Place the texture in an ImGui image
	ImGui::Begin(tag.c_str(), nullptr, flags);
	//REV: draw image title or some shit?
	//REV: scale image here!!
	
	ImVec2 imageSize = ImVec2( (float)wid, (float)hei );
	//Scaled the texture etc...
	//ImGui::Text("texture size = %.0f x %.0f", imageSize.x, imageSize.y);
	//ImGui::Text("image size = %d x %d", toshow1.size().width, toshow1.size().height);
	ImGui::Image((void*)(intptr_t)texture, imageSize);
	ImGui::End();
      }
  }
};





struct streaming_base
{
public:

  std::map<std::string,
	   std::shared_ptr<device_stream_consumer_parser_base>> parsers;
  
  std::map<std::string,bool> streammodes;
  std::map<std::string,std::set<std::string>> moderequirements;
  std::map<std::string,std::uint64_t> used_outputs;
  std::string id;
  
  double stream_specific_sync_offset_sec;
  Timer existence_time;
    
public:
  streaming_base( const std::string& _id )
    : id(_id), stream_specific_sync_offset_sec(0.0)
  {
    return;
  }

  void start_saving_raw( const std::string& path )
  {
    for( auto&& p : parsers )
      {
	p.second->start_saving_raw(path);
      }
  }

  double get_existence_time()
  {
    return existence_time.elapsed();
  }

  
  
  void stop_saving_raw()
  {
    for( auto&& p : parsers )
      {
	p.second->stop_saving_raw();
      }
  }
  
  static std::string parserfromname( const std::string& name )
  {
    auto str = split_string( name, ':' );
    if( str.size() > 1 )
      {
	return str[0];
      }
    else
      {
	return "";
      }
  }

  static std::string outputfromname( const std::string& name )
  {
    auto str = split_string( name, ':' );
    if( str.size() > 1 )
      {
	return str[1];
      }
    else
      {
	return str[0];
      }
  }

  static std::string namefrom( const std::string& parser, const std::string& output )
  {
    return parser + ":" + output;
  }
  
  //REV: this is last thing called in constructor of subclass!
  void init()
  {
    for( const auto& parser : parsers )
      {
	fprintf(stdout, "For parser, will get possible outputs...\n");
	
	for( const auto& op : parser.second->possible_outputs() )
	  {
	    if( used_outputs.find( namefrom(parser.first, op) ) == used_outputs.end() )
	      {
		used_outputs[namefrom(parser.first, op)] = 0;
	      }
	  }
	
	if( !parser.second )
	  {
	    fprintf(stderr, "REV: wtf parser ptr is null?!?!?!\n");
	    exit(1);
	  }
	
	
	
	//REV: wait for timebase to settle? But sometimes maybe they are not connected, so just let it happen...?
	//REV: this may not be set yet? But it should still EXIST!
	/*while( parser.second->get_timebase_hz_sec() <= 0 )
	  {
	    fprintf(stdout, "Waiting for [%s] (%s parser) timebase to settle!\n", id.c_str(), parser.first.c_str() );
	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	  }
	  fprintf(stdout, "Got timebase of parser: [%lf]\n", parser.second->get_timebase_hz_sec() );*/
      }

    //Is a single "device" (sensor? stream?) with many modes always guarantee to handle sync within itself? I guess so.
    //So, I should just have a single offset for the whole device/sensor.
    
    existence_time.reset();
  } //end void init
  
  virtual ~streaming_base() //destructor always has to be fucking virtual? WAI
  {
    for( auto&& parser : parsers )
      {
	if( parser.second ) { parser.second->stop(); }
      }
  }
  
  virtual void update( loopcond& loop )=0;

  //REV: virtual function, updates cv::Mat to render in gl context window thing (gets appropriate data from timed bufers, etc.)
  //REV: returns offset from requested time to worst offset returned time
  virtual double update_torender( std::map<std::string,timed_mat>& toupdate, const double timesec, std::mutex& mu )=0;

  //REV: virtual void to check if stream finished of its own accord (for some reason). E.g. device ran out of battery,
  // disconnected, etc...
  //REV: note should I D/C when AVAIL disappears?
  virtual bool isfinished()=0;
  
  template <typename PayT, typename TimeT>
  std::shared_ptr<timed_buffer<PayT,TimeT>> cast_output( const std::string& opname )
  {
    auto pname = parserfromname( opname );
    auto oname = outputfromname( opname );
    if( parsers.contains(pname) && parsers[pname] )
      {
	auto theany = parsers[pname]->get_output(oname);
	if( theany.has_value() )
	  {
	    return std::any_cast<std::shared_ptr<timed_buffer<PayT,TimeT>>>( theany  );
	  }
	else
	  {
	    fprintf(stderr, "What. the. fuck. [%s] output was not available...? pname (%s)  oname (%s)\n", opname.c_str(), pname.c_str(), oname.c_str() );
	    exit(1);
	  }
      }
    exit(1);
    return nullptr;
  }

  
  
  bool am_using(const std::string& op )
  {
    if( used_outputs.contains(op) )
      {
	return (used_outputs[op] > 0);
      }
    return false;
  }

  bool am_streaming(const std::string& mode )
  {
    if( !streammodes.contains(mode) ) { return false; }
    return streammodes[mode];
  }

  void disable_all_streams( )
  {
    std::set<std::string> toremove;
    for( const auto& mode : streammodes )
      {
	toremove.insert( mode.first );
      }
    update_modes( std::set<std::string>{}, toremove );
    return;
  }
  
  void update_modes( const std::set<std::string>& toadd, const std::set<std::string>& toremove )
  {
    //REV: remove and re-add happens? :/
    for( const auto& r : toremove )
      {
	if( streammodes.contains(r) )
	  {
	    if( true == streammodes[r] )
	      {
		streammodes[r] = false;

		fprintf(stdout, "Will REMOVE (%s)\n", r.c_str() );
		//REMOVE requirements
		for( const auto& removeopt : moderequirements[r] )
		  {
		    if( used_outputs[removeopt] > 0 )
		      {
			used_outputs[removeopt] -= 1;
		      }
		    else
		      {
			fprintf(stderr, "This should never happen, used outputs 0?\n");
			exit(1);
		      }
		  }
	      }
	    else
	      {
		fprintf(stderr, "Trying to remove, but streammode is FALSE?! (%s)\n", r.c_str() );
	      }
	  }
	else
	  {
	    fprintf(stderr, "wtf (%s)\n", r.c_str());
	    exit(1);
	  }
      }
    
    for( const auto& a : toadd )
      {
	if( /* !toremove.contains(a) && */
	    streammodes.contains(a)
	    && false == streammodes[a] )
	  {
	    streammodes[a] = true;
	    fprintf(stdout, "Will ADD (%s)\n", a.c_str() );
	    
	    //ADD requirements
	    for( const auto& addopt : moderequirements[a] )
	      {
		if( !used_outputs.contains(addopt) )
		  {
		    fprintf(stderr, "Big error, used outputs does not contain [%s] (you forgot parser:output?)\n", addopt.c_str());
		    exit(1);
		  }
		used_outputs[addopt] += 1;
	      }
	  }
      }
    
    //REV: activate the outputs of my parser...
    //parser->deactivate_all_outputs();
    for( const auto& op : used_outputs )
      {
	auto pname = parserfromname( op.first );
	auto oname = outputfromname( op.first );
	//fprintf(stdout, "Doing for output: [%s] (of %ld)\n", op.first.c_str(), used_outputs.size());
	if( !parsers.contains(pname) ) { fprintf(stderr, "Fail\n"); exit(1); }

	if( op.second > 0 && !parsers[pname]->isactive(oname) )
	  {
	    //REV: repeatedly activating/deactivating will mess with what is added to the timed_buffers!
	    fprintf(stdout, "[%s]: Newly activating [%s] (pname=%s) (oname=%s)\n", id.c_str(), op.first.c_str(), pname.c_str(), oname.c_str() );
	    parsers[pname]->activate_output(oname);
	  }
	else if( 0 == op.second && parsers[pname]->isactive(oname) )
	  {
	    fprintf(stdout, "[%s]: Deactivating [%s] (pname=%s) (oname=%s)\n", id.c_str(), op.first.c_str(), pname.c_str(), oname.c_str() );
	    parsers[pname]->deactivate_output(oname);
	  }
      }
    
    return;
  } //end update_modes
  
  std::string publishname( const std::string& tag ) const
  {
    return tag + " (" + id + ")";
  }

  
};





#ifdef WITH_ITD

struct streaming_itd
  : public streaming_base
{
  std::shared_ptr<ITD_wrapper> itd;
  std::shared_ptr<itd_usb_receiver> itdrecvr;
  std::shared_ptr<itd_parser> itdparser; //They can access via this->parser (base type though)

  std::shared_ptr<timed_buffer<cv::Mat,itdtime_t>> depptr;
  std::shared_ptr<timed_buffer<cv::Mat,itdtime_t>> bgrptr;
  std::shared_ptr<timed_buffer<cv::Mat,itdtime_t>> monoRptr;
  std::shared_ptr<timed_buffer<cv::Mat,itdtime_t>> disparptr;
    
  streaming_itd( loopcond& loop, std::shared_ptr<ITD_wrapper> _itdptr, const std::string& _id )
    : streaming_base(_id), itd(_itdptr)
  {
    itdrecvr = std::make_shared<itd_usb_receiver>( _itdptr );
    itdrecvr->start( loop );
    
    itdparser = std::make_shared<itd_parser>();
    itdparser->start( loop, itdrecvr );
    
    parsers[""] = itdparser; //automatically downcast (upcast?) to base type?
    
    streammodes["color"] = false;
    streammodes["depth"] = false;
    streammodes["monoR"] = false;
    streammodes["disparity"] = false;
    
    moderequirements["color"] = std::set<std::string>{":bgr"};
    moderequirements["depth"] = std::set<std::string>{":depth"};
    moderequirements["monoR"] = std::set<std::string>{":monoR"};
    moderequirements["disparity"] = std::set<std::string>{":disparity"};
    
    depptr = cast_output<cv::Mat,itdtime_t>( "depth" );
    bgrptr = cast_output<cv::Mat,itdtime_t>( "bgr" );
    disparptr = cast_output<cv::Mat,itdtime_t>( "disparity" );
    monoRptr = cast_output<cv::Mat,itdtime_t>( "monoR" );
    
    //INIT (does some setup from base class using parser->)
    init();
    
    return;
  }

  ~streaming_itd()
  {
    if( itdrecvr ) { itdrecvr->stop(); }
    if( itdparser ) { itdparser->stop(); }
  }

  void update(loopcond& loop){}
  
  bool isfinished()
  {
    if( false == itdrcvr->islooping() )
      { return true; }

    if( false == itdparser->islooping() )
      { return true; }
    
    return false;
  }
  
  double update_torender( std::map<std::string,timed_mat>& toupdate, const double timesec, std::mutex& mu )
  {
    double minoffsec=BIGNUM; //If I allow positives, we can go back "forward" right? I.e. skip ahead. Yay.
    
    //Update timebases (if they are not already set)
    //REV: Note constructor will not complete until they are set so... :)
    depptr->set_timebase_hz_sec( itdparser->get_timebase_hz_sec() );
    bgrptr->set_timebase_hz_sec( itdparser->get_timebase_hz_sec() );
    disparptr->set_timebase_hz_sec( itdparser->get_timebase_hz_sec() );
    monoRptr->set_timebase_hz_sec( itdparser->get_timebase_hz_sec() );
    
    //double itd_timeoffset_sec = -0.5;
    stream_specific_sync_offset_sec = -0.5;
    double sample_timesec = timesec + stream_specific_sync_offset_sec;
      //itd_timeoffset_sec;
    
    for( const auto& mode : streammodes )
      {
	const std::string name = publishname(mode.first);
	
	if( "depth"==mode.first )
	  {
	    //REV: check if any others are zeroed...if so use them.
	    depptr->rezero_time( bgrptr );
	    depptr->rezero_time( disparptr );
	    depptr->rezero_time( monoRptr );
	    bool zeroed = depptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = depptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    double sectime = depptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    //fprintf(stdout, "Retrieved frame from time [%lf] (target %lf)\n", sectime, timesec );
		    //Colorize it...?
		    cv::Mat prettydepth;
		    double scale = 1.0/256.0; //scale from its max? went from (0,255) to (0,256*256-1)
		    
		    val.value().payload.convertTo( prettydepth, CV_8UC1, scale );
		    cv::applyColorMap( prettydepth, prettydepth, cv::COLORMAP_JET ); //REV: must take different map?
		    toupdate[name] = timed_mat( prettydepth,
						sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		
	      }
	  }
	else if( "color"==mode.first )
	  {
	    bgrptr->rezero_time( depptr );
	    bgrptr->rezero_time( disparptr );
	    bgrptr->rezero_time( monoRptr );
	    bool zeroed = bgrptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = bgrptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore,&d1,&d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    toupdate[name] = timed_mat( val.value().payload,
						bgrptr->clocktime_sec_of_timestamp(val.value().timestamp).value() );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else if( "disparity"==mode.first )
	  {
	    disparptr->rezero_time( depptr );
	    disparptr->rezero_time( bgrptr );
	    disparptr->rezero_time( monoRptr );
	    bool zeroed = disparptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = disparptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    toupdate[name] = timed_mat( val.value().payload,
						disparptr->clocktime_sec_of_timestamp(val.value().timestamp).value() );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else if( "monoR"==mode.first )
	  {
	    monoRptr->rezero_time( depptr );
	    monoRptr->rezero_time( bgrptr );
	    monoRptr->rezero_time( disparptr );
	    bool zeroed = monoRptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = monoRptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    toupdate[name] = timed_mat( val.value().payload,
						monoRptr->clocktime_sec_of_timestamp(val.value().timestamp).value() );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else
	  {
	    fprintf(stderr, "Unknown stream mode?! Impossible\n");
	    exit(1);
	  }
      } //end for mode in streammodes

    if( minoffsec >= BIGNUM ) { minoffsec = timesec; }
    return minoffsec;
  } //end update_torender
  
  
}; //end streaming_itd 

#endif



#ifdef WITH_REALSENSE

struct streaming_rs
  : public streaming_base
{
  std::shared_ptr<realsense_usb_receiver> rsrecvr;
  std::shared_ptr<realsense_parser> rsparser; //They can access via this->parser (base type though)
  
  std::shared_ptr<timed_buffer<cv::Mat,rstime_t>> depptr;
  std::shared_ptr<timed_buffer<cv::Mat,rstime_t>> bgrptr;
  std::shared_ptr<timed_buffer<cv::Mat,rstime_t>> infraRptr;
  std::shared_ptr<timed_buffer<cv::Mat,rstime_t>> infraLptr;
  //std::shared_ptr<timed_buffer<vec3f,rstime_t>> gyroptr;
  //std::shared_ptr<timed_buffer<vec3f,rstime_t>> accelptr;

  //REV: _id is the serial number...?
  //REV: id must be unique? I.e. not include...? Huh, I should put serial number too?
  streaming_rs( loopcond& loop, const std::string& sn, const std::string& _id )
    : streaming_base(_id)
  {
    bool withlaser=false;
    rsrecvr = std::make_shared<realsense_usb_receiver>( sn, withlaser );
    rsrecvr->start( loop );
    
    rsparser = std::make_shared<realsense_parser>();
    rsparser->start( loop, rsrecvr );
    
    parsers[""] = rsparser; //automatically downcast (upcast?) to base type?
    
    streammodes["color"] = false;
    streammodes["depth"] = false;
    streammodes["infraR"] = false;
    streammodes["infraL"] = false;
    
    moderequirements["color"] = std::set<std::string>{":bgr"};
    moderequirements["depth"] = std::set<std::string>{":depth"};
    moderequirements["infraR"] = std::set<std::string>{":infraR"};
    moderequirements["infraL"] = std::set<std::string>{":infraL"};
    
    bgrptr = cast_output<cv::Mat,rstime_t>( "bgr" );
    depptr = cast_output<cv::Mat,rstime_t>( "depth" );
    infraLptr = cast_output<cv::Mat,rstime_t>( "infraL" );
    infraRptr = cast_output<cv::Mat,rstime_t>( "infraR" );
    
    //INIT (does some setup from base class using parser->)
    init();
    
    return;
  }

  ~streaming_rs()
  {
    //REV: Will destroying the last pointer to them stop them? Maybe this is not necessary...
    if( rsrecvr ) { rsrecvr->stop(); }
    if( rsparser ) { rsparser->stop(); }
  }

  void update(loopcond& loop){}

  bool isfinished()
  {
    if( false == rsrecvr->islooping() )
      {	return true;  }

    if( false == rsparser->islooping() )
      {	return true;  }

    return false;
  }
  
  double update_torender( std::map<std::string,timed_mat>& toupdate, const double timesec, std::mutex& mu )
  {
    double minoffsec=BIGNUM; //If I allow positives, we can go back "forward" right? I.e. skip ahead. Yay.

    //fprintf(stdout, "UPDATING: %lf\n", timesec);
    //Update timebases (if they are not already set)
    //REV: Note constructor will not complete until they are set so... :)
    depptr->set_timebase_hz_sec( rsparser->get_timebase_hz_sec() );
    bgrptr->set_timebase_hz_sec( rsparser->get_timebase_hz_sec() );
    infraLptr->set_timebase_hz_sec( rsparser->get_timebase_hz_sec() );
    infraRptr->set_timebase_hz_sec( rsparser->get_timebase_hz_sec() );

    //double rs_timeoffset_sec = -0.5;
    stream_specific_sync_offset_sec = -0.5;
    double sample_timesec = timesec + stream_specific_sync_offset_sec;
    //double sample_timesec = timesec + rs_timeoffset_sec;
    
    for( const auto& mode : streammodes )
      {
	const std::string name = publishname(mode.first);
	
	if( "depth"==mode.first )
	  {
	    //REV: check if any others are zeroed...if so use them.
	    depptr->rezero_time( bgrptr );
	    depptr->rezero_time( infraLptr );
	    depptr->rezero_time( infraRptr );
	    bool zeroed = depptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = depptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    //REV: what is the *SCALE*? Assume 0.001 (1 mm) i.e. 1/1000 meter, then 256*256-1 will be max 65.3 meters?
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    double sectime = depptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    double rawtime = val.value().timestamp;
		    //fprintf(stdout, "DEP: Retrieved frame from time [%lf] (zerocorrect: [%lf]) (target %lf)\n", rawtime, sectime, sample_timesec );
		    //Colorize it...?
		    cv::Mat prettydepth;
		    double scale = 1.0/256.0; //scale from its max? went from (0,255) to (0,256*256-1)
		    
		    val.value().payload.convertTo( prettydepth, CV_8UC1, scale );
		    cv::applyColorMap( prettydepth, prettydepth, cv::COLORMAP_JET ); //REV: must take different map?
		    toupdate[name] = timed_mat( prettydepth,
						sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		
	      }
	  }
	else if( "color"==mode.first )
	  {
	    bgrptr->rezero_time( depptr );
	    bgrptr->rezero_time( infraLptr );
	    bgrptr->rezero_time( infraRptr );
	    bool zeroed = bgrptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = bgrptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    const double sectime = bgrptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    double rawtime = val.value().timestamp;
		    //fprintf(stdout, "BGR: Retrieved frame from time [%lf] (zerocorrect: [%lf]) (target %lf)\n", rawtime, sectime, sample_timesec );
		    
		    toupdate[name] = timed_mat( val.value().payload,
						sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else if( "infraL"==mode.first )
	  {
	    infraLptr->rezero_time( depptr );
	    infraLptr->rezero_time( bgrptr );
	    infraLptr->rezero_time( infraRptr );
	    bool zeroed = infraLptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = infraLptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    const double sectime = infraLptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    double rawtime = val.value().timestamp;
		    //fprintf(stdout, "IFL: Retrieved frame from time [%lf] (zerocorrect: [%lf]) (target %lf)\n", rawtime, sectime, sample_timesec );
		    
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    toupdate[name] = timed_mat( val.value().payload,
						sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else if( "infraR"==mode.first )
	  {
	    infraRptr->rezero_time( depptr );
	    infraRptr->rezero_time( bgrptr );
	    infraRptr->rezero_time( infraLptr );
	    bool zeroed = infraRptr->zero_from_last_if_unzeroed( timesec ); //Depthptr is the base!
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = infraRptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    const double sectime = infraRptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    double rawtime = val.value().timestamp;
		    //fprintf(stdout, "IFR: Retrieved frame from time [%lf] (zerocorrect: [%lf]) (target %lf)\n", rawtime, sectime, sample_timesec );
		    		    
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    toupdate[name] = timed_mat( val.value().payload,
						sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else
	  {
	    fprintf(stderr, "Unknown stream mode?! Impossible\n");
	    exit(1);
	  }
      } //end for mode in streammodes
    if( minoffsec >= BIGNUM ) { minoffsec = timesec; }
    return minoffsec;
  } //end update_torender
  
  
}; //end streaming_rs

#endif // WITH_REALSENSE

////////////////////// REV: fucked myself into a hole with 2 parsers (who I need to combine!)
////////////////////// REV: but only one provides the videos...? For now just do that?
////////////////////// Another option is to try to get outputs from ALL guys? Or hierarchy ( data:gaze vs video:scene )
/////////////////////////////////--------> I like that. Disallow ambiguity.

struct streaming_tobii2
  : public streaming_base
{
  std::shared_ptr<tobii2_streaming_conn> tobii2conn;
  std::shared_ptr<tobii2_data_parser> dparser; //They can access via this->parser (base type though)
  std::shared_ptr<tobii2_video_parser> vparser; //They can access via this->parser (base type though)
  
  //REV: eye videos?
  std::shared_ptr<timed_buffer<cv::Mat,tobii2time_t>> sceneptr;
  
  std::shared_ptr<timed_buffer<json,tobii2time_t>> gpptr; // gp (2d gaze)
  std::shared_ptr<timed_buffer<json,tobii2time_t>> ptsptr; //sync ptr
  std::shared_ptr<timed_buffer<json,tobii2time_t>> marker2dptr; //marker (calib)

  
  tobii2_REST rest; //rest interface for calib etc.
  
    
  //REV: _id is the serial number...?
  //REV: id must be unique? I.e. not include...? Huh, I should put serial number too?
  streaming_tobii2( loopcond& loop, const tobii_glasses2_info& info, const std::string& _id )
    : streaming_base(_id), rest( tobii2_REST(info.ip6_addr) )
  {
    tobii2conn = std::make_shared<tobii2_streaming_conn>( info );
    tobii2conn->start( loop );
    
    dparser = std::make_shared<tobii2_data_parser>();
    dparser->start( loop, tobii2conn->datarecv );

    vparser = std::make_shared<tobii2_video_parser>();
    vparser->start( loop, tobii2conn->videorecv );
    
    parsers["data"] = dparser; //automatically downcast (upcast?) to base type?
    parsers["video"] = vparser; //automatically downcast (upcast?) to base type?

    //REV: set "modes" and check tha moderequirements size is same?
    streammodes["scene"] = false;
    streammodes["scene&gaze"] = false;
    //streammodes["calib"] = false;
    
    //"Calib" mode (automatically turns off if success?)
    
    moderequirements["scene"] = std::set<std::string>{"video:scene"};
    moderequirements["scene&gaze"] = std::set<std::string>{"video:scene","data:gp","data:pts","data:marker2d"};
    //moderequirements["calib"] = std::set<std::string>{"video:scene","data:gp","data:pts","data:marker2d"};
        
    sceneptr = cast_output<cv::Mat,tobii2time_t>( "video:scene" );
    gpptr = cast_output<json,tobii2time_t>( "data:gp" );
    ptsptr = cast_output<json,tobii2time_t>( "data:pts" );
    marker2dptr = cast_output<json,tobii2time_t>( "data:marker2d" );
    
    //INIT (does some setup from base class using parser->)
    init();
    
    return;
  }

  ~streaming_tobii2()
  {
    //REV: Will destroying the last pointer to them stop them? Maybe this is not necessary...
    if( tobii2conn ) { tobii2conn->stop(); }
    if( vparser ) { vparser->stop(); }
    if( dparser ) { dparser->stop(); }
  }

  void update(loopcond& loop){}

  bool isfinished()
  {
    /*if( false == tobii2conn->islooping() )
      { return true; }*/
    //REV: check conn->datarecv, videorecv, datakeepalive, videokeepalive, all looping?
    
    if( false == vparser->islooping() )
      { return true; }

    if( false == dparser->islooping() )
      { return true; }

    return false;
  }
  
  double update_torender( std::map<std::string,timed_mat>& toupdate, const double timesec, std::mutex& mu )
  {
    double minoffsec=BIGNUM; //If I allow positives, we can go back "forward" right? I.e. skip ahead. Yay.
    
    //Update timebases (if they are not already set)
    //REV: Note constructor will not complete until they are set so... :)
    sceneptr->set_timebase_hz_sec( vparser->get_timebase_hz_sec() );
    
    gpptr->set_timebase_hz_sec( dparser->get_timebase_hz_sec() );
    ptsptr->set_timebase_hz_sec( dparser->get_timebase_hz_sec() );
    marker2dptr->set_timebase_hz_sec( dparser->get_timebase_hz_sec() );
    
    
    //REV: I need to handle time better if it is tobii.
    //REV: if lots of slowdown, I should modify multiple ones so that they all show same time? Problem is it may drop for old ones...
    //double tobii2_timeoffset_sec = -0.5;
    stream_specific_sync_offset_sec = -0.5;
    double sample_timesec = timesec + stream_specific_sync_offset_sec;
    //double sample_timesec = timesec + tobii2_timeoffset_sec;
    
    for( const auto& mode : streammodes )
      {
	const std::string name = publishname(mode.first);
	
	if( "scene"==mode.first )
	  {
	    bool zeroed = sceneptr->zero_from_last_if_unzeroed( timesec );
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100; //Tobii is quite lagged, I need a way to offset target time?
		double d1=0,d2=0;
		bool ok=false;
		auto val = sceneptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore,&d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    //REV: what is the *SCALE*? Assume 0.001 (1 mm) i.e. 1/1000 meter, then 256*256-1 will be max 65.3 meters?
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    double sectime = sceneptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    toupdate[name] = timed_mat(  val.value().payload,
						 sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		
	      }
	  }
	else if( "scene&gaze"==mode.first ) /* REV: This will not resample gaze on the same frame if it repeats... :( */
	  {
	    bool zeroed = sceneptr->zero_from_last_if_unzeroed( timesec );
	    
	    if( mode.second )
	      {
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = sceneptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    cv::Mat toshow = val.value().payload.clone();
		    
		    uint64_t pts = val.value().timestamp;
		    
		    //REV: get gaze pos and calib marker
		    if( vparser->get_timebase_hz_sec() != 90000 ){ fprintf(stderr, "mpeg timebase not 90k?\n"); exit(1); }
		    auto datats = dparser->ts_from_pts( pts, vparser->get_timebase_hz_sec(), "pts" );
		    
#ifdef SINGLE_GAZE
		    uint64_t deltausec = 20000; //20 msec
		    dropbefore = true;
		    if( datats.has_value() )
		      {
			auto gp = gpptr->get_timed_element( datats.value(), deltausec, relative_timing::EITHER, dropbefore );
	      
			//GP is from top-left (Y 0 is top). I.e. same as opencv lol.
			if( gp.has_value() )
			  {
			    auto gpv = gp.value();
			    int xpx = toshow.size().width * (float)gpv.payload["gp"][0];
			    int ypx = toshow.size().height * (float)gpv.payload["gp"][1];
			    int rad=20;
			    int thick=4;
			    
			    cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
			  }
		      }
#else // MULTI GAZE
		    auto vstart = pts - (0.080 * vparser->get_timebase_hz_sec());
		    auto vend = pts;
		    auto tsstart = dparser->ts_from_pts( vstart, vparser->get_timebase_hz_sec(), "pts" );
		    auto tsend = dparser->ts_from_pts( vend, vparser->get_timebase_hz_sec(), "pts" );
		    uint64_t deltausec = 10000; //20 msec
		    dropbefore = true;
		    if( tsstart.has_value() && tsend.has_value() )
		      {
			auto gpvec = gpptr->get_timed_element_range( tsstart.value(), tsend.value(), deltausec, dropbefore );
			
			//GP is from top-left (Y 0 is top). I.e. same as opencv lol.
			if( gpvec.size() > 0 )
			  {
			    for( auto& gpv : gpvec )
			      {
				int xpx = toshow.size().width * (float)gpv.payload["gp"][0];
				int ypx = toshow.size().height * (float)gpv.payload["gp"][1];
				int rad=20;
				int thick=4;
				
				cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
			      }
			  }
		      }
#endif // MULTI GAZE

		    //CALIB MARKER2D

		    if( datats.has_value() )
		      {
			uint64_t mdeltausec = 250000; //250 msec...it is not sent often!
			dropbefore = true;
			auto m2 = marker2dptr->get_timed_element( datats.value(), mdeltausec, relative_timing::EITHER, dropbefore );
		    
			//GP is from top-left (Y 0 is top). I.e. same as opencv lol.
			if( m2.has_value() )
			  {
			    auto m2v = m2.value();
			    int xpx = toshow.size().width * (float)m2v.payload["marker2d"][0];
			    int ypx = toshow.size().height * (float)m2v.payload["marker2d"][1];
			    int rad=20;
			    int thick=4;
			
			    cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 255, 30), thick);
			  }
		      }

		    
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    auto maybets = sceneptr->clocktime_sec_of_timestamp(val.value().timestamp);
		    if( maybets.has_value() )
		      {
			toupdate[name] = timed_mat( toshow,
						    sceneptr->clocktime_sec_of_timestamp(val.value().timestamp).value() );
		      }
		    else
		      {
			fprintf(stderr, "scene&gaze, maybets no value?\n");
			exit(1);
		      }
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  }
	else
	  {
	    fprintf(stderr, "Unknown stream mode?! (%s) Impossible\n", mode.first.c_str());
	    exit(1);
	  }
      } //end for mode in streammodes
    if( minoffsec >= BIGNUM ) { minoffsec = timesec; }
    return minoffsec;
  } //end update_torender
  
  
}; //end streaming_tobii2


//REV: IMPLEMENT 13 oct 2022
struct streaming_tobii3
  : public streaming_base
{
  std::shared_ptr<rtsp_receiver> rtsprcvr;
  std::shared_ptr<tobii3_parser> t3parser;
  
  std::shared_ptr<timed_buffer<cv::Mat,tobii3time_t>> sceneptr;
  std::shared_ptr<timed_buffer<cv::Mat,tobii3time_t>> eyeptr;
  std::shared_ptr<timed_buffer<json,tobii3time_t>> gazeptr;

  tobii3_REST rest;
  
  
  streaming_tobii3(  loopcond& loop, const zeroconf_service_reply_struct& apiinfo,  const zeroconf_service_reply_struct& rtspinfo, const std::string& _id )
    : streaming_base(_id), rest( apiinfo.srcaddr, apiinfo.dstport )
  {
    //auto vec = split_string(rtspinfo.srcaddr, '%');
    std::string safeaddr = rtspinfo.srcaddr;
    //if( vec.size() > 1 )
    //  {
    //	safeaddr = join_strings_vec( vec, "%25" );
    //  }
    std::string addrport = safeaddr + ":" + std::to_string(rtspinfo.dstport);
    std::string myurl = "rtsp://" + addrport + "/live/all"; //full URL rtsp://
    //myurl = encode_url(myurl);
    boost::asio::ip::address addr;
    try
      {
	addr = boost::asio::ip::address::from_string(safeaddr);
      }
    catch( boost::exception& e )
      {
	fprintf(stderr, "Streaming Tobii3 Const Not a valid address [%s]\n", safeaddr.c_str());
	return;
      }
    
    std::cout << "Valid " << addr << std::endl;
    rtsprcvr = std::make_shared<rtsp_receiver>( myurl );
    rtsprcvr->start(loop);

    //REV: passing point just for mutex related stuff, for actual doloop it just needs the start..
    t3parser = std::make_shared<tobii3_parser>( rtsprcvr );
    t3parser->start( loop, rtsprcvr );
    
    parsers["rtsp"] = t3parser; 
    
    
    streammodes["scene"] = false;
    streammodes["scene&gaze"] = false;
    //streammodes["gaze"] = false;
    streammodes["eye"] = false;
    
    
    moderequirements["scene"] = std::set<std::string>{"rtsp:scene"};
    moderequirements["scene&gaze"] = std::set<std::string>{"rtsp:scene","rtsp:gaze"};
    //moderequirements["gaze"] = std::set<std::string>{"rtsp:gaze"};
    moderequirements["eye"] = std::set<std::string>{"rtsp:eye"};
    
    //ptrs for accessing...
    sceneptr = cast_output<cv::Mat,tobii3time_t>( "rtsp:scene" );
    gazeptr = cast_output<json,tobii3time_t>( "rtsp:gaze" );
    eyeptr = cast_output<cv::Mat,tobii3time_t>( "rtsp:eye" );
    
    //REV: marker2d calibration points is not in RTSP stream, it is over the REST api (HTTP) for tobii g3.
    init();
  }


  ~streaming_tobii3()
  {
    if( rtsprcvr ){rtsprcvr->stop();}
    if( t3parser ){t3parser->stop();}
  }

  //virtual functions UPDATE()
  //REV: for e.g. connecting/reconnecting to RTSP streams if they are unreliable? For when ptrs and etc. can not be allocated in constructors.
  void update(loopcond& loop)
  {  }

  bool isfinished()
  {
    //REV: check if they are running or not...
    if( false == rtsprcvr->islooping() )
      { return true; }
    if( false == t3parser->islooping() )
      { return true; }
    return false;
  }
  
  //virtual function UPDATE_TORENDER()
  double update_torender( std::map<std::string,timed_mat>& toupdate, const double timesec, std::mutex& mu )
  {
    double minoffsec=BIGNUM; //If I allow positives, we can go back "forward" right? I.e. skip ahead. Yay.
    
    double tb=-1;
    if( t3parser )
      { tb = t3parser->get_timebase_hz_sec(); }
    
    if( sceneptr )
      {
	sceneptr->set_timebase_hz_sec( tb );
      }
    if( eyeptr )
      {
	eyeptr->set_timebase_hz_sec( tb );
      }
    if( gazeptr )
      {
	gazeptr->set_timebase_hz_sec( tb );
      }
    
    //const double t3_timeoffset_sec = -0.30; 
    stream_specific_sync_offset_sec = -0.30;
    double sample_timesec = timesec + stream_specific_sync_offset_sec;
    //double sample_timesec = timesec + t3_timeoffset_sec;
    
    
    if( sceneptr )
      {
	//auto ztime = sceneptr->get_zerotime_clock_sec();
	if( !rest.check_calib_markers_zeroed() )
	  {
	    auto ztime = sceneptr->get_zerotime();
	    
	    if( ztime.has_value() )
	      {
		//This is how many seconds have passed since start of rtsp server... (i.e. like the zero_time roughly...)
		double secs = sceneptr->time_in_seconds( ztime.value() ); //Raw offset from zero PTS...?!
		double csecs =  sceneptr->get_zerotime_clocksec();
		
		//fprintf(stdout, "Attempting to zero calib markers! Sample Time: [%lf]   Scene PTS zeroed time [%lu]  Scene PTS "
			
		//double el = rtsprcvr->get_streamtime();
		//fprintf(stdout, "Elapsed time of rtsp object vs PTS time: [%lf] - [%lf] (%lf)\n", secs, el, secs-el );

		//REV: zero it to blah. rezero_time literally sets zero_time to it (i.e. native time).
		//REV: assumption is zero time corresponds to "zero" clocksec... So, if I request e.g. 3 seconds, it will get ZERO+BLAH.
		//scene has been zeroed...
		rest.zero_calib_markers( secs, csecs ); //ztime.value
	      }
	  }
      }
    
        
    for( const auto& mode : streammodes )
      {
	const std::string name = publishname(mode.first);
	
	if( "scene"==mode.first )
	  {
	    //If it is active! (indicated by bool, i.e. str:bool map)
	    if( mode.second )
	      {
		if( !sceneptr )
		  {
		    break;
		  }
		
		bool zeroed = sceneptr->zero_from_last_if_unzeroed( timesec );
		
		
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = sceneptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add to drawing memory
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    double sectime = sceneptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    toupdate[name] = timed_mat(  val.value().payload,
						 sectime );
		  }
	      } //end mode.second
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
	      } //end else not mode.second
	  } //end "scene" mode first
	else if( "scene&gaze"==mode.first )
	  {
	    //If it is active! (indicated by bool, i.e. str:bool map)
	    if( mode.second )
	      {
		if( !sceneptr || !gazeptr )
		  {
		    break;
		  }
		
		bool zeroed = sceneptr->zero_from_last_if_unzeroed( timesec );
		
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = sceneptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add to drawing memory
		if( val.has_value() )
		  {
		    cv::Mat toshow = val.value().payload.clone();
		    
		    uint64_t pts = val.value().timestamp;
		    
#ifdef SINGLE_GAZE
		    uint64_t deltausec = 20000; //20 msec
		    dropbefore = true;
		    auto gaze = gazeptr->get_timed_element( pts, deltausec, relative_timing::EITHER, dropbefore );
		    
		    //GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		    if( gaze.has_value() )
		      {
			
			//REV: fuck, I need to scale this by the "expected" height? It is giving me raw pixel values I think?
			auto gazev = gaze.value().payload;
			auto gazets = gaze.value().timestamp;
			if( gazev.contains( "gaze2d" ) )
			  {
			    double xrel = gazev["gaze2d"][0];
			    double yrel = gazev["gaze2d"][1];
			    int xpx = toshow.size().width * xrel;
			    int ypx = toshow.size().height * yrel; //Y is from top! In both opencv and in pupilinvis.
			    int rad=20;
			    int thick=4;
			    
			    cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
			  }
		      }
#else // MULTI GAZE
		    const double MULTIGAZE_SPAN_SEC = 0.080;
		    auto vstart = pts - (MULTIGAZE_SPAN_SEC * t3parser->get_timebase_hz_sec());
		    auto vend = pts;
		    //auto tsstart = piconn->get_gaze_parser()->ts_from_pts( vstart, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		    //auto tsend = piconn->get_gaze_parser()->ts_from_pts( vend, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		    uint64_t deltausec = 10000; //20 msec
		    dropbefore = true;
		    auto gazevec = gazeptr->get_timed_element_range( vstart, vend, deltausec, dropbefore );
		    
		    //GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		    if( gazevec.size() > 0 )
		      {
			for( auto& gazei : gazevec )
			  {
			    auto gazev = gazei.payload;
			    if( gazev.contains( "gaze2d" ) )
			      {
				double xrel = gazev["gaze2d"][0];
				double yrel = gazev["gaze2d"][1];
				int xpx = toshow.size().width * xrel;
				int ypx = toshow.size().height * yrel; //Y is from top! In both opencv and in pupilinvis.
				int rad=20;
				int thick=4;
			    
				cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
			      }
			  }
		      }
#endif // MULTI GAZE

		    //show calib marker timestamps if I get any...
		    auto cval = rest.get_calib_marker_2d_from_time( sample_timesec );
		    if( cval.has_value() )
		      {
			auto gazev = cval.value().payload;
			auto gazets = cval.value().timestamp;

			float xrel = gazev.x;
			float yrel = gazev.y;
			
			int xpx = toshow.size().width * xrel;
			int ypx = toshow.size().height * yrel; //Y is from top! In both opencv and in pupilinvis.
			int rad=30;
			int thick=7;
			
			cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 240, 240), thick);
		      }
		    		    
		    const std::lock_guard<std::mutex> lock(mu);
		    
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    auto maybets = sceneptr->clocktime_sec_of_timestamp(val.value().timestamp);
		    if( maybets.has_value() )
		      {
			toupdate[name] = timed_mat( toshow,
						    sceneptr->clocktime_sec_of_timestamp(val.value().timestamp).value() );
		      }
		  } //end if has value
	      } //end mode.second
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
	      } //end else not mode.second
	  }
	if( "eye"==mode.first )
	  {
	    //If it is active! (indicated by bool, i.e. str:bool map)
	    if( mode.second )
	      {
		if( !eyeptr )
		  {
		    break;
		  }
		
		bool zeroed = eyeptr->zero_from_last_if_unzeroed( timesec );
		
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = eyeptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add to drawing memory
		if( val.has_value() )
		  {
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    double sectime = eyeptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    toupdate[name] = timed_mat(  val.value().payload,
						 sectime );
		  }
	      } //end mode.second
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
	      } //end else not mode.second
	  } //end "eye" mode first
	else
	  {
	    //unknown mode
	  }
      } //end for mode in modes
    if( minoffsec >= BIGNUM ) { minoffsec = timesec; }
    return minoffsec;
  } //end update_torender
  
};

struct streaming_pupilinvis
  : public streaming_base
{

  //REV: this should be dealloc when it happens. Otherwise it gets passed from where...? I should construct it HERE! Fuck yea.
  std::shared_ptr<pupilinvis_interface> piconn=nullptr; //this "contains" the gaze/vid parser as well as the RTSP streamers...
  
  //REV: eye videos?
  std::shared_ptr<timed_buffer<cv::Mat,pupilinvistime_t>> worldptr=nullptr;
  std::shared_ptr<timed_buffer<vec2f,pupilinvistime_t>> gazeptr=nullptr; // gaze (2d gaze)
  
  //REV: _id is the serial number...?
  //REV: id must be unique? I.e. not include...? Huh, I should put serial number too?
  //REV: FIX THIS FROM HERE
  streaming_pupilinvis( loopcond& loop, const zeroconf_service_reply_struct& info, const std::string& _id )
    //streaming_pupilinvis( loopcond& loop, std::shared_ptr<pupilinvis_interface> _piconn, const std::string& _id )
    : streaming_base(_id)
  {
    piconn = std::make_shared<pupilinvis_interface>( info );
    piconn->start( loop ); //REV: this does the creation of the get_vid_parser() etc...but they are not yet "started"!
    moderequirements["world&gaze"] = std::set<std::string>{"video:world","data:gaze"};
    moderequirements["gaze"] = std::set<std::string>{"data:gaze"};
    moderequirements["world"] = std::set<std::string>{"video:world"};
    return;
  }
  
  void update(loopcond& loop)
  {
    //fprintf(stdout, "Will get gaze stream...\n");
    if( !piconn->get_gaze_stream() )
      {
	//fprintf(stdout, "Gaze stream is null, attempting to create...\n");
	//REV: is something else trying to access me? get_gaze_stream()?
	bool success = piconn->init_gaze(loop);
	//fprintf(stdout, "DONE Gaze stream is null, attempting to create...\n");
	if( success )
	  {
	    fprintf(stdout, "UPDATE: Success init gaze..!\n");
	    if( !piconn->get_gaze_stream() )
	      {
		fprintf(stderr, "REV: wtf success but gaze stream ptr is still null?\n");
		exit(1);
	      }
	    if( !piconn->get_gaze_parser() )
	      {
		fprintf(stderr, "REV: wtf success but gaze parser ptr is still null?\n");
		exit(1);
	      }
	    parsers["data"] = piconn->get_gaze_parser(); //automatically downcast (upcast?) to base type?
	    streammodes["gaze"] = false;
	    gazeptr = cast_output<vec2f,pupilinvistime_t>( "data:gaze" );
	    init();
	  }
      }
    else
      {
	//fprintf(stdout, "GAZE STREAM WAS NOT NULL (yay)...\n");
      }

    //fprintf(stdout, "Will get vid stream...\n");
    if( !piconn->get_vid_stream() )
      {
	//fprintf(stdout, "Vid stream was null...attempting to init world\n");
	bool success = piconn->init_world(loop);
	if( success )
	  {
	    fprintf(stdout, "Successfully init Vid stream...\n");
	    parsers["video"] = piconn->get_vid_parser(); //automatically downcast (upcast?) to base type?
	    streammodes["world"] = false;
	    worldptr = cast_output<cv::Mat,pupilinvistime_t>( "video:world" );
	    init();
	  }
	else
	  {
	    //fprintf(stdout, "Un-Successfully init vid stream...(it will stay null!)\n");
	  }
      }
    else
      {
	//	fprintf(stdout, "VID STREAM WAS NOT NULL (yay)...\n");
      }

    //fprintf(stdout, "Trying to check world&gaze\n");
    if( piconn->get_vid_stream() && piconn->get_gaze_stream() && !streammodes.contains("world&gaze") )
      {
	//fprintf(stdout, "Adding option -- world and gaze\n");
	streammodes["world&gaze"] = false;
	init();//not needed?
      }
    
    return;
  } //end UPDATE streaming pi (buttons)

  //REV: this is a bad way to do it, since I need to handle "disconnects" (temporary) versus full requests...
  //REV: better to handle it with "avails" -> However, in that case, how to deal with RTSP "please stop" shit?
  bool isfinished()
  {
    /*
    if( piconn->get_vid_stream() )
      {}
    if( piconn->get_vid_parser() )
      {}
    
    if( piconn->get_gaze_stream() )
      {}
    if( piconn->get_gaze_parser() )
    {}*/
    if( false == piconn->islooping() )
      { return true; }

    return false;
  }

  ~streaming_pupilinvis()
  {
    //REV: Will destroying the last pointer to them stop them? Maybe this is not necessary...
    if( piconn ) { piconn->stop(); }
  }



  //REV: todo 11 oct 2022
  // 1) reinit decoder parser each time RTSP is restarted
  // 2) modify timestamps in the timed_buffer to realtime timestamps (offsets) based on RTCP times...nasty but it may work.
  //    keep track of sum offset from very start, and add seconds offset to each based on "start time" difference between
  //    previous and current... To do this I need to keep track of shit when I decode it -- I can simply add a set re-timed value to
  //    things. How can I get the timebase from the RTSP? I guess just used the decoded one from the streams... Yea ;0 Assume
  //    it's the same as the other fucking guy...(RTP timestamps). Then, TS/90k gives me seconds. Start timestamp /1e6 gives me
  //    seconds.
  // 3) 
  double update_torender( std::map<std::string,timed_mat>& toupdate, const double timesec, std::mutex& mu )
  {
    double minoffsec=BIGNUM; //If I allow positives, we can go back "forward" right? I.e. skip ahead. Yay.
    //Update timebases (if they are not already set)
    //REV: Note constructor will not complete until they are set so... :)

    //fprintf(stdout, "*********** UPDATING TIME [%lf] *****************\n", timesec );
    
    if( worldptr )
      {
	worldptr->set_timebase_hz_sec( piconn->get_vid_parser()->get_timebase_hz_sec() );
      }
    
    if( gazeptr )
      {
	gazeptr->set_timebase_hz_sec( piconn->get_gaze_parser()->get_timebase_hz_sec() );
      }

    //I don't seem to get gaze data until about a second after video data? Weird...
    // WTF pupil invis gaze times and video times don't line up? Do I really have to sync shit using the RTCP frames?
    
    //const double pi_timeoffset_sec = -0.30; 
    const int PIWPX = 1088; //px
    const int PIHPX = 1080;

    stream_specific_sync_offset_sec = -0.30;
    double sample_timesec = timesec + stream_specific_sync_offset_sec;
    //double sample_timesec = timesec + pi_timeoffset_sec;
    
    for( const auto& mode : streammodes )
      {
	const std::string name = publishname(mode.first);
	
	if( "world"==mode.first )
	  {
	    	    
	    //If it is active! (indicated by bool, i.e. str:bool map)
	    if( mode.second )
	      {
		if( !worldptr )
		  {
		    break;
		  }
		
		bool zeroed = worldptr->zero_from_last_if_unzeroed( timesec );

		bool dropbefore=true;
		double deltasec=0.100; //Tobii is quite lagged, I need a way to offset target time?
		double d1=0,d2=0;
		bool ok=false;
		auto val = worldptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add to drawing memory
		if( val.has_value() )
		  {
		    //REV: what is the *SCALE*? Assume 0.001 (1 mm) i.e. 1/1000 meter, then 256*256-1 will be max 65.3 meters?
		    const std::lock_guard<std::mutex> lock(mu);
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    double sectime = worldptr->clocktime_sec_of_timestamp(val.value().timestamp).value();
		    toupdate[name] = timed_mat(  val.value().payload,
						 sectime );
		  }
	      }
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name );
		//remove
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		
	      }
	  } //end mode WORLD
	else if( "world&gaze"==mode.first ) /* REV: This will not resample gaze on the same frame if it repeats... :( */
	  {
	    
	    if( mode.second )
	      {
		if( !worldptr || !gazeptr )
		  {
		    break;
		  }
		bool zeroed = worldptr->zero_from_last_if_unzeroed( timesec );
		
		//REV: problem is here, I may have a situation where I need to also have "gaze" zeroed. I want to set same
		//times and set it as zero.
		if( false == gazeptr->iszeroed() )
		  {
		    gazeptr->rezero_time( worldptr );
		  }
		
		
		
		bool dropbefore=true;
		double deltasec=0.100;
		double d1=0,d2=0;
		bool ok=false;
		auto val = worldptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore, &d1, &d2, &ok );
		if( ok && d2 < minoffsec ) { minoffsec = d2; }
		//add
		if( val.has_value() )
		  {
		    cv::Mat toshow = val.value().payload.clone();
		    
		    uint64_t pts = val.value().timestamp;
		    
		    //fprintf(stdout, "Vid timestamp: [%ld]\n", pts);
		    
		    //REV: get gaze pos and calib marker
		    //if( piconn->get_vid_parser()->get_timebase_hz_sec() != 90000 ){ fprintf(stderr, "rtsp timebase not 90k?\n"); exit(1); }
		    
#ifdef SINGLE_GAZE
		    uint64_t deltausec = 20000; //20 msec
		    dropbefore = true;
		    auto gaze = gazeptr->get_timed_element( pts, deltausec, relative_timing::EITHER, dropbefore );
		    
		    //GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		    if( gaze.has_value() )
		      {
			
			//REV: fuck, I need to scale this by the "expected" height? It is giving me raw pixel values I think?
			auto gazev = gaze.value().payload;
			auto gazets = gaze.value().timestamp;
			//fprintf(stdout, "Got gaze: [%ld] ([%f] [%f])\n", gazets, gazev.x, gazev.y);
			const int pi_w = 1088;
			const int pi_h = 1080;
			double xrel = (float)gazev.x / pi_w;
			double yrel = (float)gazev.y / pi_h;
			int xpx = toshow.size().width * xrel;
			int ypx = toshow.size().height * yrel; //Y is from top! In both opencv and in pupilinvis.
			int rad=20;
			int thick=4;
			
			cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
		      }
#else // MULTI GAZE
		    const double MULTIGAZE_SPAN_SEC = 0.080;
		    auto vstart = pts - (MULTIGAZE_SPAN_SEC * piconn->get_vid_parser()->get_timebase_hz_sec());
		    auto vend = pts;
		    //auto tsstart = piconn->get_gaze_parser()->ts_from_pts( vstart, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		    //auto tsend = piconn->get_gaze_parser()->ts_from_pts( vend, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		    uint64_t deltausec = 10000; //20 msec
		    dropbefore = true;
		    auto gazevec = gazeptr->get_timed_element_range( vstart, vend, deltausec, dropbefore );
		    
		    //GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		    if( gazevec.size() > 0 )
		      {
			for( auto& gazev : gazevec )
			  {
			    //fprintf(stdout, "Got gaze: [%ld] ([%f] [%f]\n", gazev.timestamp, gazev.payload.x, gazev.payload.y);
			    const int pi_w = 1088;
			    const int pi_h = 1080;
			    double xrel = (float)gazev.payload.x / pi_w;
			    double yrel = (float)gazev.payload.y / pi_h;
			    int xpx = toshow.size().width * xrel;
			    int ypx = toshow.size().height * yrel;
			    int rad=20;
			    int thick=4;
			    
			    cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
			  }
		      }
#endif // MULTI GAZE

		    const std::lock_guard<std::mutex> lock(mu);
		    
		    //can safely value() because it would not return without being zeroed when queried for sec anyways.
		    auto maybets = worldptr->clocktime_sec_of_timestamp(val.value().timestamp);
		    if( maybets.has_value() )
		      {
			toupdate[name] = timed_mat( toshow,
						    worldptr->clocktime_sec_of_timestamp(val.value().timestamp).value() );
		      }
		    else
		      {
			fprintf(stderr, "world&gaze, maybets no value?\n");
			exit(1);
		      }
		  } //end if val.has_value() (we got a video frame!!!)
		/////////////// REV: CASE -- NO VIDEO FRAME (JUST GAZE) ////////////
		else //see if we can draw raw gaze without video frame...
		  {
		    //REV: rows, cols order
		    cv::Mat toshow = cv::Mat::zeros(PIHPX, PIWPX, CV_8UC3);
#ifdef SINGLE_GAZE
		    double deltasec2 = 0.020;
		    dropbefore = true;
		    auto gaze = gazeptr->get_timed_element_fromzero_secs( sample_timesec, deltasec2, relative_timing::EITHER, dropbefore );
		    
		    //GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		    if( gaze.has_value() )
		      {
			//REV: fuck, I need to scale this by the "expected" height? It is giving me raw pixel values I think?
			auto gazev = gaze.value().payload;
			auto gazets = gaze.value().timestamp;
			//fprintf(stdout, "Got gaze: [%ld] ([%f] [%f])\n", gazets, gazev.x, gazev.y);
			const int pi_w = 1088;
			const int pi_h = 1080;
			double xrel = (float)gazev.x / pi_w;
			double yrel = (float)gazev.y / pi_h;
			int xpx = toshow.size().width * xrel;
			int ypx = toshow.size().height * yrel; //Y is from top! In both opencv and in pupilinvis.
			int rad=20;
			int thick=4;
			
			cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);

			const std::lock_guard<std::mutex> lock(mu);
			auto maybets = gazeptr->clocktime_sec_of_timestamp(gaze.value().timestamp);
			if( maybets.has_value() )
			  {
			    toupdate[name] = timed_mat( toshow,
							gazeptr->clocktime_sec_of_timestamp(gaze.value().timestamp).value() );
			  }
		      }
		   
#else // MULTI GAZE
		    const double MULTIGAZE_SPAN_SEC = 0.080;
		    auto gstart = sample_timesec - MULTIGAZE_SPAN_SEC;
		    auto gend = sample_timesec;
		    //auto tsstart = piconn->get_gaze_parser()->ts_from_pts( vstart, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		    //auto tsend = piconn->get_gaze_parser()->ts_from_pts( vend, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		    double deltasec2 = 0.010;
		    dropbefore = true;
		    auto gazevec = gazeptr->get_timed_element_range_zeroed_secs( gstart, gend, deltasec2, dropbefore );
		    
		    //GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		    if( gazevec.size() > 0 )
		      {
			for( auto& gazev : gazevec )
			  {
			    //fprintf(stdout, "Got gaze: [%ld] ([%f] [%f]\n", gazev.timestamp, gazev.payload.x, gazev.payload.y);
			    const int pi_w = 1088;
			    const int pi_h = 1080;
			    double xrel = (float)gazev.payload.x / pi_w;
			    double yrel = (float)gazev.payload.y / pi_h;
			    int xpx = toshow.size().width * xrel;
			    int ypx = toshow.size().height * yrel;
			    int rad=20;
			    int thick=4;
			    
			    cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
			  }


			const std::lock_guard<std::mutex> lock(mu);
			auto lastgaze = gazevec.back();
			auto maybets = gazeptr->clocktime_sec_of_timestamp(lastgaze.timestamp);
			if( maybets.has_value() )
			  {
			    toupdate[name] = timed_mat( toshow,
							gazeptr->clocktime_sec_of_timestamp(lastgaze.timestamp).value() );
			  }
			
		      }
		    
#endif // MULTI GAZE
		  } //end ELSE (if we didn't get a video frame -- we'll draw blank).
	      } //end if mode.second
	    else
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  } //end mode = WORLD&GAZE
	else if( "gaze"==mode.first )
	  {
	    //fprintf(stdout, "REV: Just gaze not implemented for the moment...\n");
	    if( mode.second )
	      {
		if( !gazeptr )
		  {
		    break;
		  }
		
		//REV: fuck I should choose one as a "main" and zero that one first, then re-zero both to that?
		//Need to redo each time though? :(
		bool zeroed = gazeptr->zero_from_last_if_unzeroed( timesec );
		
		//REV: rows, cols order
		cv::Mat toshow = cv::Mat::zeros(PIHPX, PIWPX, CV_8UC3);
		
#ifdef SINGLE_GAZE
		double deltasec = 0.020;
		dropbefore = true;
		    
		auto gaze = gazeptr->get_timed_element_fromzero_secs( sample_timesec, deltasec, relative_timing::EITHER, dropbefore );
		    
		//GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		if( gaze.has_value() )
		  {
			
		    //REV: fuck, I need to scale this by the "expected" height? It is giving me raw pixel values I think?
		    auto gazev = gaze.value().payload;
		    auto gazets = gaze.value().timestamp;
		    //fprintf(stdout, "Got gaze: [%ld] ([%f] [%f])\n", gazets, gazev.x, gazev.y);
		    const int pi_w = 1088;
		    const int pi_h = 1080;
		    double xrel = (float)gazev.x / pi_w;
		    double yrel = (float)gazev.y / pi_h;
		    int xpx = toshow.size().width * xrel;
		    int ypx = toshow.size().height * yrel; //Y is from top! In both opencv and in pupilinvis.
		    int rad=20;
		    int thick=4;
			
		    cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);

		    const std::lock_guard<std::mutex> lock(mu);
		    auto maybets = gazeptr->clocktime_sec_of_timestamp(gaze.value().timestamp);
		    if( maybets.has_value() )
		      {
			toupdate[name] = timed_mat( toshow,
						    gazeptr->clocktime_sec_of_timestamp(gaze.value().timestamp).value() );
		      }
		  }
#else // MULTI GAZE
		const double MULTIGAZE_SPAN_SEC = 0.080;
		auto gstart = sample_timesec - MULTIGAZE_SPAN_SEC;
		auto gend = sample_timesec;
		//auto tsstart = piconn->get_gaze_parser()->ts_from_pts( vstart, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		//auto tsend = piconn->get_gaze_parser()->ts_from_pts( vend, piconn->get_vid_parser()->get_timebase_hz_sec(), "pts" );
		double deltasec = 0.010;
		bool dropbefore = true;
		auto gazevec = gazeptr->get_timed_element_range_zeroed_secs( gstart, gend, deltasec, dropbefore );
		    
		//GP is from top-left (Y 0 is top). I.e. same as opencv lol.
		if( gazevec.size() > 0 )
		  {
		    for( auto& gazev : gazevec )
		      {
			//fprintf(stdout, "Got gaze: [%ld] ([%f] [%f]\n", gazev.timestamp, gazev.payload.x, gazev.payload.y);
			const int pi_w = 1088;
			const int pi_h = 1080;
			double xrel = (float)gazev.payload.x / pi_w;
			double yrel = (float)gazev.payload.y / pi_h;
			int xpx = toshow.size().width * xrel;
			int ypx = toshow.size().height * yrel;
			int rad=20;
			int thick=4;
			    
			cv::circle( toshow, cv::Point(xpx,ypx), rad, cv::Scalar(30, 30, 255), thick);
		      }

		    const std::lock_guard<std::mutex> lock(mu);
		    auto lastgaze = gazevec.back();
		    auto maybets = gazeptr->clocktime_sec_of_timestamp(lastgaze.timestamp);
		    if( maybets.has_value() )
		      {
			toupdate[name] = timed_mat( toshow,
						    gazeptr->clocktime_sec_of_timestamp(lastgaze.timestamp).value() );
		      }
		  } //end if gazevec size > 0
#endif // MULTI GAZE
	      } //end if mode.second (this option is "on")
	    else //else, remove it's window..
	      {
		const std::lock_guard<std::mutex> lock(mu);
		auto it = toupdate.find( name ); 
		if( it != toupdate.end() )
		  {
		    it = toupdate.erase(it);
		  }
		//remove
	      }
	  } //end if "gaze" is mode.first
	else
	  {
	    fprintf(stderr, "Unknown stream mode?! (%s) Impossible\n", mode.first.c_str());
	    exit(1);
	  }
      } //end for mode in streammode?
    if( minoffsec >= BIGNUM ) { minoffsec = timesec; }
    return minoffsec;
  } //end update_torender
  
  
}; //end streaming_pupilinvis




struct streaming_set
{
public:
  std::map< std::string, std::shared_ptr<streaming_tobii2> > streaming_tobii2s; // by IP? or serial
  std::map< std::string, std::shared_ptr<streaming_pupilinvis> > streaming_pis; // by IP? or serial
  std::map< std::string, std::shared_ptr<streaming_tobii3> > streaming_tobii3s; // by IP? or serial
  
#ifdef WITH_ITD
  std::map< std::string, std::shared_ptr<streaming_itd> > streaming_itds; //just "itd" -- only one for now... :( By serial?
#endif

#ifdef WITH_REALSENSE
  std::map< std::string, std::shared_ptr<streaming_rs> > streaming_rss; //by serial?
#endif
  
  //REV: need to keep the textures too?
  //REV: I can't "cleanup" textures
  std::map<std::string,timed_mat> torender;
  std::map<std::string,stream_view_subwindow> svss; //REV: checks for changes (missing or added to torender).
  //Each of them is uh...tagged? By its name or IP or etc.?
  
  bool issaving;


  //REV: this is one ghetto way to do it (check they stream minimum time).
  //REV: better to just guarantee framerate etc.
  bool check_can_save_raw()
  {
    bool cansave=true;
    double MIN_TIME_BEFORE_SAVING_SEC=4.0;
    
    for( auto& s : streaming_tobii2s )
      {
	auto existed = s.second->get_existence_time();
	if( existed < MIN_TIME_BEFORE_SAVING_SEC ) { cansave = false; }
      }
    for( auto& s : streaming_tobii3s )
      {
	auto existed = s.second->get_existence_time();
	if( existed < MIN_TIME_BEFORE_SAVING_SEC ) { cansave = false; }
      }
    for( auto& s : streaming_pis )
      {
	auto existed = s.second->get_existence_time();
	if( existed < MIN_TIME_BEFORE_SAVING_SEC ) { cansave = false; }
      }
#ifdef WITH_REALSENSE
    for( auto& s : streaming_rss )
      {
	auto existed = s.second->get_existence_time();
	if( existed < MIN_TIME_BEFORE_SAVING_SEC ) { cansave = false; }
      }
#endif
    
#ifdef WITH_ITD
    for( auto& s : streaming_itds )
      {
	auto existed = s.second->get_existence_time();
	if( existed < MIN_TIME_BEFORE_SAVING_SEC ) { cansave = false; }
      }
#endif
    
    return cansave;
  }
  
  //Uses existing torender to update svss
  void update_svss( )
  {
    //fprintf(stdout, "Updating SVSSs...\n");
    //First, erase any that are now missing (they would only be removed if user toggled it off)
    for( auto it=svss.begin(); it != svss.end(); /*++it THIS WILL CAUSE INFINITE LOOP*/)
      {
	//fprintf(stdout, "Checking to erase [%s]\n", it->first.c_str() );
	if( !torender.contains( it->first ) )
	  {
	    //fprintf(stdout, "Contained [%s]! Erasing!!\n", it->first.c_str() );
	    it = svss.erase( it );
	  }
	else
	  {
	    ++it;
	  }
      }

    //fprintf(stdout, "Finished erasing!\n");
    
    //Second, add or update textures from torender
    for( const auto& r : torender )
      {
	if( !svss.contains( r.first ) )
	  {
	    //svss[r.first] = stream_view_subwindow( r.first );
	    svss.emplace( r.first, stream_view_subwindow{r.first} );
	  }
	svss[r.first].update( r.second.mat ); //Note, we are ignoring the r.second.timestamp
      }

    //fprintf(stdout, "Finished updating?!!\n");
    
  }

  void show_svss( )
  {
    for( auto&& s : svss )
      {
	s.second.show_in_imgui();
      }
  }

  bool stop_saving_raw()
  {
    if( !issaving )
      {
	return false;
      }

    for( auto&& stream : streaming_tobii2s )
      {
	fprintf(stdout, "Stopping save: [%s]\n", stream.first.c_str());
	stream.second->stop_saving_raw( );
      }

    for( auto&& stream : streaming_tobii3s )
      {
	fprintf(stdout, "Stopping save: [%s]\n", stream.first.c_str());
	stream.second->stop_saving_raw( );
      }
    
    for( auto&& stream : streaming_pis )
      {
	fprintf(stdout, "Stopping save: [%s]\n", stream.first.c_str());
	stream.second->stop_saving_raw( );
      }

#ifdef WITH_REALSENSE
    for( auto&& stream : streaming_rss )
      {
	fprintf(stdout, "Stopping save: [%s]\n", stream.first.c_str());
	stream.second->stop_saving_raw( );
      }
#endif

#ifdef WITH_ITD
    for( auto&& stream : streaming_itds )
      {
	fprintf(stdout, "Stopping save: [%s]\n", stream.first.c_str());
	stream.second->stop_saving_raw( );
      }
#endif

    issaving = false;
    
    return true;
    
  }

  
  
  bool start_saving_raw( const std::string& basepath )
  {
    //for all streaming_tobii2, streaing etc.
    // start_saving_raw (to directory)

    //use its id in e.g. streaming_itds[x], the x

    //Create basepath if it is not made (it will be)
    //Create path with timestamp appended (if user wants?)
    //Create subpath inside (for my name)

    if( issaving )
      {
	return false;
      }
    
    issaving = true;
    
    auto ts = get_datetimestamp_now();
    auto mysavedir = basepath + "/raw-" + ts;
    auto created = createdir( mysavedir );

    fprintf(stdout, "Will attempt to create and start saving in [%s]\n", mysavedir.c_str());
    if( created )
      {
	std::set<std::string> usednames;
	for( auto&& stream : streaming_tobii2s )
	  {
	    std::string name = "device-" + clean_fname(stream.first);
	    while( usednames.contains(name) )
	      {
		fprintf(stderr, "There is some overlap in your shit? [%s] overlaps\n", name.c_str());
		name += "1";
	      }
	    usednames.insert(name);
	    auto subdir = mysavedir + "/" + name;
	    created = createdir( subdir );
	    if( !created )
	      {
		fprintf(stderr, "Fuck off\n");
	      }
	    else
	      {
		fprintf(stdout, "Starting raw save: [%s] (path=%s)\n", stream.first.c_str(), subdir.c_str());
		stream.second->start_saving_raw( subdir );
	      }
	  }

	//REV: this is redundant literally I just copy the exact same thing FUCK (macro it...?)
	for( auto&& stream : streaming_tobii3s )
	  {
	    std::string name = "device-" + clean_fname(stream.first);
	    while( usednames.contains(name) )
	      {
		fprintf(stderr, "There is some overlap in your shit? [%s] overlaps\n", name.c_str());
		name += "1";
	      }
	    usednames.insert(name);
	    auto subdir = mysavedir + "/" + name;
	    created = createdir( subdir );
	    if( !created )
	      {
		fprintf(stderr, "Fuck off\n");
	      }
	    else
	      {
		fprintf(stdout, "Starting raw save: [%s] (path=%s)\n", stream.first.c_str(), subdir.c_str());
		stream.second->start_saving_raw( subdir );
	      }
	  }
	
	for( auto&& stream : streaming_pis )
	  {
	    std::string name = "device-" + clean_fname(stream.first);
	    while( usednames.contains(name) )
	      {
		fprintf(stderr, "There is some overlap in your shit? [%s] overlaps\n", name.c_str());
		name += "1";
	      }
	    usednames.insert(name);
	    auto subdir = mysavedir + "/" + name;
	    created = createdir( subdir );
	    if( !created )
	      {
		fprintf(stderr, "Fuck off\n");
	      }
	    else
	      {
		fprintf(stdout, "Starting raw save: [%s] (path=%s)\n", stream.first.c_str(), subdir.c_str());
		stream.second->start_saving_raw( subdir );
	      }
	  }
	
#ifdef WITH_REALSENSE
	for( auto&& stream : streaming_rss )
	  {
	    std::string name = "device-" + clean_fname(stream.first);
	    while( usednames.contains(name) )
	      {
		fprintf(stderr, "There is some overlap in your shit? [%s] overlaps\n", name.c_str());
		name += "1";
	      }
	    usednames.insert(name);
	    auto subdir = mysavedir + "/" + name;
	    created = createdir( subdir );
	    if( !created )
	      {
		fprintf(stderr, "Fuck off\n");
	      }
	    else
	      {
		fprintf(stdout, "Starting raw save: [%s] (path=%s)\n", stream.first.c_str(), subdir.c_str());
		stream.second->start_saving_raw( subdir );
	      }
	  }
#endif //WITH_REALSENSE

#ifdef WITH_ITD
	for( auto&& stream : streaming_itds )
	  {
	    std::string name = "device-" + clean_fname(stream.first);
	    while( usednames.contains(name) )
	      {
		fprintf(stderr, "There is some overlap in your shit? [%s] overlaps\n", name.c_str());
		name += "1";
	      }
	    usednames.insert(name);
	    auto subdir = mysavedir + "/" + name;
	    created = createdir( subdir );
	    if( !created )
	      {
		fprintf(stderr, "Fuck off\n");
	      }
	    else
	      {
		fprintf(stdout, "Starting raw save: [%s] (path=%s)\n", stream.first.c_str(), subdir.c_str());
		stream.second->start_saving_raw( subdir );
	      }
	  }
#endif // WITH_ITDS
      }
    return true;
  } //end start saving raw
  
  streaming_set()
    : issaving(false)
  { return;  }

  ~streaming_set()
  {
    stop_saving_raw(); //checks if is saving anyways
    //stop saving?
  }

  //void draw_update_streams( const double timesec )
  double draw_update_streams( const double timesec )
  {
    double slowest = BIGNUM;
    std::mutex mu;
    //fprintf(stdout, "Will update TORENDER (itd)\n");
#ifdef WITH_ITD
    for( auto&& itd : streaming_itds )
      {
	double d = itd.second->update_torender( torender, timesec, mu );
	if( d < slowest ) { slowest = d; }
      }
#endif

#ifdef WITH_REALSENSE
    for( auto&& rs : streaming_rss )
      {
	double d = rs.second->update_torender( torender, timesec, mu );
	if( d < slowest ) { slowest = d; }
      }
#endif

    for( auto&& tobii2 : streaming_tobii2s )
      {
	double d = tobii2.second->update_torender( torender, timesec, mu );
	if( d < slowest ) { slowest = d; }
      }

    for( auto&& tobii3 : streaming_tobii3s )
      {
	double d = tobii3.second->update_torender( torender, timesec, mu );
	if( d < slowest ) { slowest = d; }
      }

    for( auto&& pi : streaming_pis )
      {
	double d = pi.second->update_torender( torender, timesec, mu );
	if( d < slowest ) { slowest = d; }
      }

    
    
    //fprintf(stdout, "Will update SVSS\n");
    update_svss();
    //fprintf(stdout, "Will show SVSS\n");
    show_svss();
    //fprintf(stdout, "Finished draw/update\n");

    if( slowest >= BIGNUM )
      {	slowest = 0.0; }
    return slowest;
  }


#ifdef WITH_ITD
  void add_itd( loopcond& loop, std::shared_ptr<ITD_wrapper> _itdptr, const std::string& id )
  {
    if( id.empty() )
      {
	fprintf(stderr, "WTF ID EMPTY? ITD\n");
	exit(1);
      }
    auto it = streaming_itds.find( id );
    if( it == streaming_itds.end() )
      {
	streaming_itds[id] = std::make_shared<streaming_itd>( loop, _itdptr, id );
      }
  }

  bool remove_itd( const std::string& id )
  {
    auto it = streaming_itds.find( id );
    if( it != streaming_itds.end() )
      {
	it = streaming_itds.erase( it );
	return true;
      }
    return false;
  }
  
  std::shared_ptr<streaming_itd> get_itd( const std::string& id )
  {
    auto it = streaming_itds.find( id );
    if( it != streaming_itds.end() )
      {
	return it->second;
      }
    return nullptr;
  }

#endif //WITH_ITD



  
#ifdef WITH_REALSENSE
  void add_rs( loopcond& loop, const std::string& sn, const std::string& id )
  {
    if( id.empty() )
      {
	fprintf(stderr, "WTF ID EMPTY? ITD\n");
	exit(1);
      }
    auto it = streaming_rss.find( id );
    if( it == streaming_rss.end() )
      {
	streaming_rss[id] = std::make_shared<streaming_rs>( loop, sn, id );
      }
  }

  
  bool remove_rs( const std::string& id )
  {
    auto it = streaming_rss.find( id );
    if( it != streaming_rss.end() )
      {
	it = streaming_rss.erase( it );
	return true;
      }
    return false;
  }

  std::shared_ptr<streaming_rs> get_rs( const std::string& id )
  {
    auto it = streaming_rss.find( id );
    if( it != streaming_rss.end() )
      {
	return it->second;
      }
    return nullptr;
  }

#endif //WITH_REALSENSE

  
  void add_tobii2( loopcond& loop, const tobii_glasses2_info& info, const std::string& id )
  {
    if( id.empty() )
      {
	fprintf(stderr, "WTF ID EMPTY? TOBII2\n");
	exit(1);
      }
    
    auto it = streaming_tobii2s.find( id );
    if( it == streaming_tobii2s.end() )
      {
	streaming_tobii2s[id] = std::make_shared<streaming_tobii2>( loop, info, id );
      }
  }
  
  bool remove_tobii2( const std::string& id )
  {
    auto it = streaming_tobii2s.find( id );
    if( it != streaming_tobii2s.end() )
      {
	it = streaming_tobii2s.erase( it );
	return true;
      }
    return false;
  }
  
  std::shared_ptr<streaming_tobii2> get_tobii2( const std::string& id )
  {
    auto it = streaming_tobii2s.find( id );
    if( it != streaming_tobii2s.end() )
      {
	return it->second;
      }
    return nullptr;
  }
  
  
  void add_pupilinvis( loopcond& loop, const zeroconf_service_reply_struct& info, const std::string& id )
  //void add_pupilinvis( loopcond& loop, const std::shared_ptr<pupilinvis_interface> piconn, const std::string& id )
  {
    if( id.empty() )
      {
	fprintf(stderr, "WTF ID EMPTY? PI\n");
	exit(1);
      }
    
    auto it = streaming_pis.find( id );
    if( it == streaming_pis.end() )
      {
	//REV: this is where it would normally be added (which is fine!). Need to update buttons though!
	//REV: here! Add with service info...
	streaming_pis[id] = std::make_shared<streaming_pupilinvis>( loop, info, id );
	fprintf(stdout, "Created (add pupilinvis), will update...\n");
	streaming_pis[id]->update( loop );
	fprintf(stdout, "DONE Created (add pupilinvis), will update...\n");
      }
  }
  
  bool remove_pupilinvis( const std::string& id )
  {
    auto it = streaming_pis.find( id );
    if( it != streaming_pis.end() )
      {
	it = streaming_pis.erase( it );
	return true;
      }
    return false;
  }

  bool remove_tobii3( const std::string& id )
  {
    auto it = streaming_tobii3s.find( id );
    if( it != streaming_tobii3s.end() )
      {
	it = streaming_tobii3s.erase( it );
	return true;
      }
    return false;
  }
  
  std::shared_ptr<streaming_pupilinvis> get_pupilinvis( const std::string& id )
  {
    auto it = streaming_pis.find( id );
    if( it != streaming_pis.end() )
      {
	return it->second;
      }
    return nullptr;
  }

  std::shared_ptr<streaming_tobii3> get_tobii3( const std::string& id )
  {
    auto it = streaming_tobii3s.find( id );
    if( it != streaming_tobii3s.end() )
      {
	return it->second;
      }
    return nullptr;
  }

  void add_tobii3( loopcond& loop, const zeroconf_service_reply_struct& apiinfo, const zeroconf_service_reply_struct& rtspinfo, const std::string& id )
  //void add_pupilinvis( loopcond& loop, const std::shared_ptr<pupilinvis_interface> piconn, const std::string& id )
  {
    if( id.empty() )
      {
	fprintf(stderr, "WTF ID EMPTY? TOBII3\n");
	exit(1);
      }
    
    auto it = streaming_tobii3s.find( id );
    if( it == streaming_tobii3s.end() )
      {
	//REV: this is where it would normally be added (which is fine!). Need to update buttons though!
	//REV: here! Add with service info...
	streaming_tobii3s[id] = std::make_shared<streaming_tobii3>( loop, apiinfo, rtspinfo, id );
      }
  }
  
}; //end struct streaming set





//REV: make "model" of each active connection based on
//type -- connected_tobii2s etc.?
//
ImVec4 from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
  auto res = ImVec4(r / (float)255, g / (float)255, b / (float)255, a / (float)255);
  return res;
}

ImVec4 black = from_rgba( 0, 0, 0, 255 );
ImVec4 white = from_rgba( 255, 255, 255, 255 );
ImVec4 gray = from_rgba( 100, 100, 100, 255 );
ImVec4 gray2 = from_rgba( 30, 30, 30, 255 );
ImVec4 darkyellow = from_rgba( 20, 110, 110, 255 );

struct uxwindow
{
  GLFWwindow* win=nullptr;
  int winwidth, winheight;
  int fb_width, fb_height;
  bool fullscreen;
  double scale_factor;
  
  const int conn_panel_width = 350;
  const int conn_entry_height = 50;
  
  const int savebutton_wid = 250;
  const int savebutton_hei = 60;
  

  std::string buttonlabel;
  ImVec2 button_pos;
  
  // REV: if it is in ACTIVESTREAMS, but it is not in AVAIL_X, stop the streaming
  //auto streamptr = activestreams.get_pupilinvis( name );
  //streamptr->disable_all_streams(); //REV: fuck I need to render this first! But I will remove the stream...
  //tobii3_removes.insert(name);
    

  loopcond loop;

  std::string wintitle_str="RTEYE2";
  bool first_frame=true;
  
  streaming_set activestreams;
  
  Timer apptimesec;
  double lagtimesec;
  
  double laggedsec;
  
  avail_tobii_glasses2 avail_tobii2s;
  std::set<std::string> tobii2_removes;

  std::shared_ptr<avail_pupilinvis> avail_pis;
  std::set<std::string> pi_removes;

  std::shared_ptr<avail_tobii3> avail_tobii3s;
  std::set<std::string> tobii3_removes;
  
#ifdef WITH_ITD
  avail_itd avail_itds;
  std::set<std::string> itd_removes;
#endif

#ifdef WITH_REALSENSE
  avail_realsense_devs avail_rss;
  std::set<std::string> rs_removes;
#endif
  
  uint64_t framenum;
  
  uxwindow()
    : lagtimesec(0.0), framenum(0)
  {
    avail_tobii2s.start( loop );
    
    auto avail_zss = std::make_shared<avail_zeroconf_services_set>();

    avail_pis = std::make_shared<avail_pupilinvis>( avail_zss );
    avail_pis->start( loop );

    avail_tobii3s = std::make_shared<avail_tobii3>( avail_zss );
    avail_tobii3s->start( loop );
    
#ifdef WITH_ITD
    avail_itds.start( loop );
#endif
#ifdef WITH_REALSENSE
    avail_rss.start( loop );
#endif
  }
  
  ~uxwindow()
  {
    loop.stop();

    avail_tobii2s.stop();
    
    avail_tobii3s->stop();

    avail_pis->stop();
    
#ifdef WITH_ITD
    avail_itds.stop();
#endif

#ifdef WITH_REALSENSE
    //REV: this was not here, is there a reason not to stop the RSS? 
    avail_rss.stop(); 
#endif
    
    cleanup();
  }

  //for while(window)
  operator bool()
  {
    end_frame_and_draw();
    
    auto res = !glfwWindowShouldClose(win);
    if( first_frame )
      {
	//do init stuff?
	first_frame=false;
      }
    begin_frame();
    return res;
  }
  
  
  //REV: FIX HERE FOR PUPIL INVIS
  void draw_conn_entry_pupilinvis( const zeroconf_service_reply_struct& info, const std::string name )
  //void draw_conn_entry_pupilinvis( std::shared_ptr<pupilinvis_interface> piconn, const std::string name )
  //void draw_conn_entry_pupilinvis( const std::shared_ptr<pupilinvis_interface> info, const std::string name )
  {
    
    int connbutton_w=150, connbutton_h=30, connbutton_buffer=10, connbutton_xoffset=20;
    int togglebutton_w=150, togglebutton_h=30, togglebutton_buffer=5, togglebutton_xoffset=40;
    
    auto initpos = ImGui::GetCursorScreenPos();
    
    //Draw enough extra space for the button
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
					      { initpos.x + conn_panel_width,
						initpos.y + connbutton_h + 10 },
					      ImColor(gray));

    
    auto pos = ImGui::GetCursorPos();
    button_pos = { conn_panel_width - connbutton_w - connbutton_xoffset, pos.y + 7 };
    ImGui::SetCursorPos(button_pos);

    //REV: get stream to see its state.
    auto streamptr = activestreams.get_pupilinvis( name );
    
    if( !streamptr ) //==nullptr
      {
	//Imgui: X##Y shows label X on button, uses X##Y as ID.
	//Or better imgui::pushID and popID?
	//Button, that makes connect
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "CONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    //activestreams.add_pupilinvis( loop, piconn, name );
	    activestreams.add_pupilinvis( loop, info, name );
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
      }
    else
      {
	//fprintf(stdout, "Will update the pi!\n");
	streamptr->update(loop); //REV; there is no "time" here...I just need to "unzero" if it is not zeroed! :)
	//fprintf(stdout, "DONE Will update the pi!\n");
	
	//Button to disconnect *UNLESS* I am streaming! (need a pointer to that streaming shit too?)
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "DISCONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    //REV DISABLE ALL STREAMS (as if button had been pressed)
	    streamptr->disable_all_streams(); //REV: fuck I need to render this first! But I will remove the stream...
	    //REV: stop streaming!
	    pi_removes.insert(name);
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
	
	std::set<std::string> toremove, toadd;
	
	for( auto&& mode : streamptr->streammodes )
	  {
	    initpos = ImGui::GetCursorScreenPos();
	    int toggle_h = 20;
	    //Draw enough extra space for the button
	    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						      { initpos.x + conn_panel_width,
							initpos.y + togglebutton_h + togglebutton_buffer },
						      ImColor(gray));
	    
	    pos = ImGui::GetCursorPos();
	    const ImVec2 label_pos = { pos.x + togglebutton_xoffset, pos.y + togglebutton_buffer/2 };
	    ImGui::SetCursorPos(label_pos);
	    	    
	    if( mode.second ) //Mode is ON (draw stop streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (STOP)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { (float)togglebutton_w, (float)togglebutton_h }))
		  {
		    toremove.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    else  // mode is OFF (draw start streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (START)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toadd.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    
	    ImGui::SetCursorPos( { 0, pos.y + togglebutton_buffer + togglebutton_h } );
	  }

	//PI has no calib status. Just connect and disconnect?
		
	//update with toremove toadd
	streamptr->update_modes( toadd, toremove );
		
      } //end if pupilinvis stream is CONNECTED
  } //end draw_conn_entry_pupilinvis


  //REV: FIX HERE FOR TOBII3
  void draw_conn_entry_tobii3( const zeroconf_service_reply_struct& apiinfo, const zeroconf_service_reply_struct& rtspinfo, const std::string name )

  {
    
    int connbutton_w=150, connbutton_h=30, connbutton_buffer=10, connbutton_xoffset=20;
    int togglebutton_w=150, togglebutton_h=30, togglebutton_buffer=5, togglebutton_xoffset=40;
    
    auto initpos = ImGui::GetCursorScreenPos();
    
    //Draw enough extra space for the button
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
					      { initpos.x + conn_panel_width,
						initpos.y + connbutton_h + 10 },
					      ImColor(gray));

    
    auto pos = ImGui::GetCursorPos();
    button_pos = { conn_panel_width - connbutton_w - connbutton_xoffset, pos.y + 7 };
    ImGui::SetCursorPos(button_pos);

    //REV: get stream to see its state.
    auto streamptr = activestreams.get_tobii3( name );
    
    if( !streamptr ) //==nullptr
      {
	//Imgui: X##Y shows label X on button, uses X##Y as ID.
	//Or better imgui::pushID and popID?
	//Button, that makes connect
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "CONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    activestreams.add_tobii3( loop, apiinfo, rtspinfo, name );
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
      }
    else
      {
	streamptr->update(loop); //REV; there is no "time" here...I just need to "unzero" if it is not zeroed! :)
	
	//Button to disconnect *UNLESS* I am streaming! (need a pointer to that streaming shit too?)
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "DISCONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    //REV DISABLE ALL STREAMS (as if button had been pressed)
	    streamptr->disable_all_streams(); //REV: fuck I need to render this first! But I will remove the stream...
	    //REV: stop streaming!
	    tobii3_removes.insert(name);
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
	
	std::set<std::string> toremove, toadd;
	
	for( auto&& mode : streamptr->streammodes )
	  {
	    initpos = ImGui::GetCursorScreenPos();
	    int toggle_h = 20;
	    //Draw enough extra space for the button
	    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						      { initpos.x + conn_panel_width,
							initpos.y + togglebutton_h + togglebutton_buffer },
						      ImColor(gray));
	    
	    pos = ImGui::GetCursorPos();
	    const ImVec2 label_pos = { pos.x + togglebutton_xoffset, pos.y + togglebutton_buffer/2 };
	    ImGui::SetCursorPos(label_pos);
	    	    
	    if( mode.second ) //Mode is ON (draw stop streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (STOP)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toremove.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    else  // mode is OFF (draw start streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (START)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toadd.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    
	    ImGui::SetCursorPos( { 0, pos.y + togglebutton_buffer + togglebutton_h } );
	  }

	//REV: draw rectangle...
	initpos = ImGui::GetCursorScreenPos();
	
	int caliblabel_h = 20;
	int caliblabel_ybuffer = 10;
	int caliblabel_xoffset = 5;
	
	int calibbutton_h = 30;
	int calibbutton_w = 130;
	int calibbutton_ybuffer = 10;
	
	int calibbutton_xoffset = 100;
	
	//int recth = caliblabel_h + caliblabel_ybuffer + calibbutton_ybuffer + calibbutton_h;
	int recth =  calibbutton_ybuffer + calibbutton_h + caliblabel_ybuffer;
	
	//Draw enough extra space for the button
	ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						  { initpos.x + conn_panel_width,
						    initpos.y + recth },
						  ImColor(gray));
	
	pos = ImGui::GetCursorPos();
	const ImVec2 label_pos = { pos.x + caliblabel_xoffset, pos.y + caliblabel_ybuffer };
	ImGui::SetCursorPos(label_pos);
	
	streamptr->rest.update_calib_status(); //do only once every so often?
	
	std::string caliblabel = "Calibration status:\n\t" + streamptr->rest.get_calib_status();
	ImGui::Text(" %s", caliblabel.c_str());
	ImGui::SetCursorPos( { 0, pos.y + caliblabel_ybuffer + caliblabel_h } );
	
	
	//pos = ImGui::GetCursorPos();
	button_pos = { conn_panel_width - 20 - calibbutton_w, pos.y + caliblabel_ybuffer/2 };
	ImGui::SetCursorPos(button_pos);
	
	//REV: tobii3 has no offline "wait for result" although it hangs a bit...so just let it do this?
	if( true || !streamptr->rest.am_calibrating() )
	  {
	    buttonlabel = "CALIBRATE##" + name;
	    //Allow user to click to start calibrating
	    if(ImGui::Button(buttonlabel.c_str(), { calibbutton_w, calibbutton_h }))
	      {
		streamptr->rest.start_calib();
	      }
	  }
	else
	  {
	    buttonlabel = "IN PROGRESS##" + name;
	    //ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	    ImGui::BeginDisabled();
	    //ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	    
	    //Just draw button grayed out and disabled...
	    if(ImGui::Button(buttonlabel.c_str(), { calibbutton_w, calibbutton_h }))
	      {
		// no op
	      }
	    
	    //ImGui::PopItemFlag();
	    ImGui::EndDisabled();
	    //ImGui::PopStyleVar();
	  }
	ImGui::SetCursorPos( {0, pos.y + calibbutton_h + calibbutton_ybuffer } );
	
	//update with toremove toadd
	streamptr->update_modes( toadd, toremove );
		
      } //end if tobii3 stream is CONNECTED
  } //end draw_conn_entry_tobii3
  
  
  

  
  void draw_conn_entry_tobii2( const tobii_glasses2_info& info, const std::string name )
  {

    int connbutton_w=150, connbutton_h=30, connbutton_buffer=10, connbutton_xoffset=20;
    int togglebutton_w=150, togglebutton_h=30, togglebutton_buffer=5, togglebutton_xoffset=40;
    
    auto initpos = ImGui::GetCursorScreenPos();
    
    //Draw enough extra space for the button
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
					      { initpos.x + conn_panel_width,
						initpos.y + connbutton_h + 10 },
					      ImColor(gray));


    auto pos = ImGui::GetCursorPos();
    button_pos = { conn_panel_width - connbutton_w - connbutton_xoffset, pos.y + 7 };
    ImGui::SetCursorPos(button_pos);
    
    auto streamptr = activestreams.get_tobii2( name );
    
    if( !streamptr ) //==nullptr
      {
	//Imgui: X##Y shows label X on button, uses X##Y as ID.
	//Or better imgui::pushID and popID?
	//Button, that makes connect
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "CONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    activestreams.add_tobii2( loop, info, name );
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
      }
    else
      {
	//Button to disconnect *UNLESS* I am streaming! (need a pointer to that streaming shit too?)
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "DISCONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    //REV DISABLE ALL STREAMS (as if button had been pressed)
	    streamptr->disable_all_streams(); //REV: fuck I need to render this first! But I will remove the stream...
	    //REV: stop streaming!
	    //activestreams.remove_tobii2( name ); //will call its destructor etc.
	    tobii2_removes.insert(name);
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
	
	std::set<std::string> toremove, toadd;
	
	for( auto&& mode : streamptr->streammodes )
	  {
	    initpos = ImGui::GetCursorScreenPos();
	    int toggle_h = 20;
	    //Draw enough extra space for the button
	    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						      { initpos.x + conn_panel_width,
							initpos.y + togglebutton_h + togglebutton_buffer },
						      ImColor(gray));

	    pos = ImGui::GetCursorPos();
	    const ImVec2 label_pos = { pos.x + togglebutton_xoffset, pos.y + togglebutton_buffer/2 };
	    ImGui::SetCursorPos(label_pos);
	    	    
	    if( mode.second ) //Mode is ON (draw stop streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (STOP)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toremove.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    else  // mode is OFF (draw start streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (START)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toadd.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    
	    ImGui::SetCursorPos( { 0, pos.y + togglebutton_buffer + togglebutton_h } );
	  }

	//REV: draw rectangle...
	initpos = ImGui::GetCursorScreenPos();
	
	int caliblabel_h = 20;
	int caliblabel_ybuffer = 10;
	int caliblabel_xoffset = 5;
	
	int calibbutton_h = 30;
	int calibbutton_w = 130;
	int calibbutton_ybuffer = 10;
	
	int calibbutton_xoffset = 100;
	
	//int recth = caliblabel_h + caliblabel_ybuffer + calibbutton_ybuffer + calibbutton_h;
	int recth =  calibbutton_ybuffer + calibbutton_h + caliblabel_ybuffer;
	
	//Draw enough extra space for the button
	ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						  { initpos.x + conn_panel_width,
						    initpos.y + recth },
						  ImColor(gray));

	pos = ImGui::GetCursorPos();
	const ImVec2 label_pos = { pos.x + caliblabel_xoffset, pos.y + caliblabel_ybuffer };
	ImGui::SetCursorPos(label_pos);
	
	streamptr->rest.update_calib_status(); //do only once every so often?
	
	std::string caliblabel = "Calibration status:\n\t" + streamptr->rest.get_calib_status();
	ImGui::Text(" %s", caliblabel.c_str());
	ImGui::SetCursorPos( { 0, pos.y + caliblabel_ybuffer + caliblabel_h } );
	
	
	//pos = ImGui::GetCursorPos();
	button_pos = { conn_panel_width - 20 - calibbutton_w, pos.y + caliblabel_ybuffer/2 };
	ImGui::SetCursorPos(button_pos);
	
	//REV: is CALIB a new *MODE*? Nah...
	if( !streamptr->rest.am_calibrating() )
	  {
	    buttonlabel = "CALIBRATE##" + name;
	    //Allow user to click to start calibrating
	    if(ImGui::Button(buttonlabel.c_str(), { calibbutton_w, calibbutton_h }))
	      {
		streamptr->rest.start_calib();
	      }
	  }
	else
	  {
	    buttonlabel = "IN PROGRESS##" + name;
	    //ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	    ImGui::BeginDisabled();
	    //ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

	    //Just draw button grayed out and disabled...
	    if(ImGui::Button(buttonlabel.c_str(), { calibbutton_w, calibbutton_h }))
	      {
		// no op
	      }
	    
	    //ImGui::PopItemFlag();
	    ImGui::EndDisabled();
	    //ImGui::PopStyleVar();
	  }
	ImGui::SetCursorPos( {0, pos.y + calibbutton_h + calibbutton_ybuffer } );
		
	
	//update with toremove toadd
	streamptr->update_modes( toadd, toremove );
		
      } //end if tobii2 stream is CONNECTED
  } //end draw_conn_entry_tobii2
  

#ifdef WITH_REALSENSE
  void draw_conn_entry_rs( const std::string& sn, const std::string name )
  {

    int connbutton_w=150, connbutton_h=30, connbutton_buffer=10, connbutton_xoffset=20;
    int togglebutton_w=150, togglebutton_h=30, togglebutton_buffer=5, togglebutton_xoffset=40;
    
    auto initpos = ImGui::GetCursorScreenPos();
    //REV: maybe once I have set screenpos, I can follow up with normal pos? Hm.
    //auto initpos = ImGui::GetCursorPos();
    
    //Draw enough extra space for the button
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
					      { initpos.x + conn_panel_width,
						initpos.y + connbutton_h + 10 },
					      ImColor(gray));

    auto pos = ImGui::GetCursorPos(); //POS has not moved from drawing the filled rect?
    button_pos = { conn_panel_width - connbutton_w - connbutton_xoffset, pos.y + 7 }; //BOTTOM LEFT OF BUTTON?!?!
    ImGui::SetCursorPos(button_pos);
    
    auto streamptr = activestreams.get_rs( name );
    
    if( !streamptr ) //==nullptr
      {
	//Button, that makes connect
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	buttonlabel = "CONNECT##" + name;
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    activestreams.add_rs( loop, sn, name );
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
      }
    else
      {
	//Button to disconnect *UNLESS* I am streaming! (need a pointer to that streaming shit too?)
	buttonlabel = "DISCONNECT##" + name;
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    //REV: stop streaming!
	    streamptr->disable_all_streams();
	    //activestreams.remove_rs( name ); //will call its destructor etc.
	    rs_removes.insert(name);
	  }

	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
	
	std::set<std::string> toremove, toadd;
	
	for( auto&& mode : streamptr->streammodes )
	  {
	    initpos = ImGui::GetCursorScreenPos();
	    int toggle_h = 20;
	    //Draw enough extra space for the button
	    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						      { initpos.x + conn_panel_width,
							initpos.y + togglebutton_h + togglebutton_buffer },
						      ImColor(gray));

	    
	    pos = ImGui::GetCursorPos(); //topleft of MY GUY
	    const ImVec2 label_pos = { pos.x + togglebutton_xoffset, pos.y + togglebutton_buffer/2 };
	    ImGui::SetCursorPos(label_pos);
	    	    
	    if( mode.second ) //Mode is ON (draw stop streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (STOP)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    //fprintf(stdout, "REMOVE: Inserting %s\n", mode.first.c_str() );
		    toremove.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    else  // mode is OFF (draw start streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (START)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    //fprintf(stdout, "ADD: Inserting %s\n", mode.first.c_str() );
		    toadd.insert(mode.first);
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    
	    ImGui::SetCursorPos( { 0, pos.y + togglebutton_buffer + togglebutton_h } );
	  }
	
	//update with toremove toadd
	streamptr->update_modes( toadd, toremove );
		
      } //end if rs stream is CONNECTED
  } //end draw_conn_entry_rs
#endif //WITH_REALSENSE

  
  
#ifdef WITH_ITD
  void draw_conn_entry_itd( std::shared_ptr<ITD_wrapper> itd, const std::string name )
  {
    int connbutton_w=150, connbutton_h=30, connbutton_buffer=10, connbutton_xoffset=20;
    int togglebutton_w=150, togglebutton_h=30, togglebutton_buffer=5, togglebutton_xoffset=40;
    
    auto initpos = ImGui::GetCursorScreenPos();
    
    //Draw enough extra space for the button
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
					      { initpos.x + conn_panel_width,
						initpos.y + connbutton_h + 10 },
					      ImColor(gray));

    auto pos = ImGui::GetCursorPos();
    button_pos = { conn_panel_width - connbutton_w - connbutton_xoffset, pos.y + 7 };
    ImGui::SetCursorPos(button_pos);
    
    auto streamptr = activestreams.get_itd( name );
    
    //if( !itd->is_connected() ) //AND NOT STREAMING
    if( !streamptr ) //==nullptr
      {
	//Button, that makes connect
	buttonlabel = "CONNECT##" + name;

	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    fprintf(stdout, "Will add itd!\n");
	    activestreams.add_itd( loop, itd, name );
	    fprintf(stdout, "Finished it?\n");
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
      }
    else   //( itd->is_connected() )  //!activestreams.issaving() && 
      {
		
	//Button to disconnect *UNLESS* I am streaming! (need a pointer to that streaming shit too?)
	buttonlabel = "DISCONNECT##" + name;
	if( activestreams.issaving ) { ImGui::BeginDisabled(); }
	if(ImGui::Button(buttonlabel.c_str(), { connbutton_w, connbutton_h }))
	  {
	    //REV: stop streaming!
	    streamptr->disable_all_streams();
	    //activestreams.remove_itd( name ); //will call its destructor etc.
	    itd_removes.insert(name);
	  }
	if( activestreams.issaving ) { ImGui::EndDisabled(); }
	
	ImGui::SetCursorPos({0, pos.y + connbutton_h + connbutton_buffer});
	
	std::set<std::string> toremove, toadd;
	
	for( auto&& mode : streamptr->streammodes )
	  {
	    initpos = ImGui::GetCursorScreenPos();
	    int toggle_h = 20;
	    //Draw enough extra space for the button
	    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						      { initpos.x + conn_panel_width,
							initpos.y + togglebutton_h + togglebutton_buffer },
						      ImColor(gray));
	    
	    pos = ImGui::GetCursorPos();
	    const ImVec2 label_pos = { pos.x + togglebutton_xoffset, pos.y + togglebutton_buffer/2 };
	    ImGui::SetCursorPos(label_pos);
	    	    
	    if( mode.second ) //Mode is ON (draw stop streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (STOP)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toremove.insert(mode.first);
		    //activestreams.remove_itd( name ); //will call its destructor etc.
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    else  // mode is OFF (draw start streaming button)
	      {
		if( activestreams.issaving ) { ImGui::BeginDisabled(); }
		buttonlabel = mode.first + ": (START)##" + name;
		if(ImGui::Button(buttonlabel.c_str(), { togglebutton_w, togglebutton_h }))
		  {
		    toadd.insert(mode.first);
		    //activestreams.remove_itd( name ); //will call its destructor etc.
		  }
		if( activestreams.issaving ) { ImGui::EndDisabled(); }
	      }
	    
	    ImGui::SetCursorPos( { 0, pos.y + togglebutton_buffer + togglebutton_h } );
	  }
	
	//update with toremove toadd
	streamptr->update_modes( toadd, toremove );
		
      } //end if itd stream is CONNECTED
  } //end draw_conn_entry_itd
#endif //WITH_ITD

  void draw_save_button( )
  {
    const ImVec2 pos = ImGui::GetCursorPos();
    //REV: text entry field? Add "tag"?
    const int savebutton_xoffset = 30;
    const int ybuffer = 3;
    
    const ImVec2 savebutton_pos = {pos.x + savebutton_xoffset, pos.y + ybuffer}; //ImGui::GetCursorScreenPos();

    const int wid = savebutton_wid;
    const int hei = savebutton_hei - 10 - ybuffer*2;
    
    ImGui::SetCursorPos(savebutton_pos);

    
    
    if( !activestreams.issaving )
      {
	auto cansave = activestreams.check_can_save_raw(); //REV: ghetto way to ensure that I have sufficient FPS set etc.
		
	buttonlabel = "SAVE ALL CONNECTED##";
	if( !cansave ) {ImGui::BeginDisabled();}
	if(ImGui::Button(buttonlabel.c_str(), { wid, hei } ) )
	  {
	    std::string rawpath = get_user_home() + "/";
	    auto startedsaving = activestreams.start_saving_raw( rawpath );
	  }
	if( !cansave ) {ImGui::EndDisabled();}
      }
    else
      {
	buttonlabel = "STOP SAVING##";
	if(ImGui::Button(buttonlabel.c_str(), { wid, hei } ) )
	  {
	    //auto startedsaving = activestreams.start_saving_raw( rawpath );
	    auto stoppedsaving = activestreams.stop_saving_raw();
	  }
      }
    
  }
  
  void draw_conn_entry_header( const std::string& title )
  {
    const float left_space = 0.f;
    const float upper_space = 10.f;
    
    
    const ImVec2 initpos = ImGui::GetCursorScreenPos();
    //std::cout << title << " : POS IS: " << pos.x << " " << pos.y << std::endl;
    const bool draw_device_outline = false; //true;

    //REV: rectangles and lines are drawn in SCREEN**** space??!?!?! why
    //Upper space?
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
					      { initpos.x + conn_panel_width,
						initpos.y + upper_space }, ImColor(black));
    if (draw_device_outline)
      {
	//Upper Line
	ImGui::GetWindowDrawList()->AddLine({ initpos.x + left_space, initpos.y + upper_space },
					    { initpos.x + conn_panel_width, initpos.y + upper_space },
					    ImColor(white));
      }
    
    //Device Header area
    ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x + left_space, initpos.y + upper_space },
					      { initpos.x + conn_panel_width,
						initpos.y + conn_entry_height + upper_space },
					      ImColor(darkyellow));
    
    auto pos = ImGui::GetCursorPos();
    
    const ImVec2 name_pos = { pos.x + 9, pos.y + 17 };
    ImGui::SetCursorPos(name_pos);

    ImGui::Text(" %s", title.c_str());
    
    
    //REV: oooh...subdevices ;)
    //////////////// DRAW EVERYTHING ELSE HERE

    //Reset pos
    ImGui::SetCursorPos({ 0, pos.y + conn_entry_height });
    
    auto end_screen_pos = ImGui::GetCursorScreenPos();
    //std::cout << title << " : END POS IS: " << end_screen_pos.x << " " << end_screen_pos.y << std::endl;
    
    if (draw_device_outline)
      {
	//Left space
	ImGui::GetWindowDrawList()->AddRectFilled({ initpos.x, initpos.y },
						  { end_screen_pos.x + left_space, end_screen_pos.y }, ImColor(black));
	
	//Left line
	ImGui::GetWindowDrawList()->AddLine({ initpos.x + left_space, initpos.y + upper_space },
					    { end_screen_pos.x + left_space, end_screen_pos.y }, ImColor(white));
	//Right line
	const float compensation_right = 0; //17.f; //For the slider bar...?
	ImGui::GetWindowDrawList()->AddLine( { initpos.x + conn_panel_width - compensation_right,
	    pos.y + upper_space }, { end_screen_pos.x + conn_panel_width - compensation_right, end_screen_pos.y },
	  ImColor(white));
	
	//Bottom? line 
	const float compensation_bottom = 0; //1.0f;
	ImGui::GetWindowDrawList()->AddLine({ end_screen_pos.x + left_space, end_screen_pos.y - compensation_bottom },
					    { end_screen_pos.x + left_space + conn_panel_width, end_screen_pos.y - compensation_bottom },
					    ImColor(white));
      }
  }
  
  void draw_connections_panel()
  {
     auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
     
     ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
     ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImVec4)ImColor(from_rgba(0, 0, 0, 0xff)));
     
     ///////////// SAVEBUTTON //////////////
     ImGui::SetNextWindowPos({ 0, 0 }); //top-left
     ImGui::SetNextWindowSize( { (float)conn_panel_width, (float)savebutton_hei } );
     
     ImGui::Begin("SaveButton Panel", nullptr, flags );

     draw_save_button();
     
     ImGui::End(); //SaveButton Panel
     
     
     
     ///////////// CONNECTIONS PANEL //////////////
     //REV: +1 to make it "the next pixel"?
     ImGui::SetNextWindowPos({ 0, (float)savebutton_hei }); //top-left
     ImGui::SetNextWindowSize( { (float)(conn_panel_width), (float)(winheight-savebutton_hei) } );

     ImGui::Begin("Connections Panel", nullptr, flags | ImGuiWindowFlags_AlwaysVerticalScrollbar);
     
     //REV: need to know if it "IS CONNECTED"
     //Draw all connected devices (query)

     //ImGui::SetCursorScreenPos({0,savebutton_hei}); //Uh, will this work.
     //ImGui::SetCursorPos({0,savebutton_hei}); //Uh, will this work.
     
     auto tobiiconns = avail_tobii2s.get_conns();
     std::set<std::string> availt2s;
     for( const auto& conn : tobiiconns )
       {
	 //REV: conn.first is string , conn.second is struct tobii2 conn
	 const std::string t2name = "TOBII2:" + conn.second.name + "(" + conn.first + ")";
	 draw_conn_entry_header( t2name );
	 draw_conn_entry_tobii2( conn.second, t2name ); //name is IP???
	 availt2s.insert(t2name);
       }
     
     //REV: remove any streams not in availt3s
     auto streamingt2s = activestreams.streaming_tobii2s;
     for( auto& at2name : availt2s )
       {
	 size_t nerased = streamingt2s.erase( at2name );
       }
     //REV: left overs are streamings that were not in avails
     for( auto& st2 : streamingt2s )
       {
	 auto sptr = activestreams.get_tobii2( st2.first );
	 sptr->disable_all_streams();
	 sptr->stop_saving_raw();
	 tobii2_removes.insert( st2.first );
       }
     

     
     auto piconns = avail_pis->get_conns();
     std::set<std::string> availpis;
     for( const auto& conn : piconns )
       {
	 //REV: conn.first is string , conn->second is zeroconf_reply_struct, first is IP:PORT
	 const std::string piname = "PupilInvis:" + conn.second.name + "(" + conn.first + ")";
	 draw_conn_entry_header( piname );
	 
	 draw_conn_entry_pupilinvis( conn.second, piname ); //name is IP???
	 availpis.insert(piname);
       }

     //REV: remove any streams not in availpis
     auto streamingpis = activestreams.streaming_pis;
     for( auto& apiname : availpis )
       {
	 size_t nerased = streamingpis.erase( apiname );
       }
     //REV: left overs are streamings that were not in avails
     for( auto& spi : streamingpis )
       {
	 auto sptr = activestreams.get_pupilinvis( spi.first );
	 sptr->disable_all_streams();
	 sptr->stop_saving_raw();
	 pi_removes.insert( spi.first );
       }



     

     auto apiconns = avail_tobii3s->get_api_conns();
     auto rtspconns = avail_tobii3s->get_rtsp_conns();
     std::set<std::string> availt3s;
     for( const auto& apiconn : apiconns )
       {
	 //REV: conn.first is string , conn->second is zeroconf_reply_struct, first is IP:PORT
	 const std::string tobii3name = "Tobii3:" + apiconn.second.name + "(" + apiconn.first + ")";
	 
	 auto rtspitr = rtspconns.find( apiconn.first );
	 //if( !rtspconns.contains( apiconn.first ) )
	 if( rtspitr == rtspconns.end() )
	   {
	     fprintf(stderr, "REV: TOBII3 error, API conn without corresponding RTSP conn? [%s]\n", tobii3name.c_str());
	     break;
	   }
	 
	 draw_conn_entry_header( tobii3name );
	 draw_conn_entry_tobii3( apiconn.second, rtspitr->second, tobii3name ); //name is IP???

	 availt3s.insert(tobii3name);
       }
     
     //REV: remove any streams not in availt3s
     auto streamingt3s = activestreams.streaming_tobii3s;
     for( auto& at3name : availt3s )
       {
	 size_t nerased = streamingt3s.erase( at3name );
       }
     //REV: left overs are streamings that were not in avails
     for( auto& st3 : streamingt3s )
       {
	 auto sptr = activestreams.get_tobii3( st3.first );
	 sptr->disable_all_streams();
	 sptr->stop_saving_raw();
	 tobii3_removes.insert( st3.first );
       }

#ifdef WITH_ITD
     auto itdptr = avail_itds.get_itd();
     std::set<std::string> availitds;
     if( itdptr.has_value() )
       {
	 const std::string itdname = "ITD:" + itdptr.value()->get_serial();
	 draw_conn_entry_header( itdname ); // + itdptr.value()->get_serial() );
	 draw_conn_entry_itd( itdptr.value(), itdname );
	 availitds.insert( itdname );
       }

     //REV: remove any streams not in availt3s
     auto streamingitds = activestreams.streaming_itds;
     for( auto& aitdname : availitds )
       {
	 size_t nerased = streamingitds.erase( aitdname );
       }
     //REV: left overs are streamings that were not in avails
     for( auto& sitd : streamingitds )
       {
	 auto sptr = activestreams.get_itd( sitd.first );
	 sptr->disable_all_streams();
	 sptr->stop_saving_raw();
	 itd_removes.insert( sitd.first );
       }
#endif

#ifdef WITH_REALSENSE
     //rss won't return if it excepts...or will it? Wait, why not wtf?
     Timer t1;
     auto rss = avail_rss.get_avail_devices();
     std::set<std::string> availrss;
     //fprintf(stdout, "RS: check dev %lf msec\n", t1.elapsed()*1e3);
     for( const auto& conn : rss )
       {
	 const std::string rsname = "RS:" + conn.second; //+ conn first?
	 draw_conn_entry_header( rsname ); //serial, name
	 t1.reset();
	 draw_conn_entry_rs( conn.first, rsname ); //serial, name
	 availrss.insert( rsname );
       }
     
     //REV: remove any streams not in availt3s
     auto streamingrss = activestreams.streaming_rss;
     for( auto& arsname : availrss )
       {
	 size_t nerased = streamingrss.erase( arsname );
       }
     //REV: left overs are streamings that were not in avails
     for( auto& srs : streamingrss )
       {
	 auto sptr = activestreams.get_rs( srs.first );
	 sptr->disable_all_streams();
	 sptr->stop_saving_raw();
	 rs_removes.insert( srs.first );
       }
#endif
          
     ImGui::End(); //Conn Panel
     
     ImGui::PopStyleVar(); //window padding
     ImGui::PopStyleColor(); //bg col
     
     // In "model views" they have the "x" for the sub-controls thing for the device!
    //iterate through and draw each
     // draw_controls in model-view draws the actual boxes on left with x etc. etc.

     //if none, see bb thing in else { show_no-device_overlay }

     //ImGui::GetContentRegionAvail()
     //const ImVec2 pos = ImGui::GetCursorScreenPos();
     //      ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImGui::GetContentRegionAvail().y));
     //ImGui::GetWindowDrawList()->AddRectFilled(bb.GetTL(), bb.GetBR(), ImColor(dark_window_background));
     //y 0 0 bottom left.

     //REV: the draw controls automatically moves the "POS" as it draws...
     //REV: get window height gets MINI window height (i.e. in imgui in side imgui::BEGIN/END)
     //panel_y is just the height of the "title" thing (and it happens to be used for the "add source" button height as well)
     //auto windows_width = ImGui::GetContentRegionMax().x; -> uhhh

     //When I draw, I draw from top-left!?
     //And pos is from top-left as well.

     
     begin_viewport(); //Uh?
     //std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  //REV: why does streamed stuff sometimes lag behind realtime? Should I pause my time when things get "stuck"? I.e. re-zero my
  //time each time it connecs/reconnects?
  void draw_update_streams()
  {
    double nowtimes = apptimesec.elapsed();
    double nowtime=nowtimes*1e3;
    if( (framenum % 500) == 0 )
      {
	
	fprintf(stdout, "GUI: Updating [%5.1lf] msec (delta %4.1lf) (AVG: %4.1lf)\n", nowtime, nowtime-lasttime, nowtime/framenum );
      }
    framenum++;
    
    double sampletime = nowtimes + laggedsec;
    //REV: this is erasing every single time the one (it is not "dropping before", it is droping inclusive!)
    //probably due to search to "after" etc.
    //Erase (first,last) should not erase last! [first,last)!
    //It is sampling 10 msec, so it gets no inputs for some time steps?
    //No, it will just "zero from last" if there was an unzeroed one before. But, for e.g. realsense, they should be based
    //on the other pointer zeros? So, timing of infraL/infraR is different then (returns faster?) That shouldn't be right...
    // RS had "clusters" of frames arriving together!
    // REV: something is fucked with access timess?

    //REV: *BUT* this is fucking up zeroing, since I am passing unpredictable zero-numbers in.
    double slowest = activestreams.draw_update_streams( sampletime );
    double myerror = slowest - nowtimes; //negative if we need to go back...
    
    //REV: don't use "zero" as default minimum? It should be ZERO OFFSET FROM CURRENT!
    fprintf(stdout, "Now time [%lf]    Sampling [%lf] (lag=%lf)    Slowest [%lf] (error=%lf)\n", nowtimes, sampletime, laggedsec, slowest, myerror );
    laggedsec = 0;
    if( myerror < 0.050 ) { laggedsec = myerror; } //move backwards in time (jump) if we are too slow... (make sure we have some "buffer" in front of us though..., e.g. 500msec...)
    if( myerror > 2.0   ) { laggedsec = myerror; } //jump forwards in time if we are too far behind (2 seconds increment max).
    
    
    //REV: remove after drawing so it properly closes the streaming windows if I disconnected the stream.
    for( auto&& r : tobii2_removes )
      {
	activestreams.remove_tobii2( r );
      }
    tobii2_removes.clear();

    for( auto&& r : tobii3_removes )
      {
	activestreams.remove_tobii3( r );
      }
    tobii3_removes.clear();
    
    for( auto&& r : pi_removes )
      {
	activestreams.remove_pupilinvis( r );
      }
    pi_removes.clear();

#ifdef  WITH_REALSENSE
    for( auto&& r : rs_removes )
      {
	activestreams.remove_rs( r );
      }
    rs_removes.clear();
#endif

#ifdef WITH_ITD
    for( auto&& r : itd_removes )
      {
	activestreams.remove_itd( r );
      }
    itd_removes.clear();
#endif
    
    lasttime = nowtime;
  }
  
      
  void cleanup()
  {
    if(win)
      {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(win);
	glfwTerminate();
      }
  }

  void open_window( const bool _fullscreen )
  {
    fullscreen = _fullscreen;
    
    cleanup();
    
    if (!glfwInit())
      { fprintf(stderr, "Could not init glfw?\n");
	terminate(1); }
    
    //Set GLFW's error callback function
    glfwSetErrorCallback(error_callback);

#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    
    // Dynamically adjust new window size (by detecting monitor resolution)
    auto primary = glfwGetPrimaryMonitor();
    if (primary)
      {
	const auto mode = glfwGetVideoMode(primary);
	if (fullscreen)
	  {
	    winwidth = mode->width;
	    winheight = mode->height;
	  }
	else
	  {
	    winwidth = int(mode->width * 0.7f);
	    winheight = int(mode->height * 0.7f);
	  }
      }
    
    //_win = glfwCreateWindow(winwidth, winheight, "RTEYE2", NULL, NULL);
    // Create GUI Windows
    win = glfwCreateWindow(winwidth, winheight, wintitle_str.c_str(), (fullscreen ? primary : nullptr), nullptr);

    if (!win)
      {
	fprintf(stderr, "Could not create glfw window?\n");
	terminate(1);
      }

    //else, successful. Continue.
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGL())
      {
	// GLAD failed
	std::cerr << "GLAD failed to initialize :(\n";
	terminate(1);
      }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


    setStyle();
    
  } //end open_window
  
  void setGuiScale(float guiScale)
  {
    int fbw, fbh, ww, wh;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    glfwGetWindowSize(win, &ww, &wh);
    
    float pixelRatio = fbw / ww;
    
    ImGui::GetIO().FontGlobalScale = guiScale / pixelRatio;
  }
  
  //The pointer in Begin() is boolean, goes false when
  // it is x-ed out?
  
  //bool show_test_window = true;
  //  bool show_another_window = true;
  /*int state = glfwGetKey(_win, GLFW_KEY_F8);
    if (state == GLFW_PRESS)*/
  void begin_frame()
  {
    glfwPollEvents(); //get events (mouse, keyboard etc.)


    int w = winwidth; int h = winheight;
    
    glfwGetWindowSize(win, &winwidth, &winheight);

    // Set minimum size 1
    if (winwidth <= 0) winwidth = 1;
    if (winheight <= 0) winheight = 1;
    
    if( w != winwidth || h != winheight )
      {
	fprintf(stdout, "Window changed size! (%dx%d -> %dx%d)\n", w, h, winwidth, winheight);
      }
    
    
    
    int fw = fb_width;
    int fh = fb_height;
    glfwGetFramebufferSize(win, &fb_width, &fb_height);
    
    // Set minimum size 1
    if (fb_width <= 0) fb_width = 1;
    if (fb_height <= 0) fb_height = 1;

    if( fw!=fb_width || fh!=fb_height )
      {
	fprintf(stdout, "FrameBuffer changed size! (%dx%d -> %dx%d)\n", fw, fh, fb_width, fb_height);
      }
    
    auto sf = scale_factor;

    scale_factor = static_cast<float>(pick_scale_factor(win));
    winwidth = static_cast<int>(winwidth / scale_factor);
    winheight = static_cast<int>(winheight / scale_factor);
    
    
    // Reset ImGui state
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity(); //loads identity matrix into openGL? Only if openGL thinks we are moving our camera around?
    // In other words, just rotating everything in the world.

    //REV: this would only matter if I set my callback?
    //ImGui::GetIO().MouseWheel = _mouse.ui_wheel;
    //_mouse.ui_wheel = 0.f;

    ImGui_ImplOpenGL3_NewFrame(); //new frame? REV: what is this OpenGL3 shit?
    //ImGui_ImplGlfw_NewFrame(scale_factor);
    ImGui_ImplGlfw_NewFrame(); //Fuck, can't set scale factor here?
    
    ImGui::NewFrame(); //REV: RS -- They don't call this?!?!?!?!?!?!?!
    
    // ImGui::SetNextWindowSize(ImVec2(320,240));
    /*ImGui::Begin("Diagnostics", &show_fps);
    ImGui::Text("Stats:");
    // Framerate
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();*/
  }


  //REV: this is just clearning OPENGL for drawing and etc...
  void begin_viewport()
  {
    // Rendering
    glViewport(0, 0,
	       static_cast<int>(ImGui::GetIO().DisplaySize.x * scale_factor),
	       static_cast<int>(ImGui::GetIO().DisplaySize.y * scale_factor));
    
    //if (_enable_msaa) glEnable(GL_MULTISAMPLE);
    //else glDisable(GL_MULTISAMPLE);
    
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    
  }
  
  void end_frame_and_draw()
  {
    //Only if not first frame? (first frame nothing is ready? )

    if( !first_frame )
      {
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(win);
      }
  }

  // draw active streams (just a list of streams with cv::Mat and etc. -- which are filled by the background dudes)
  // They are just pointer? To a thing with the texture in it. And the "filler" side also has the pointer.
  // And it does all the work. What I do is just (1) layout (2) draw? NEED TO FEEDBACK IF THEY "X" IT OUT?!
  //    callback, stop showing stream? Keep consuming but just don't show? Nah.

  // Problem: sizing them? Only change each one's "location" if there are new ones? Where will it place new ones? Over the top?
  // Fuck sizing them at all now, just draw them as windows? Nah, size them with some layout... based on window width/height
  // Each frame is a pointer->texture and mat thing (SVS)
  // Also has e.g. "is_shown" and also source type (what is filling me?). Nah.
  // But, they are only drawn if they still exist? For example, if user stops streaming it/it disappears, I should not draw it
  // (in fact, I should delete it from my list over here!)
  
    
  //On left, make panel (really a window) -- and on it draw "text" and "buttons" etc.
  // based on "connections"

  // Walk through my "avail-sensor" guys, checking if there are any, and draw as I want for each.

  // (these are just for "available").

  // For each of them also, add a button, and if user clicks it, then "connect" to that one guy?

  // Connecting to that one guy just leads to additional stuff ("disconnect" button?)
  // And also options for output etc. But only once it is properly connected?

    
  
  
  // On "connect"
  // set "is_connected" to true (stream model or something?). Note just do if(tobii) etc. etc.

  // In the stream model, if is_connected -> initate reciever/parser (separate thread...?)
  // show options for that guy (is tobii, is itd, is RS, etc.). Nice toggle asset like RS ;(
  //   options -> (switch?) show scene. show eyes (?). do calibrate eyetracker. If calibrating (Show *CALIBRATING*) or something.
  //   success; SUCCESS. fail FAIL (uncalibrated).

  // Those open certain windows? Option to "colorize" depth? Option to log-ize depth. To set min/max range?

  // *START SAVING* (raw). Big button at top. Basically starts streaming all currently connected.
  //  While saving -- disable everything...(connection-wise). Must stop saving raw to enable anything.
  //  SO, just set global option "IS_SAVING". and check it to know if I should allow connections etc.

  //  How to gray-out button? Set alpha? Set color? Set style...
  // (put note -- (saving raw)) instead of button.

  // Note, need to be able to draw at left panel (no expansion etc.) based on last box?
  //  Just do it all at once -- iterate through, calculate size of panel entry?
  // How to put a line between them?

  //REV: only show a button to view output if the output has some kind of "streamable" function? To show it?
  // what about gaze? and sync? What about gyro?

  // Basic: we have "avail" which are running and adding possible connections to the left
  //    possible connections are what? Some common interface wrapper? That offer "streams" and "actions"?
  //    Note the parser CONSUMES EVERYTHING (dropping non-outputs).
  //  -> OK.
  // how about zeroing time?

  // So we have "possible connections" on left showing maybe just TOBII2 (IP addr -- serial). Maybe showing camera name/serial
  //     (realsense?). For ITD just "camera" (maybe get serial too?)

  // Calling "connect" will initiate both *receiver* and *parser* (which will have no outputs)
  // Then, user specifies "outputs" to add, and it will do so (problem is, e.g.for gaze it will also start adding sync?)

  // So need a nicer way than just a direct interface to get_output.
  // Also, need to know output *type*? (because I need to cast it to the right type of pointer from std::any).

  // REV: just write it by type? By source type? E.g. show everything on left by what it is, handle them all differently,
  // draw them all differently :(

  // Panel left. Add "guys" with "connect" button on left (line them up...? Added, with type?).
  // if is_connected, change (add) -- make "disconnect" option, and add controls once it connects?
  // E.g. "Show Scene" "Show infraL/infraR", "Show gaze".
  // Also other controls? E.g. "calibrate"

  // How to calibrate "Between" in time? Just re-zero? Interpolate between last and next linearly?
  // Or, exactly calibrate TIMING via interpolation between two frames, e.g. it says milliseconds 25:22 and 25:31, corresponding
  // to realtime in another stream ?

  // Need to zero in time before in space....
  
  
  // zero time will be called as soon as possible (if it is not zeroed). Or will it zero it to other streams?
  //   must zero time with same timepoints for some streams (e.g. realsense...depth and color). I need to specify which ones
  //   I have to do that for?
  
  //REV: Change/add re-layout windows if something is added?
  //REV: if person closes it, what to do? Stop streaming? Yea...that makes sense?
  // The whole point of streaming is to show it.

  //However, Have option (global?) at top "start saving all raw"

  //-> REV: "connected" and "streaming"? Connected starts buffering time-things? Nah, will fill up memory.
  //    But, yea, keep dropping after some point in past is safe (even if I don't show).
  //    I like that.
  //->    So have "streaming" and "showing"

  // And then "save RAW all streaming" (write "tag"? DATE-BLAH) (stream rate of receivers, stream rate of parsers, etc.?)
  //     And tobii should have "calibrate"?

  // And other guys should have "sync time" and "sync space"? Based on some signal...? Motion energy?

  //Common interface for getting/showing frames (from selected "output")? But different guys (e.g. tobii etc.)
  // check what type they are? Always getting "gaze" screen?
  // if "show_gaze" it will draw it on the "scene"? A separate thread getting the gaze...?
  // Normally not streaming?
  

  //REV: I guess I need to reallocate texture if size/type of underlying thing changes, etc.?
  // Otherwise, just keep it there? Keep writing over it? Better to have some ringbuffer...
  //Texture is just the thing, and I draw it "wrap it" over the part of object/view/port thing, which is just flat anyways :0
  //If I don't do nice ortho projection (?) then it will curve on the edges due to angle etc.? (of "camera")
  
  //begin_viewport is once in handle_ready_frames  (every main loop -- at end of while(window())!)
  //begin_frame is in uxwindow() (i.e. beginning every main loop)
  //end_frame is at the beginning as well (in uxwindow())

  
  
  //render_2d_view is in draw_viewport
  // draw_viewport is in handle_ready_frames
  /*
    at beginning of render_2d_view : (then iterate through key-value-pair in layout, and render each header/footer, and 
// the stream itself (show_frame)
glViewport(0, 0,
            static_cast<GLsizei>(win.framebuf_width()), static_cast<GLsizei>(win.framebuf_height()));
        glLoadIdentity();
        glOrtho(0, win.width(), win.height(), 0, -1, +1);

        auto layout = calc_layout(view_rect);*/

  //Note framebuffer is part of RAM that is mapped to drive display.
  //I guess it is the framebuffer of the WHOLE WINDOW? 
  
  
  //REV: could make mouse click detect target inside frame (pixel) and  use that for calibration etc...
  //Need way to
  //1) zero time
  //2) zero space
  //3) save all configs?


  //REV: I could keep textures of previous frames in GPU memory...and refer to them for skipped frames?

  //upload() (and upload_image()) in rendering in texture_buffer is what does the actual call to glDraw2D etc.
  //stream_model::upload_frame inside of model-views
  //viewer_model::upload_frame (note, this finds the correct corresponding object in our list of objects based on
  // where the frame comes from (its internal ID) -- which we've sorted out objects by - -and adds to that queue.
  // And in handle_ready_frames, it calls that.

  //first begin_viewport, then imbegin("viewport"), then try draw_viewport -> render_2d_view
  //draw_viewport at end of handle_ready_frames. Note it is inside "imgui::begin() and end()"

  //yea, I think it is the show_frame, show_stream_header, show_stream_footer (of stream_mv)
  // which is stream_modelview thin

  //They do adjustratio to make the "grid" for the item (regardless of its size). Width preferred. I.e. so it is never bigger
  //than the grid it is in? Uh...

  // Last question -- what makes the windows? Of the RS?
  // It can't move! It's static thing with header/footer! Very thin line drawn around edge...

  //draw_texture (in rendering.h) just draws the background gray box, over which they draw the actual content (header, footer,
  // and the frame itself, with grow(-3)? No, the grow is for space between windows in the viewport!

  //So, two ways. Use the natural way (and place the windows myself each time?). Then I can put custom stuff on windows?

  //Fuck it, fow now just draw windows myself...? (what happens if I don't draw each frame will it disappear?)
  // Also, scale of window...?
  
  //REV: see begin_frame -> poll events, set _width/_height from glfwGetWindowSize(_win, &_width, &_height);
  //glfwGetFramebufferSize(_win, &_fb_width, &_fb_height); -> REV: wat?

  //see begin_viewport, etc.. Scale size wtf?. Shape is from the rectangle (calculates layout...automatically?
  //Even if they move it?
  
};





int main( int argc, char** argv )
{
  uxwindow win;
  bool fullscreen=true;
  win.open_window(fullscreen);
  

  while( win )
    {
      win.draw_connections_panel();
      win.draw_update_streams();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

  return 0;

} //end MAIN











