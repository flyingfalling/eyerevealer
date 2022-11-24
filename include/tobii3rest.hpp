//REV: tobii3 rest interface
//REV: todo -- make it so thread waiting for reply from the post (e.g. calibrate). Otherwise it blocks/hangs for
//   like 500 msec...
//    thread to connect to various guys (e.g. starting RTSP servers). Otherwise, it hangs.

#pragma once

//standard libs
#include <cstdio>
#include <cstdlib>
#include <iostream>

//Other libs
//JSON
#include <nlohmann/json.hpp>

//HTTP rest interface...
#include <httprest.hpp>

// my includes
#include <utilities.hpp>


using namespace nlohmann;


//REV: REST server interface/status for Tobii glasses 3 eye tracker.
//Similar to TOBII2, but format is different with "signals" and shit.
//Furthermore, calibration marker live recognition is not streamed, but rather detected as HTTP response signal (fuck) for 3 seconds
//when we send a command?

//Tobii3 has no "projects" or "participants" -- just metadata.
//calibrate!emit-markers will emit for 3 sec or until calib done (?)
//calibrate!run starts calib process.
//signal calibrate:marker will provide data in format: [1.869804, [-210.64152, 72.88064, 686.21405], [0.64479, 0.40370]]
//First value is timestamp (REV: wtf?)? PTS time? Second is 3D in mm, third is 2d scene.
//Calibs then stay until you calib again. you can't "delete" a calib and return to state before it.
//Start recording -- set name of recording -- stop recording.

//Controlled shutdown (hold down recording unit) will stop recording.
//Uncontrolled (remove battery, SD card, or holding down button for 12 sec) will corrupt recording...


//recorder!start, !stop, etc.? Then, during recording, you can set metadata etc. (only during recording? :(). After stopped, can be renamed etc...
//!cancel will just cancel it and not save any data...

//REV: URL is specified by the "API" thing I get (IP and port!)

//REV: there are subsystems, like system/battery etc.! Called "children" etc. You can make them?

//REV !run returns true if success, else false. But, WHEN does it return?! Fuck....Anyways, it waits for the post response?
//REV: it happens rather quickly?

//REV: !marker is "timestamp", "position2D" and "position3D"?

struct tobii3_REST
{
public:
  std::string base_url;
  std::string port;
  uint64_t lastid=0;
  std::string calibstatus;
  Timer last_calib_started;
  
  const double calib_timeout_sec = 3.0;
  
  bool currently_calibrating; //REV: fuck need to mux it.
  bool done_calibrating; //REV: there is no way to check for calibration?
  bool emit_calibrating;
  
  
  //REV: Will access it async so better be here ugh.
  //vec2f marker_pos_2d;
  //vec3f marker_pos_3d;
  timed_buffer<vec2f, double> marker_pos_2d;
  timed_buffer<vec3f, double> marker_pos_3d;
  
  std::mutex mu;
  
  //REV: how to check if I have calibrated or not (can I query it?)
  //http://<g3-address>/rest/<path-to-api-element>
  //REV: so http://10.10.10.10/rest/calibrate!run

  //REV: to check marker position, I do http://.../rest/calibrate:!emit-markers and then
  //REV: do a GET at http://.../rest/calibrate:marker.
  //That is what, a text stream? Fuck? Not JSON?. Note, calibrate!run (not !start!)
  
  //REV: assume IP4?!?!?! (fuck...IP6 for tobii2, but IP6 for tobii3 doesn't work for RTSP?)
  tobii3_REST( const std::string& burl, const int _port=80 )
    : base_url(burl), port(std::to_string(_port)), calibstatus("UNCALIBRATED"), currently_calibrating(false), done_calibrating(false), emit_calibrating(false)
  {
    fprintf(stdout, "Creating TOBII3 REST struct: [%s]\n", burl.c_str() );

    marker_pos_2d.set_timebase_hz_sec(1.0);
    marker_pos_3d.set_timebase_hz_sec(1.0);
    
  }

    
  std::string make_property_str( const std::string& obj_name, const std::string& prop_name )
  {
    std::string url = "/rest/" + obj_name + "." + prop_name;
    return url;
  }
  
  std::string make_action_str( const std::string& obj_name, const std::string& action_name )
  {
    std::string url = "/rest/" + obj_name + "!" + action_name;
    return url;
  }

  std::string make_signal_str( const std::string& obj_name, const std::string& signal_name )
  {
    std::string url = "/rest/" + obj_name + ":" + signal_name;
    return url;
  }

  //REV: action, body should exist even if empty! (empty array [])
  //REV: response is just JSON I guess? :(
  //error-info property in the response...? Can provide info about shit.
  //REV: requires empty body no matter what! Empty array must be done with this in nhlomman 
  json do_action( const std::string& obj_name, const std::string& action_name, const json& params=json::array())
  {
    auto actionurl = make_action_str( obj_name, action_name );
    //REV: how to check JSON is empty?
    auto results = post_json( base_url, actionurl, params, port );
    return results;
  }
  
  json get_signal( const std::string& obj_name, const std::string& signal_name )
  {
    auto signalurl = make_signal_str( obj_name, signal_name );
    json j;
    {
      //const std::lock_guard<std::mutex> lock(mu);
      j["id"] = 0; //lastid;
      //++lastid;
    }
    auto results = post_json( base_url, signalurl, j, port );
    return results;
  }

