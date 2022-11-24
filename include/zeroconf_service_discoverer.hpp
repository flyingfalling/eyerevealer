
#pragma once

#include <mdns.h>
#include <mdns_cpp/mdns.hpp>
#include <mdns_cpp/utils.hpp>

using namespace mdns_cpp;

struct zeroconf_service_reply_struct
{
  //REV: STRING_FORMAT   -- this replaces the argument with with s.length, s.str (just a template, replacement thing).
  //"%s : %s %.*s SRV %.*s priority %d weight %d port %d\n", fromaddrstr.data(), entrytype,
  //	       MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(srv.name), srv.priority, srv.weight, srv.port
  //10.42.0.157:5353 : answer PI monitor:OnePlus 8:b66de52865b67021._http._tcp.local. SRV pi.local. priority 0 weight 0 port 8080
  //std::string thestr = MDNS_STRING_FORMAT(entrystr)
  std::string srcaddrport;
  std::string srcaddr;
  std::string name;
  std::string svcname;
  int dstport;
  bool ip6=false;
  Timer t;

  double get_elapsed_seconds()
  {
    return t.elapsed();
  }

  void reset_timeout()
  {
    t.reset();
  }
  
  std::string tostr()
  {
    std::string result = "SRC ADDR:PORT=" + srcaddrport + "   SRC ADDR=" + srcaddr + "    NAME=" + name + "    svcname=" + svcname + "    DST PORT=" + std::to_string(dstport);
    return result;
  }
  
  zeroconf_service_reply_struct()
  {
  }

  /*
  zeroconf_service_reply_struct( const zeroconf_service_reply_struct& rhs )
    : srcaddrport(rhs.srcaddrport), srcaddr(rhs.srcaddr), name(rhs.name), svcname(rhs.svcname), dstport(rhs.dstport), ip6(rhs.ip6)
  {
    //wtf copy constructor?
    }*/
  
  zeroconf_service_reply_struct( const std::string& _srcaddrport, const std::string& _name, const std::string& _svcname, const int _dstport )
    : srcaddrport(_srcaddrport), name(_name), svcname(_svcname), dstport(_dstport)
  {
    auto ipaddrvec = split_string(srcaddrport, ':');
    //if( ipaddrvec.size() != 2 )
    //  {
    //	fprintf(stderr, "REV WARNING: HMM source address:port was not correct format? Ewwww is it IP6 or some shit? (may be [::]:port etc.)\n");
    //}
    //drop the last one, assuming it is the port
    ipaddrvec.pop_back();

    if( ipaddrvec.size() > 2 )
      {
	ip6 = true;
      }
    
    //The rest must be the ip addr.
    //Wait, join with :? OK...
    srcaddr = join_strings_vec( ipaddrvec, ":" );
    return;
  }
};


//REV: wtf...I don't want this static shit here lol
//static mdns_record_txt_t txtbuffer[128];

