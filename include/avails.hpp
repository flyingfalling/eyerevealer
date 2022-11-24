
#pragma once

struct tobii_glasses2_info
{
  std::string myclass;
  std::string name;
  std::string interface;
  int restport;
  std::string ip4_addr;
  std::string version;
  std::string id;
  
  std::string ip6_addr;
  int live_port;
  
  sockaddr_in6 live_i6addr;
  socklen_t live_i6len; //can I always get this back from sizeof()?
  
  tobii_glasses2_info()
  { }

  //REV: s can't be const...because I lock the mutex to get info.
  tobii_glasses2_info( socket_udp6& s, const std::string& msg )
  {
    live_port = s.get_other_port();
    ip6_addr = s.get_other_addr_str();
    
    live_i6addr = s.get_other_addr(live_i6len);
    
    auto j = json::parse(msg);

    if( !j.contains("type") || j["type"] != "identity" )
      {
	std::fprintf(stderr, "Passed msg is not correct type of message to construct tobii glasses2 conn [%s]\n", msg.c_str());
	exit(1);
      }
    myclass = j["class"];
    if( myclass != "glasses2" )
      {
	std::fprintf(stderr, "Class is not glasses2! Can't construct glasses2_info [%s]\n", myclass.c_str());
	exit(1);
      }
    name = j["name"];
    version = j["version"];
    id = j["id"];
    restport = j["port"];
    interface = j["interface"];
    if( j.contains("ipv4") )
      {
	ip4_addr = j["ipv4"];
      }
  }
};


//REV: problem is this will remove the "service" but not the "thing"? No, it will, and when I remove the service, I make sure to
// also remove the thing, which has a "connect" thing inside of it?
//REV: no, even tobii2 guy just has totally separate things for the "available" thing and the "connection" thing?
// So -- start loop of zeroconf services -- keeps up to date of those.
// Then, separate loop that takes that zeroconf looper as argument (mutexed),
// and checks if any of them are pupil invis or tobii 3 etc, and adds/removes accordingly (the IP and etc.)
// Then, each of those can be attempted to be "connected" to for streaming. OK.

// Both gaze and video are (separate) RTSP streams for pupil invis -- connect to both in own loop, and buffer each one separately
// as an absolute buffer of what? AV_PACKET? Yea...? And then, consume (decode) those AV packets appropriately...
// Note that the format video stream is in the stream list...just select the "best" (first?) one.


static const int64_t MDNS_QUERY_TIMEOUT_MSEC = 600;
static const int64_t AVAIL_TIMEOUT_MSEC = 700;
static const double  MDNS_DISCONNECT_TIMEOUT_SEC = 15.0;

struct avail_zeroconf_services
  : public looper
{
public:
  std::map<std::string,zeroconf_service_reply_struct> conns;
  std::thread mythread;
  std::mutex mu;
  zeroconf_service_discoverer zsd;
  std::string service;
  
  size_t get_n_avail()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return conns.size();
  }

  //REV: just copy them and filter for the stuff we want/need...
  //Spin up a zeroconf avail loop for the service we need outside... we only need ne anyways.
  //REV: this will return zeroconf_service_reply_struct --
  // will have:
  // std::string srcaddrport;
  // std::string name;
  // std::string svcname;
  // int dstport;
  //REV can I make a more unique name? Can same name be on multiple guys? I can't search for specific guy...I need to select based on
  // e.g. "type"
  std::map<std::string,zeroconf_service_reply_struct> get_conns()
  {
    const std::lock_guard<std::mutex> lock(mu);
    auto ret = conns;
    return ret;
  }

  avail_zeroconf_services( const std::string& _service = "_http._tcp.local." )
    : looper(), service(_service)
  {
  }

  ~avail_zeroconf_services()
  {
    stop();
  }

  void stop()
  {
    std::fprintf(stdout, "IN STOP: available zeroconf service discovery thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: available zeroconf service discovery thread (Tag=[%s])\n", tag.c_str());
  }
  
  void start( loopcond& loop )
  {
    mythread = std::thread( &avail_zeroconf_services::doloop, this, std::ref(loop) );
    //mythread.detach(); //No need to join. Resources released when execution stops.
  }

  void doloop( loopcond& loop )
  {
    while(localloop() && loop())
      {
		
	auto results = zsd.query_service( service );

	{ //REV: fixed 27 oct -- was doing this before wrong place only if res>0
	  const std::lock_guard<std::mutex> lock(mu);
	  conns.clear();
	  if( results.size() > 0 )
	    {
	      for( size_t x=0; x<results.size(); ++x )
		{
		  //REV: wtf no copy constructor...? I'm so confused... Oh it is const or some shit?
		  auto r = results[x];
		  //update it? Clear any that are gone now.
		  conns[r.srcaddrport] = r;
		}
	    }
	}
	//Short...but could predicate it on my localloop/doloop
	std::this_thread::sleep_for(std::chrono::milliseconds(MDNS_QUERY_TIMEOUT_MSEC));
      } //end while loop
    
    std::fprintf(stdout, "END: zeroconf service discovery loop (Tag=[%s])\n", tag.c_str());
  }
  
}; //end avail_zeroconf_services





