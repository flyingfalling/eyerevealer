
#pragma once

//standard libs
#include <cstdio>
#include <cstdlib>
#include <iostream>

//Other libs
//JSON
#include <nlohmann/json.hpp>

//BOOST ASIO (asynch IO)
//#include <boost/asio.hpp>
//#include <boost/asio/connect.hpp>
//#include <boost/asio/ip/tcp.hpp>

//HTTP client. (BOOST BEAST)
//#include <boost/network/protocol/http/client.hpp>
//#include <boost/network/uri.hpp>
//#include <boost/beast/core.hpp>
//#include <boost/beast/http.hpp>
//#include <boost/beast/version.hpp>


//HTTP rest interface...
#include <httprest.hpp>

// my includes
#include <utilities.hpp>


using namespace nlohmann;


//REV: REST server interface/status for Tobii glasses 2 eye tracker.
struct tobii2_REST
{
public:
  std::string base_url;

  std::string projid;
  std::string partid;
  
  std::string calibid;
  std::string calibstatus;

  bool currently_calibrating; //REV: fuck need to mux it.
  bool done_calibrating;

  //REV: assume IP6
  tobii2_REST( const std::string& burl )
    : base_url(burl), calibstatus("UNCALIBRATED"), currently_calibrating(false), done_calibrating(false)
  {
    fprintf(stdout, "Creating TOBII2 REST struct: [%s]\n", burl.c_str() );
  }
  
  std::string create_tobii2_project()
  {
    auto j = post_json( base_url, "/api/projects" );
    return j["pr_id"];
  }
  
  std::string create_tobii2_participant( const std::string& proj_id )
  {
    json j;
    j["pa_project"] = proj_id;
    auto jr = post_json( base_url, "/api/participants", j );
    return jr["pa_id"];
  }
  
  std::string create_tobii2_calibration( const std::string& proj_id, const std::string& partic_id )
  {
    json j;
    j["ca_project"] = proj_id;
    j["ca_type"] = "default";
    j["ca_participant"] = partic_id;
    auto jr = post_json( base_url, "/api/calibrations", j );
    return jr["ca_id"];
  }
  
  void start_tobii2_calibration( const std::string& calib_id )
  {
    std::string caliburl = "/api/calibrations/" + calib_id + "/start";
    auto jr = post_json( base_url, caliburl );
    return; //no ret?
  }

  //REV: add other API stuff
  // E.g. get project info by doing GET on /api/projects
  // Delete project by doing post /api/projects/PROJ_ID/delete or some shit.
  
  void update_calib_status()
  {
    if( currently_calibrating )
      {
	auto action = "/api/calibrations/" + calibid + "/status";
	auto rj = get_json( base_url, action );
	if( rj.contains( "ca_state" ) )
	  {
	    calibstatus = rj["ca_state"];
	    if( calibstatus == "failed" || calibstatus == "calibrated" )
	      {
		currently_calibrating=false;
		done_calibrating=true;
	      }
	    
	  }
	else
	  {
	    calibstatus = "calibrating/unavailable";
	  }
      }
    return;
  }

  bool am_calibrating() const
  {
    return currently_calibrating;
  }
  
  std::string get_calib_status() const
  {
    return calibstatus;
  }
  
  //REV: get a future? (i.e. do it async?)
  std::string wait_calib_status()
  {
    size_t maxtimeouts=30;
    std::string finalstatus = wait_for_status( maxtimeouts,
					       base_url,
					       "/api/calibrations/" + calibid + "/status",
					       "ca_state",
					       {"failed", "calibrated"} );
    return finalstatus;
  }


  void delete_calib()
  {
    if( done_calibrating )
      {
	auto delaction = "/api/calibrations/" + calibid;
	auto rj = delete_json( base_url, delaction );
	calibid.clear();
      }
  }
  
  void start_calib()
  {
    update_calib_status();
    
    if( currently_calibrating )
      {
	fprintf(stdout, "You are currently calibrating! Returning\n");
	return;
      }
    
    if( done_calibrating )
      {
	if( calibstatus == "success" )
	  {
	    fprintf(stdout, "You can not recalibrate on the same calibration...must make a new calibration participant etc.\n");
	  }
      }
    
    done_calibrating = false;
    currently_calibrating = true;
    
    createcalib();
    
    start_tobii2_calibration( calibid ); //can I recalibrate ?
    
    return; //user should wait_calib_status()
  }
  
  void createcalib()
  {
    fprintf(stdout, "REV: TOBII2: creating calib!\n" );
    
    projid = create_tobii2_project();
    fprintf(stdout, "REV: created tobii project ID [%s]\n", projid.c_str());
    
    partid = create_tobii2_participant( projid );
    fprintf(stdout, "REV: created tobii participant ID [%s]\n", partid.c_str());
    
    calibid = create_tobii2_calibration( projid, partid );
    fprintf(stdout, "REV: created and starting calibration for calibration id [%s]\n", calibid.c_str() );
    
    return;
    //User calls wait_calib_status( caliid ) and gets "failure" or "success" (or ""). Could do std::optional.
  }
};