static int query_callback(int sock, const struct sockaddr *from, size_t addrlen, mdns_entry_type_t entry,
                          uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void *data,
                          size_t size, size_t name_offset, size_t name_length, size_t record_offset,
                          size_t record_length, void *user_data)
{
  (void)sizeof(sock);
  (void)sizeof(query_id);
  (void)sizeof(name_length);
  (void)sizeof(user_data);

  //REV: are these buffers good enough sizes?
  char addrbuffer[128]{};
  char namebuffer[512]{};
  char entrybuffer[512]{};
  
  //REV: cast it...
  //REV: don't fucking free this shit!? I hope it does not
  std::vector<zeroconf_service_reply_struct>* user_vec_ptr = (std::vector<zeroconf_service_reply_struct>*)(user_data);

  //REV this std::string was pointing to constant data?
  const auto fromaddrstr = ipAddressToString(addrbuffer, sizeof(addrbuffer), from, addrlen);
  
  const char *entrytype =
      (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer" : ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
  
  mdns_string_t entrystr = mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));
  
  const int str_capacity = 1000;
  char str_buffer[str_capacity]={};
  
  if (rtype == MDNS_RECORDTYPE_PTR)
    {
      mdns_string_t namestr =
        mdns_record_parse_ptr(data, size, record_offset, record_length, namebuffer, sizeof(namebuffer));
      
      snprintf(str_buffer, str_capacity, "%s : %s %.*s PTR %.*s rclass 0x%x ttl %u length %d\n", fromaddrstr.data(),
	       entrytype, MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(namestr), rclass, ttl, (int)record_length);
    }
  else if (rtype == MDNS_RECORDTYPE_SRV)
    {
      mdns_record_srv_t srv =
        mdns_record_parse_srv(data, size, record_offset, record_length, namebuffer, sizeof(namebuffer));
      
      snprintf(str_buffer, str_capacity,"%s : %s %.*s SRV %.*s priority %d weight %d port %d\n", fromaddrstr.data(), entrytype,
	       MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(srv.name), srv.priority, srv.weight, srv.port);
      //fprintf(stdout, "Attempting to create!!\n");
      zeroconf_service_reply_struct uds( std::string(fromaddrstr.data()), std::string(entrystr.str, entrystr.length), std::string(srv.name.str, srv.name.length), srv.port );
      //fprintf(stdout, "Created...pushingto back...\n");
      user_vec_ptr->push_back( uds );
      //fprintf(stdout, "Done...pushing\n");
    }
  else if (rtype == MDNS_RECORDTYPE_A)
    {
      struct sockaddr_in addr;
      mdns_record_parse_a(data, size, record_offset, record_length, &addr);
      const auto addrstr = ipv4AddressToString(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
      snprintf(str_buffer, str_capacity,"%s : %s %.*s A %s\n", fromaddrstr.data(),
	       entrytype, MDNS_STRING_FORMAT(entrystr), addrstr.data());
    }
  else if (rtype == MDNS_RECORDTYPE_AAAA)
    {
      struct sockaddr_in6 addr;
      mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
      const auto addrstr = ipv6AddressToString(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
      snprintf(str_buffer, str_capacity,"%s : %s %.*s AAAA %s\n", fromaddrstr.data(),
	       entrytype, MDNS_STRING_FORMAT(entrystr), addrstr.data());
    }
  else if (rtype == MDNS_RECORDTYPE_TXT)
    {
      const size_t TXTBUFSIZE=1024;
      mdns_record_txt_t txtbuffer[TXTBUFSIZE];
      size_t parsed = mdns_record_parse_txt(data, size, record_offset, record_length, txtbuffer,
					    sizeof(txtbuffer) / sizeof(mdns_record_txt_t));
      for (size_t itxt = 0; itxt < parsed; ++itxt)
      {
	if (txtbuffer[itxt].value.length)
	  {
	    snprintf(str_buffer, str_capacity,"%s : %s %.*s TXT %.*s = %.*s\n",
		     fromaddrstr.data(),
		     entrytype,
		     MDNS_STRING_FORMAT(entrystr),
		     MDNS_STRING_FORMAT(txtbuffer[itxt].key),
		     MDNS_STRING_FORMAT(txtbuffer[itxt].value));
	  }
	else
	  {
	    snprintf(str_buffer, str_capacity,"%s : %s %.*s TXT %.*s\n", fromaddrstr.data(), entrytype, MDNS_STRING_FORMAT(entrystr),
		     MDNS_STRING_FORMAT(txtbuffer[itxt].key));
	  }
      }
    }
  else
    {
      snprintf(str_buffer, str_capacity,"%s : %s %.*s type %u rclass 0x%x ttl %u length %d\n", fromaddrstr.data(), entrytype,
	       MDNS_STRING_FORMAT(entrystr), rtype, rclass, ttl, (int)record_length);
    }

  //REV: stringbuffer contains all the results. I could parse it afterwards...
  fprintf(stdout, "REV: printing results in here:\n");
  std::cout << std::string(str_buffer);
  
  return 0;
}


struct zeroconf_service_discoverer
{
  std::vector<zeroconf_service_reply_struct> query_service( const std::string& service )
  {
    std::vector<zeroconf_service_reply_struct> userdats;
    
    int sockets[32];
    int query_id[32];
    //REV: will he have namespace clash between mDNS and mdns_cpp?
    mDNS mdnsobj;
    int num_sockets = mdnsobj.openClientSockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);

#if DEBUG_LEVEL >= 100
    std::cout << "Opened " << num_sockets << " socket" << (num_sockets ? "s" : "") << " for mDNS query\n";
#endif
    
    if (num_sockets <= 0)
      {
	//const auto msg = "Failed to open any client sockets";
	//std::cerr << msg << std::endl;
	//throw std::runtime_error(msg);
	return userdats;
      }
    

    size_t capacity = 2048;
    void *buffer = malloc(capacity);
    //void *user_data = 0;
    
    void* user_data = (void*)(&userdats); //cast to void pointer...
    
    size_t records;

    for (int isock = 0; isock < num_sockets; ++isock)
      {
	query_id[isock] = mdns_query_send(sockets[isock], MDNS_RECORDTYPE_PTR, service.data(), strlen(service.data()),
					  buffer, capacity, 0);
	if (query_id[isock] < 0)
	  {
	    std::cerr << "Failed to send mDNS query: " << strerror(errno) << std::endl;
	  }
      }
    
    
    int res=0;
#if DEBUG_LEVEL >= 100
    std::cerr << "Reading mDNS query replies" << std::endl;
#endif
    do
      {
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	
	int nfds = 0;
	fd_set readfs;
	FD_ZERO(&readfs);
	for (int isock = 0; isock < num_sockets; ++isock)
	  {
	    if (sockets[isock] >= nfds)
	      { nfds = sockets[isock] + 1; }
	    FD_SET(sockets[isock], &readfs);
	  }
	
	records = 0;
	res = select(nfds, &readfs, 0, 0, &timeout);
	if(res > 0)
	  {
	    for (int isock = 0; isock < num_sockets; ++isock)
	      {
		if (FD_ISSET(sockets[isock], &readfs) )
		  {
		    //REV: push back data here
#if DEBUG_LEVEL>=100
		    fprintf(stdout, "CALLING MDNS QUERY RECV!\n");
#endif
		    records += mdns_query_recv(sockets[isock], buffer, capacity, query_callback, user_data, query_id[isock]);
		  }
		FD_SET(sockets[isock], &readfs);
	      }
	  }
      } while (res > 0);
    
    free(buffer);

    for (int isock = 0; isock < num_sockets; ++isock)
      {
	mdns_socket_close(sockets[isock]);
      }

#if DEBUG_LEVEL > 100
    std::cerr << "Closed socket" << (num_sockets ? "s" : "") << std::endl;
#endif

    return userdats;
    
  } //query_service
};