struct avail_zeroconf_services_set
{
private:
  //std::map< std::string, std:shared_ptr<avail_zeroconf_services> > zcss;
  std::map<std::string,std::shared_ptr<avail_zeroconf_services>> zcss;
  std::mutex mu;

public:
  //will "get" return...pointer?
  std::shared_ptr<avail_zeroconf_services> get_avail_zeroconf_for_service( const std::string& _service, loopcond& myloop )
  {
    const std::lock_guard<std::mutex> lock(mu);
    auto itr = zcss.find( _service );
    if( itr == zcss.end() )
      {
	zcss[ _service ] = std::make_shared<avail_zeroconf_services>( _service );
	itr = zcss.find( _service );
	itr->second->start( myloop );
      }
    return itr->second; //REV: it's a POINTER to the avail_zeroconf_services
  }
};


//REV: separate into API and RTSP avails? Then combine them later?
struct avail_tobii3
  : public looper
{
  
  const std::string apiservice = "_tobii-g3api._tcp.local."; //REV: no period on end...? Doesn't matter? But need at least .local
  const std::string rtspservice = "_rtsp._tcp.local."; //REV: period on end...
  
  std::thread mythread;
  std::shared_ptr<avail_zeroconf_services_set> zsvcset;

  std::map<std::string,zeroconf_service_reply_struct> apiconns;
  std::map<std::string,zeroconf_service_reply_struct> rtspconns;

  std::mutex mu;
  
  avail_tobii3( std::shared_ptr<avail_zeroconf_services_set> _zsvcset )
    : zsvcset(_zsvcset)
  {
    
  }

  ~avail_tobii3()
  {
    stop();
  }

  //start
  void start( loopcond& loop )
  {
    mythread = std::thread( &avail_tobii3::doloop, this, std::ref(loop) );
  }
  
  //stop
  void stop()
  {
    std::fprintf(stdout, "IN STOP: avail tobii3 discovery thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    
    //REV: maybe can never join because it never lets me out?
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: avail tobii3 discovery thread (Tag=[%s])\n", tag.c_str());
  }

  const std::string get_tobii3_serial( const std::string& name ) const
  {
    const std::string tocompare = "TG03B-080200014621";
    if( name.size() < tocompare.size() )
      {
	fprintf(stderr, "REV: unrecognized tobii3 name, must be at least [%lu] characters long (e.g. [%s]). Your size: [%lu]   Your string: [%s]\n", tocompare.size(), tocompare.c_str(), name.size(), name.c_str());
	return std::string();
      }

    //REV: if I don't copy this it will literally get a substring of name (i.e. pointers).
    std::string serial = std::string( name.begin(), name.begin()+tocompare.size() ).c_str(); //Nasty way to get a deep copy...
    if( serial.substr(0,2).compare("TG") != 0 )
      {
	fprintf(stderr, "REV: Warning, tobii3 serial should start with two characters TG, but does not... [%s]\n", serial.c_str());
      }
    return serial;
  }

  void doloop( loopcond& loop )
  {
    auto apiptr = zsvcset->get_avail_zeroconf_for_service( apiservice, loop );
    auto rtspptr = zsvcset->get_avail_zeroconf_for_service( rtspservice, loop );
    
    while(localloop() && loop())
      {
	auto apizcconns = apiptr->get_conns();
	auto rtspzcconns = rtspptr->get_conns();
	
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  auto apitormv = apiconns; //REV: wait...this won't ref right?
	  
	  if( apizcconns.size() > 0 )
	    {
	    
	      for( auto iter=apizcconns.begin(); iter != apizcconns.end(); ++iter )
		{
		  auto name = get_tobii3_serial(iter->second.name) + " @ " + iter->second.srcaddr;
		  auto addrport = iter->second.srcaddrport;

#ifndef TOBII3_ALLOW_IP6
		  auto isip6 = iter->second.ip6;
		  if( isip6 )
		    { continue; }
#endif
		  iter->second.name = name; //REV: overwrite name...dangerous?
		  if( !apiconns.contains( name ) )
		    {
		      fprintf(stdout, "ADDING TOBII3: API CONN [%s] (%s)\n", name.c_str(), iter->second.tostr().c_str());
		      apiconns[ name ] = iter->second;
		    }
		  else //REV: else, check it as existing! Reset its thing
		    {
		      //fprintf(stdout, "ALREADY CONTAINS TOBII3: API CONN [%s] (%s)\n", name.c_str(), iter->second.tostr().c_str());
		      size_t nerased = apitormv.erase( name );
		      if( nerased != 1 )
			{
			  fprintf(stderr, "Should exist, but failed?\n");
			  exit(1);
			}
		    }

		  apiconns[name].reset_timeout();
		  //REV: can't use name for map in tobii3 because we may have same NAME but different IPs (ip6 and ip4)...
		
		}
	    }
	
	  auto rtsptormv = rtspconns;
	  
	  if( rtspzcconns.size() > 0 )
	    {
	      for( auto iter=rtspzcconns.begin(); iter != rtspzcconns.end(); ++iter )
		{
		  auto name = get_tobii3_serial(iter->second.name) + " @ " + iter->second.srcaddr;
		  auto addrport = iter->second.srcaddrport;
		  iter->second.name = name; //REV: overwrite name...dangerous?
#ifndef TOBII3_ALLOW_IP6
		  auto isip6 = iter->second.ip6;
		  if( isip6 )
		    { continue; }
#endif
		  if( !rtspconns.contains( name ) )
		    {
		      fprintf(stdout, "ADDING TOBII3: RTSP CONN [%s] (%s)\n", name.c_str(), iter->second.tostr().c_str());
		      rtspconns[ name ] = iter->second;
		    }
		  else
		    {
		      //fprintf(stdout, "ALREADY CONTAINS TOBII3: RTSP CONN [%s] (%s)\n", name.c_str(), iter->second.tostr().c_str());
		      size_t nerased = rtsptormv.erase( name );
		      if( nerased != 1 )
			{
			  fprintf(stderr, "Should exist, but failed?\n");
			  exit(1);
			}
		    }
		  rtspconns[name].reset_timeout();
		  
		}
	    }
	  
	  //Any left over in rtsptormv and apitormv should be removed from conns
	  for( auto it=rtsptormv.begin(); it!=rtsptormv.end(); ++it )
	    {
	      std::string name = it->first;
	      double elapsed = rtspconns[name].get_elapsed_seconds();
	      if( elapsed > MDNS_DISCONNECT_TIMEOUT_SEC )
		{
		  size_t nerased = rtspconns.erase( name );
		  if( nerased != 1 )
		    {
		      fprintf(stderr, "REV: wtf erased not one?!?!\n");
		      exit(1);
		    }
		  else
		    {
		      fprintf(stdout, "REMOVED tobii3 [%s] rtsp (Time since last detection [%lf] sec, > timeout [%lf])\n", name.c_str(), elapsed, MDNS_DISCONNECT_TIMEOUT_SEC);
		    }
		}
	      else
		{
		  fprintf(stdout, "Detected missing tobii3 [%s] rtsp (Time since last detection [%lf] sec, timeout [%lf])\n", name.c_str(), elapsed, MDNS_DISCONNECT_TIMEOUT_SEC);
		}
	    }


	  //REV: remove them if they are not here, and the last update time is bigger than timeout...
	  for( auto it=apitormv.begin(); it!=apitormv.end(); ++it )
	    {
	      std::string name = it->first;
	      double elapsed = apiconns[name].get_elapsed_seconds();
	      if( elapsed > MDNS_DISCONNECT_TIMEOUT_SEC )
		{
		  size_t nerased = apiconns.erase( name );
		  if( nerased != 1 )
		    {
		      fprintf(stderr, "REV: wtf erased not one?!?!\n");
		      exit(1);
		    }
		  else
		    {
		      fprintf(stdout, "REMOVED tobii3 [%s] api (Time since last detection [%lf] sec, > timeout [%lf])\n", name.c_str(), elapsed, MDNS_DISCONNECT_TIMEOUT_SEC);
		    }
		}
	      else
		{
		  fprintf(stdout, "Detected missing tobii3 [%s] api (Time since last detection [%lf] sec, timeout [%lf])\n", name.c_str(), elapsed, MDNS_DISCONNECT_TIMEOUT_SEC);
		}

	    }
	}
	
	//REV: better way to do zeroconf?
	//REV: should base it on timeouts?
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	
      }
  } //end doloop (tobii3 avail)



  std::map< std::string, zeroconf_service_reply_struct> get_api_conns()
  {
    const std::lock_guard<std::mutex> lock(mu);
    auto ret = apiconns;
    return ret;
  }

  std::map< std::string, zeroconf_service_reply_struct> get_rtsp_conns()
  {
    const std::lock_guard<std::mutex> lock(mu);
    auto ret = rtspconns;
    return ret;
  }
  
}; //end avail_tobii3