  json get_property( const std::string& obj_name, const std::string& prop_name )
  {
    auto propertyurl = make_property_str( obj_name, prop_name );
    auto results = get_json( base_url, propertyurl, port );
    return results;
  }
  
  
  //REV: fuck, make some timeout or else it will never finish?
  void start_tobii3_calibration()
  {
    auto result = do_action( "calibrate", "run" );
    //REV: how long will this hang? It won't? It will either return true or false...
    //fprintf(stdout, "REV: TOBII3 calib result started, result: (true if calibration succeeded?)\n");
    //std::cout << result << std::endl;
    //REV this may
    if( !result.is_boolean() )
      {
	fprintf(stderr, "Didn't get expected JSON format for calib!run, which is just single boolean...\n");
	std::cerr << result << std::endl;
	return;
      }
    
    if( true == result )
      {
	fprintf(stdout, "Got a successful calib!\n");
	done_calibrating = true;
	currently_calibrating = false;
	emit_calibrating = false;
	calibstatus = "SUCCESS";
      }
    else
      {
	fprintf(stdout, "Got a failed calib!\n");
	done_calibrating = false;
	currently_calibrating = false; //REV: "keep trying". Will stop after 3 seconds...
	//emit_calibrating = true;
	calibstatus = "FAILED";
      }
     
    return;
  }

  //REV: emits for 3 seconds...
  //REV: give a separate button for this? Or just start
  //When we want to do calib? And draw if we are...
  void emit_tobii3_calibration_markers()
  {
    auto result = do_action( "calibrate", "emit-markers" );
    //REV: how long will this hang? It won't? It will either return true or false...
    //fprintf(stdout, "REV: Will emit markers! (True if successfully started emitting?)\n");
    //std::cout << result << std::endl;
    if( !result.is_boolean() || false == result)
      {
	fprintf(stderr, "WTF big error, Tobii G3 something is wrong with calling emit-markers?!\n");
      }
    return;
  }

  //REV: will set class
  //REV: no need to join the thread (no need to wait).
  void get_tobii3_calibration_markers()
  {
    //auto result = get_property( "calibrate", "marker" );
    auto result = get_signal( "calibrate", "marker" );
    //fprintf(stdout, "REV: Getting/setting marker!\n");

    std::cout << result << std::endl;
    if( !result.is_array() || result.size() != 3 )
      {
	fprintf(stderr, "Failed to get any calibration markers even though I was expecting them?\n");
	return;
      }

    double marker_time_sec = result[0];
    if( 3 == result[1].size() )
      {
	vec3f v3( result[1][0], result[1][1], result[1][2] );
	marker_pos_3d.add( v3, marker_time_sec,1 );
      }
    if( 2 == result[2].size() )
      {
	vec2f v2( result[2][0], result[2][1] );
	marker_pos_2d.add( v2, marker_time_sec,1 );
      }
    
    return;
  }

  //REV: add other API stuff
  // E.g. get project info by doing GET on /api/projects
  // Delete project by doing post /api/projects/PROJ_ID/delete or some shit.

  //REV: timestamps seem to be from the time I hit "connect"...so is that 0 PTS as well? Or is it related to something else?
  // (opening some connection wtf?).
  void update_calib_status()
  {
    
    //REV: try "run"?
    const std::lock_guard<std::mutex> lock(mu);
    if( !emit_calibrating)
      {
	return;
      }
    
    if( last_calib_started.elapsed() >= calib_timeout_sec )
      {
	currently_calibrating = false;
	done_calibrating = false; //not successfully calib? lol
	emit_calibrating = false;
	calibstatus = "FAILED"; //Is this right?
	return;
      }
    
    emit_tobii3_calibration_markers(); //maybe not call every time lol? Oh well.
    get_tobii3_calibration_markers();
    
    return;
  }

  bool am_calibrating()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return currently_calibrating;
  }
  
  std::string get_calib_status()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return calibstatus;
  }

  bool check_calib_markers_zeroed()
  {
    return marker_pos_2d.iszeroed();
  }
  
  void zero_calib_markers( const double timesec, const double corresptosec )
  {
    const std::lock_guard<std::mutex> lock(mu);
    marker_pos_2d.rezero_time(timesec);
    marker_pos_3d.rezero_time(timesec);
    marker_pos_2d.set_zerotime_clock_sec(corresptosec);
    marker_pos_3d.set_zerotime_clock_sec(corresptosec);
  }
  
  std::optional<timed_buffer_element<vec2f,double>> get_calib_marker_2d_from_time( const double timesec, const bool dropbefore=true )
  {
    const std::lock_guard<std::mutex> lock(mu);
    double deltasec=0.100;
    auto val = marker_pos_2d.get_timed_element_fromzero_secs( timesec, deltasec, relative_timing::EITHER, dropbefore );
    return val;
  }

  std::optional<timed_buffer_element<vec2f,double>> get_last_calib_marker_2d()
  {
    auto val = marker_pos_2d.get_last( );
    return val;
  }
  
  //REV emit markers etc. may be forbidden if i try to do it while already doing it.
  void start_calib()
  {
    {
      const std::lock_guard<std::mutex> lock(mu);
      last_calib_started.reset();
      done_calibrating = false;
      currently_calibrating = true;
      emit_calibrating = true;
    }
    
    update_calib_status();
    
    start_tobii3_calibration(); //REV: and...?
        
    return;
  }
    
}; //end struct tobii3_REST