struct avail_pupilinvis
  : public looper
{
  const std::string service = "_http._tcp.local.";
  const std::string expected  = "PI monitor";
  std::thread mythread;
  std::shared_ptr<avail_zeroconf_services_set> zsvcset;
  //IP addr, name, dstport
  //REV: actually query each one, get the JSON, and use that info? I.e. "phone" etc.?
  //REV: sort them by...serial number or some shit?
  //std::map<std::string,std::shared_ptr<pupilinvis_interface>> conns;
  std::map<std::string,zeroconf_service_reply_struct> conns;
  std::mutex mu;
  
  avail_pupilinvis( std::shared_ptr<avail_zeroconf_services_set> _zsvcset )
    : zsvcset(_zsvcset)
  {
    
  }

  ~avail_pupilinvis()
  {
    stop();
  }

  //start
  void start( loopcond& loop )
  {
    mythread = std::thread( &avail_pupilinvis::doloop, this, std::ref(loop) );
  }
  
  //stop
  void stop()
  {
    std::fprintf(stdout, "IN STOP: avail pupilinvis discovery thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();

    //REV: maybe can never join because it never lets me out?
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: avail pupilinvis discovery thread (Tag=[%s])\n", tag.c_str());
  }

  

  //REV: not nice...I'm confusing making "info" available, with using it here... because I'm doing the query here.
  //REV: note the query goes elsewhere too? But query here should specify if it is shown over there...?
  //doloop
  //REV: should really just signal here from the other guy whenever there is a change... ;/
  //NOTE, query status just updates my status (i.e. am I "good" or not). there could be another way to return stuff? I.e.
  // Just return the raw status info???? STATUS is the JSON that is returned!
  void doloop( loopcond& loop )
  {
    while(localloop() && loop())
      {
	auto ptr = zsvcset->get_avail_zeroconf_for_service( service, loop );
	auto zcconns = ptr->get_conns();

	{
	  const std::lock_guard<std::mutex> lock(mu);
	  auto tormv = conns;
	
	  if( zcconns.size() > 0 )
	    {
	    
	      for( auto iter=zcconns.begin(); iter != zcconns.end(); ++iter )
		{
		  auto name = iter->second.name;
		  //fprintf(stdout, "Got CONN: [%s] (doloop of avail pupilinvis)\n", name.c_str());
		  std::string pistr = name.substr( 0, expected.size() ); //start idx, length (not end indx!)
		  if( pistr.compare( expected ) == 0 )
		    {
		      //So let user click "connect". Should fill some other "possible connection" type thing?
		      const std::string id = iter->second.name; //Big name...shit

		      auto founditr = conns.find( id ); //can't just use "contains"?
		      if( conns.end() == founditr )
			{
			  fprintf(stdout, "AVAIL PI DOLOOP: FOUND NEW PI, Adding [%s]\n", name.c_str());
			  //auto tmpptr = std::make_shared<pupilinvis_interface>( iter->second ); //iter->second.srcaddr, iter->second.dstport );
			  //auto ret = conns.emplace( id, tmpptr );
			
			  //Fuck, write over it? Why not just update the info?
			  conns[id] = iter->second; //Overwriting it?
			}
		      
		      conns[id].reset_timeout();
		      size_t nerased = tormv.erase(id);
		      //founditr = conns.find( id );
		    } //end if is PI service
		} //end for all zerconf conns...
	    } //end if detected more than zero zeroconf conns...

	  for( auto it=tormv.begin(); it!=tormv.end(); ++it )
	    {
	      std::string name = it->first;
	      double elapsed = conns[name].get_elapsed_seconds();
	      if( elapsed > MDNS_DISCONNECT_TIMEOUT_SEC )
		{
		  size_t nerased = conns.erase( name );
		  if( nerased != 1 )
		    {
		      fprintf(stderr, "REV: wtf erased not one?!?!\n");
		      exit(1);
		    }
		  else
		    {
		      fprintf(stdout, "REMOVED pupilinvis [%s] (Time since last detection [%lf] sec, > timeout [%lf])\n", name.c_str(), elapsed, MDNS_DISCONNECT_TIMEOUT_SEC);
		    }
		}
	      else
		{
		  fprintf(stdout, "Detected missing pupilinvis [%s] (Time since last detection [%lf] sec, timeout [%lf])\n", name.c_str(), elapsed, MDNS_DISCONNECT_TIMEOUT_SEC);
		}
	    }
	}
	
	//Short...but could predicate it on my localloop/doloop
	std::this_thread::sleep_for(std::chrono::milliseconds(MDNS_QUERY_TIMEOUT_MSEC));
      } //end while loop (while true forever)
    
    std::fprintf(stdout, "END: zeroconf service discovery loop (Tag=[%s])\n", tag.c_str());
  }


  //  std::map< std::string, std::shared_ptr<pupilinvis_interface> > get_conns()
  std::map< std::string, zeroconf_service_reply_struct> get_conns()
  {
    const std::lock_guard<std::mutex> lock(mu);
    auto ret = conns;
    return ret;
  }
  
}; //end struct avail_pupilinvis









struct avail_tobii_glasses2
  : public looper
{
public:
  std::map<std::string,tobii_glasses2_info> conns;
  std::string listenip6;//="::";
  int listenport; //=13006;
  socket_udp6 sock; //I won't use a different sock to...? Can I get a ref to it or some shit? Ptr ugh.
  int torecv;
  std::mutex mu;
  std::thread mythread;
  
  size_t get_n_avail()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return conns.size();
  }
  
  avail_tobii_glasses2( const std::string& ip6="::", const int port=13006, const int maxtorecv=1024 )
    : looper(),
      listenip6(ip6), listenport(port), torecv(maxtorecv)
  {  }

  ~avail_tobii_glasses2()
  {
    stop();
  }

  std::map<std::string,tobii_glasses2_info> get_conns()
  {
    const std::lock_guard<std::mutex> lock(mu);
    auto ret = conns;
    return ret;
  }
  
  //Hopefully it does not change between when user requests it...oh well.
  tobii_glasses2_info getconn( const std::string& ip6 )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( conns.contains(ip6) )
      {
	return conns[ip6];
      }
    else
      {
	fprintf(stderr, "Conns does not contain [%s]\n", ip6.c_str() );
	exit(1);
      }
  }
    
  std::map<std::string,std::string> getbest()
  {
    std::map<std::string,std::string> bestones;
    const std::lock_guard<std::mutex> lock(mu);
    for( auto& x : conns )
      {
	std::string thisip = x.first;
	std::string thisid = x.second.id;
	
	if( !bestones.contains(thisid) )
	  {
	    bestones[thisid] = thisip;
	  }
	else 
	  {
	    //REV faster for wired rather than wifi...
	    if( strcontains( x.second.interface, "eth" ) )
	      {
		bestones[thisid] = thisip;
	      }
	  }
      }
    return bestones;
  }

  void stop()
  {
    std::fprintf(stdout, "IN STOP: UDP available tobii glasses2 discovery thread (Tag=[%s])\n", tag.c_str());
    localloop.stop();
    
    if( mythread.joinable() )
      {
	mythread.join();
      }
    std::fprintf(stdout, "OUT STOP: UDP available tobii glasses2 discovery thread (Tag=[%s])\n", tag.c_str());
  }
  
  void start( loopcond& loop )
  {
    mythread = std::thread( &avail_tobii_glasses2::doloop, this, std::ref(loop) );
    //mythread.detach(); //No need to join. Resources released when execution stops.
  }

  void doloop( loopcond& loop )
  {
    std::vector<std::byte> buf;
    std::string bufstr;
    sock.bindi6( listenip6, listenport );
    
    while(localloop() && loop())
      {
	//sock.set_send_timeout( 2.0 );
	sock.set_recv_timeout( 0.3 ); //should probe if min is available! If not, exit, continue, etc.
	
	sock.listen_and_recvupto_fromany_into( torecv, buf ); // REV: should let it timeout... if loop is false? :(
	//Need to make this poll nicely.
	if( sock.has_other_address() && !buf.empty() )
	  {
	    bufstr = bytevec_to_str( buf );
	    
	    const std::lock_guard<std::mutex> lock(mu);
	    std::string otheraddrstr=sock.get_other_addr_str();
	    std::fprintf(stdout, "Adding/Setting tobii2 glasses available! [%s] [%s]\n", otheraddrstr.c_str(), bufstr.c_str() );
	    conns[otheraddrstr] = tobii_glasses2_info( sock, bufstr );
	  }
	//Short...but could predicate it on my localloop/doloop
	std::this_thread::sleep_for(std::chrono::milliseconds(MDNS_QUERY_TIMEOUT_MSEC));
      }
    
    std::fprintf(stdout, "END: UDP available tobii glasses2 discovery loop (Tag=[%s])\n", tag.c_str());
  }
};



#ifdef WITH_ITD

//////  REV -- FUCK! ITD thing is clogging up the USB when it tries to open? :(
//#define ALWAYSON_ITD

struct avail_itd
  : public looper
{
private:
  std::shared_ptr<ITD_wrapper> itd;
  std::thread mythread;
  bool isopen=false;
  
  
public:

  avail_itd()
    : looper()
  {
    itd = std::make_shared<ITD_wrapper>();
  }

  ~avail_itd()
  {
    stop();
  }

  void stop()
  {
    localloop.stop();
    JOIN( mythread );
  }
  
  std::optional<std::shared_ptr<ITD_wrapper>> get_itd()
  {
    //std::lock_guard<std::mutex> lk(mu);
    if( !itd )
      {
	fprintf(stderr, "Someone deallocated my itd ptr?!\n");
	localloop.stop();
	return std::nullopt;
      }

#ifdef ALWAYSON_ITD
    return itd;
#else
    if( isopen )
      {
	return itd;
      }
    else
      {
	return std::nullopt;
      }
#endif
  }

  void start( loopcond& loop )
  {
#ifndef ALWAYSON_ITD
    mythread = std::thread( &avail_itd::doloop, this, std::ref(loop) );
#endif
  }
  
  void doloop( loopcond& loop )
  {
    while(localloop() && loop())
      {
	if( itd )
	  {
	    if( itd->is_open() )
	      {
		isopen = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		fprintf(stdout, "Detected ITD was open!\n");
		//bool openagain = itd->open( false );
		//fprintf(stdout, "But opening again was [%s]\n", openagain?"true":"false");
	      }
	    else
	      {
		fprintf(stdout, "Detected ITD was NOT open!\n");
		fprintf(stdout, "Attempting to open ITD!\n");
		bool opened = itd->open();
	      }
	  }
	else
	  {
	    fprintf(stderr, "Someone made my itd ptr null...wtf?\n");
	    localloop.stop();
	  }
	
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      }
  }
};

#endif // REV: WITH_ITD





#ifdef WITH_REALSENSE
//REV: if I have another ctx with realsense will it fuck shit up?
//REV: regularly queries avail realsense USB-attached.
struct avail_realsense_devs
  : public looper
{
  rs2::context ctx;
  std::queue<rs2::event_information> events;
  std::mutex mu;
  std::map<std::string,std::string> lastdevs;
  std::thread mythread;

  avail_realsense_devs()
  {
    //REV: this is necessary or it will hang when new one plugged in?!
    ctx.set_devices_changed_callback(
				     [&](rs2::event_information& _info)
				     {
				       
				       fprintf(stdout, "DETECTED CHANGED RS DEVICE (connected/disconnected!\n");
				       // removed, added
				       //std::lock_guard<std::mutex> lock(mu);
				       //events.emplace( rs2::device_list{}, ctx.query_devices(RS2_PRODUCT_LINE_ANY) );
				     });
  }

  ~avail_realsense_devs()
  {
    stop();
  }

  void start( loopcond& loop )
  {
    mythread = std::thread( [&]() {
      while( loop() && localloop() )
	  {
	    update_current_devices();
	    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	  }
    } );
  }

  void stop()
  {
    localloop.stop();
    JOIN( mythread );
  }
  
  
  std::map<std::string,std::string> get_avail_devices()
  {
    std::lock_guard<std::mutex> lock(mu);
    auto result = lastdevs;
    return result;
  }

  
  
  
  void update_current_devices()
  {
    //std::queue<rs2::event_information>().swap(events); //ghetto clear -- swap with empty container.
    //auto current_devices = ctx.query_devices(RS2_PRODUCT_LINE_ANY);
    std::vector<rs2_stream> reqs;
    auto result = get_devices_with_functionality( reqs );
    std::lock_guard<std::mutex> lock(mu);
    lastdevs = result;
    return;
  }
  
  
  
  
  
  std::map<std::string,std::string> get_devices_with_functionality( std::vector<rs2_stream>& stream_requests )
  {
    //update_current_devices();
            
    std::map<std::string,std::string> result;
    std::vector<rs2_stream> unavailable_streams = stream_requests; //Unavail from any device on system
    auto devs = ctx.query_devices(); //current_devices;
    //std::cout << typeid(devs).name() << std::endl;
    
    try
      {
	std::string sn;
	for( auto&& dev : devs )
	  {
	    std::string name = get_device_name( dev, sn );
	    if( !sn.empty() )
	      {
		result[sn] = name;
		if( !stream_requests.empty() )
		  {
		    std::map<rs2_stream, bool> found_streams;
		    for (auto& type : stream_requests)
		      {
			found_streams[type] = false;
			auto sensors = dev.query_sensors();
			for (auto& sensor : sensors )
			  {
			    auto profiles = sensor.get_stream_profiles();
			    for (auto& profile : profiles )
			      {
				auto mytype = profile.stream_type();
				if (mytype == type)
				  {
				    unavailable_streams.erase(std::remove(unavailable_streams.begin(),
									  unavailable_streams.end(), type),
							      unavailable_streams.end());
				  }
			      }
			  }
		      }
		    bool addit=true;
		    for( auto& stream  : found_streams )
		      {
			if( !stream.second )
			  {
			    addit=false;
			    break; //break just the local for loop...
			  }
		      }
		    if( addit)
		      {
			result[sn] = name;
		      }
		  } //end if requests = empty
		else
		  {
		    result[sn] = name;
		  }
	      } //for dev in devs
	    else
	      {
		fprintf(stderr, "SN is empty for RS device?! [%s]\n", name.c_str() );
	      }
	  }
      }
    catch( const std::exception& e )
      {
	std::cerr << "RS avail, caught " << e.what() << std::endl;
      }
    catch( ... )
      {
	fprintf(stderr, "Caught SOMETHING (RS avail)\n");
      }
      
    stream_requests = unavailable_streams; //Replace it with any unavailable (empty will indicateit worked properly...something
    // should have been returned as result then).
    
    return result;
  }

  //REV: can't pass const b/c it will copy it? :( So, started thread won't join?
  static std::string get_device_name( rs2::device& dev, std::string& sn)
  {
    // Each device provides some information on itself, such as name:
    std::string name = "Unknown Device";
   
	
    if (dev.supports(RS2_CAMERA_INFO_NAME))
      {
	name = dev.get_info(RS2_CAMERA_INFO_NAME);
      }
	
    // and the serial number of the device:
    //std::string sn = "########";
    sn=std::string();
    if (dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
      {
	//sn = std::string("#") + dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
	sn = std::string(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
      }
    return std::string(name + " #" + sn);
  } //end get_device_name
}; //end realsense avail

#endif //end if WITH_REALSENSE
