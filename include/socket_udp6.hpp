#pragma once

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h> //getifaddrs
#include <sys/ioctl.h> //IFF_UP etc.
#include <net/if.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <mutex>


//#define DEBUG_LEVEL 0

//This is a socket that recieves data (from anywhere etc.)
struct socket_udp6
{
  int sd; //file descriptor
  bool isopen; //sd got local file descriptor (made in constructor...so whatever)
  bool sendok;
  bool recvok;

  struct sockaddr_in6 other_addr;
  struct sockaddr_in6 listen_addr;
  socklen_t other_len;
  socklen_t listen_len;
  
  std::string other_addr_str;
  std::string listen_addr_str;

  int other_port;
  int listen_port;

  std::mutex mu;
  
  socket_udp6( )
  {
    sd = socket(AF_INET6, SOCK_DGRAM, 0);
    
    if (sd < 0)
      {
	std::fprintf(stderr, "Create socket6 UDP failed!\n");
	close(sd);
	exit(1);
      }
        
    int yes = 1;
    int r2 = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&yes, sizeof(yes));
    if( r2 != 0 )
      {
	std::fprintf(stderr, "Setting socket IP6 only Failed!\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
      }
    
    isopen = true; //when socket FD/SD exists (i.e. socket() called getting resouce file descriptor from OS)
    
    sendok = false; //ok to send (set other)
    recvok = false; //ok to recv (set listen)
    
    memset(&listen_addr, 0, sizeof(listen_addr));
    memset(&other_addr, 0, sizeof(other_addr));

    listen_len = sizeof(listen_addr);
    other_len = sizeof(other_addr);
    
    return;
  }
  
  bool set_send_timeout( const double timeout_sec )
  {
    //if 0, wait indefinitely...
    //Note no need for SEND timeout for UDP since it won't wait anyways? (or will it...of interface is fucked?)
    
    std::uint64_t allusec = (std::uint64_t)(timeout_sec * 1e6);
    std::uint64_t sec = allusec / (std::uint64_t)1e6;
    std::uint64_t usec = allusec % (std::uint64_t)1e6;
    
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    const std::lock_guard<std::mutex> lock(mu);
    int r = setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    if( r != 0 )
      {
	std::fprintf(stderr, "Setting socket SND TIMEO Failed!\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
	return false;
      }
    return true;
    
  }
  bool set_recv_timeout( const double timeout_sec )
  {
    //if 0, wait indefinitely...
    //Note no need for SEND timeout for UDP since it won't wait anyways? (or will it...of interface is fucked?)
    
    std::uint64_t allusec = (std::uint64_t)(timeout_sec * 1e6);
    std::uint64_t sec = allusec / (std::uint64_t)1e6;
    std::uint64_t usec = allusec % (std::uint64_t)1e6;
    
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    const std::lock_guard<std::mutex> lock(mu);
    int r = setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    if( r != 0 )
      {
	std::fprintf(stderr, "Setting socket RCV TIMEO Failed!\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
	return false;
      }
    return true;
    
  }

  
  ~socket_udp6()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( isopen )
      {
	close(sd);
	isopen = false;
      }
    return;
  }

  bool has_other_address()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return( !(other_addr_str.empty()) );
  }
  
  
  
  int get_socket_info( sockaddr_in6& addr, socklen_t& len ) const
  {
    //REV: reads from local socket descriptor!
    int r = getsockname( sd, (struct sockaddr*)&addr, &len );
    if( -1 == r )
      {
	std::fprintf(stderr, "Can't get SOCKNAME (for sent port of sendto)!\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
      }
    return r;
  }
  
  static std::string ip6_str_from_struct( const sockaddr_in6& addr, const socklen_t len )
  {
    std::string result;
    char ip6buffer[INET6_ADDRSTRLEN];
    int err = getnameinfo((struct sockaddr*)&addr, len, ip6buffer, sizeof(ip6buffer),
			  0, 0, NI_NUMERICHOST);
    if (err!=0)
      {
	std::fprintf(stderr, "Failed to convert address to string (code=%d)\n", err);
      }
    else
      {
	result = std::string(ip6buffer);
      }
    return result;
  }

  

  //REV: can't lock here...
  ssize_t trysend( const std::vector<std::byte>& msg, const sockaddr_in6& addr, const socklen_t len )
  {
    int flags=0;
    ssize_t result = sendto(sd, msg.data(), msg.size(), flags, 
			    (const struct sockaddr*)&addr, len );
    
    if( result < 0 )
      {
	std::fprintf(stderr, "Send failed?!\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
      }
    return result;
  }

  void sendto6( const std::string& msg, const struct sockaddr_in6& other, const int destport )
  {
    auto bytevec = str_to_bytevec( msg );
    sendto6( bytevec, other, destport );
  }
  
  //REV: this will set connectedother
  //REV: this will overwrite other_ things
  void sendto6( const std::vector<std::byte>& msg, const struct sockaddr_in6& other, const int destport )
  {
    //std::fprintf(stdout, "SENDTO6\n");
    //printsocket();
    
    //Set DESTINATION IP/PORT values
    sockaddr_in6 tmpaddr = other;
    socklen_t tmplen = sizeof(tmpaddr);
    int tmpport = destport;
    tmpaddr.sin6_port = htons(tmpport);
    tmpaddr.sin6_family = AF_INET6;
    
    std::string tmpaddrstr = ip6_str_from_struct( tmpaddr, tmplen );
    
    if( tmpaddrstr.empty() )
      {
	return;
      }
    
    ssize_t result = trysend( msg, tmpaddr, tmplen );

    
    //Set SENDING IP/PORT values (i.e. local listen values...)
    sockaddr_in6 tmplistenaddr;
    socklen_t tmplistenlen = sizeof(tmplistenaddr);
    int r = get_socket_info( tmplistenaddr, tmplistenlen );
    std::string tmplistenaddrstr = ip6_str_from_struct( tmplistenaddr, tmplistenlen );
    int tmplistenport = ntohs(tmplistenaddr.sin6_port);


    
    //LOCK
    {
      const std::lock_guard<std::mutex> lock(mu);

      //tmpaddrstr must not be empty (since we would have returned)
      other_len= tmplen;
      other_addr = tmpaddr;
      other_addr_str = tmpaddrstr;
      other_port = tmpport;

      sendok = true;
      
      if( 0==r && !tmplistenaddrstr.empty() )
	{
	  listen_len = tmplistenlen;
	  listen_addr = tmplistenaddr;
	  listen_addr_str = tmplistenaddrstr;
	  listen_port = tmplistenport;
	
	  recvok = true; //because it is bound!
	}
    }
    //UNLOCK

#if DEBUG_LEVEL>10
    fprintf(stdout, "SENDTO6   Sent [%d] bytes to (%s : %d)!\n", result, tmpaddrstr.c_str(), destport );
    printsocket();
#endif
    
    
    return;
  }

  void printsocket()
  {
    const std::lock_guard<std::mutex> lock(mu);

    sockaddr_in6 tmplistenaddr;
    socklen_t tmplistenlen = sizeof(tmplistenaddr);
    int r = get_socket_info( tmplistenaddr, tmplistenlen );
    std::string tmplistenaddrstr = ip6_str_from_struct( tmplistenaddr, tmplistenlen );
    int tmplistenport = ntohs(tmplistenaddr.sin6_port);
    
    std::fprintf(stdout, "(PRINTSOCKET) -- listen address from thisstruct is: [%s]:[%d]\n", tmplistenaddrstr.c_str(), tmplistenport);
    if( sendok )
      {
	std::fprintf(stdout, "Socket SEND (OTHER): [%s]:[%d]\n", other_addr_str.c_str(), other_port );
      }
    
    if( recvok )
      {
	std::fprintf(stdout, "Socket LISTEN (THIS): [%s]:[%d]\n", listen_addr_str.c_str(), listen_port );
      }
  }

  
  void send6( const std::string& msg )
  {
    auto bytevec = str_to_bytevec( msg );    
    send6(bytevec);
  }
  
  void send6( const std::vector<std::byte>& msg )
  {
    //std::fprintf(stdout, "SEND6\n");
    //printsocket();
    
    sockaddr_in6 tmpaddr;
    socklen_t tmplen;
    int tmpport;
    
    {
      const std::lock_guard<std::mutex> lock(mu);

      if( !sendok || !isopen )
      {
	std::fprintf(stderr, "ERROR: cannot SEND6 without having connection (need other_addr etc. set. Try sendto6?).\n");
	return;
      }
      
      //Set DESTINATION IP/PORT values
      tmpaddr = other_addr;
      tmplen = other_len;
      tmpport = other_port;
      tmpaddr.sin6_port = htons(tmpport);
      tmpaddr.sin6_family = AF_INET6;
    }
      
    std::string tmpaddrstr = ip6_str_from_struct( tmpaddr, tmplen );
    
    if( tmpaddrstr.empty() )
      {
	return;
      }
    
    ssize_t result = trysend( msg, tmpaddr, tmplen );
    
    //Set SENDING IP/PORT values (i.e. local listen values...)
    sockaddr_in6 tmplistenaddr;
    socklen_t tmplistenlen = sizeof(tmplistenaddr);
    int r = get_socket_info( tmplistenaddr, tmplistenlen );
    std::string tmplistenaddrstr = ip6_str_from_struct( tmplistenaddr, tmplistenlen );
    int tmplistenport = ntohs(tmplistenaddr.sin6_port);
    
    //LOCK
    {
      const std::lock_guard<std::mutex> lock(mu);
      
      if( 0==r && !tmplistenaddrstr.empty() )
	{
	  listen_len = tmplistenlen;
	  listen_addr = tmplistenaddr;
	  listen_addr_str = tmplistenaddrstr;
	  listen_port = tmplistenport;
	  
	  recvok = true; //because it is bound!
	}
    }
    //UNLOCK


#if DEBUG_LEVEL>10
    fprintf(stdout, "SEND6   Sent [%d] bytes to (%s : %d)!\n", result, tmpaddrstr.c_str(), tmpport );
    printsocket();
#endif

    return;
  }
  
  //REV: this needs to process ip6 into appropriate %iface.
  void bindi6( const std::string& ip6, const int port )
  {
    const std::lock_guard<std::mutex> lock(mu);
    
    listen_len = sizeof(listen_addr);
    memset(&listen_addr, 0, listen_len);
    
    listen_addr_str = ip6;
    listen_port = port;

    listen_addr.sin6_family = AF_INET6;
    listen_addr.sin6_port = htons(listen_port); //REV: shit, this is what I *listen* on?!
    
    if( ip6.empty() )
      {
	listen_addr.sin6_addr = in6addr_any; //REV: same as "::"?
      }
    else
      {
	int r = inet_pton(AF_INET6, ip6.c_str(), &(listen_addr.sin6_addr));
	if( 1 != r )
	  {
	    fprintf(stderr, "Failed to convert address [%s] to ip6 (maybe ip4 by accident?)\n", ip6.c_str() );
	    return;
	  }
      }
    
    if( -1 == bind(sd, (const struct sockaddr*)&listen_addr,
		   listen_len ) )
      {
	std::fprintf(stderr, "bindi6: bind socket failed\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
	recvok = false;
      }
    else
      {
	recvok = true;
      }
    
    return;
  } //end void bind of ip6 udp conn

  
  //REV: will this work without bind? It won't know port to listen on? :(
  //REV: unbound will cause an error in recv.
  std::vector<std::byte> listen_and_recvupto_fromany( const int size )
  {
    //std::fprintf(stdout, "LISTEN_RECVUPTO_FROMANY\n");
    //printsocket();
    
    std::vector<std::byte> buf;
    listen_and_recvupto_fromany_into( size, buf );
    return buf;
  }
  
  int tryrecvfrom( const int size, std::vector<std::byte>& buf, sockaddr_in6& tmpaddr, socklen_t& tmplen )
  {
    //REV: calling socket interface -- expects unsigned char*?

#if DEBUG_LEVEL > 100
    sockaddr_in6 tmplistenaddr;
    socklen_t tmplistenlen = sizeof(tmplistenaddr);
    int r = get_socket_info( tmplistenaddr, tmplistenlen );
    if( -1 == r )
      {
	fprintf(stderr, "Get sock info error\n");
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
	exit(1);
      }
    std::string tmplistenaddrstr = ip6_str_from_struct( tmplistenaddr, tmplistenlen );
    int tmplistenport = ntohs(tmplistenaddr.sin6_port);
    fprintf(stdout, "TRYRECVFROM: Receive into LISTEN: [%s]:[%d]\n", tmplistenaddrstr.c_str(), tmplistenport );
#endif
    
    //REV: MSG_WAITALL has no effect for datagram, so just use noflags (0)
    int num_recvd = recvfrom(sd, const_cast<std::byte*>(buf.data()), buf.size(), 0, (struct sockaddr*)&tmpaddr, &tmplen);
    //fprintf(stdout, "DONE TRYRECVFROM\n");
    if (num_recvd < 0 )
      {
#if DEBUG_LEVEL > 100
	std::fprintf(stderr, "recvfrom failed (for a reason), error %d\n", num_recvd); //timeout etc? Not really a problem. Handle nice.
	std::fprintf(stderr, "[%s]\n", strerror(errno) );
#endif
	buf.clear();
	return num_recvd;
      }
    else if( 0 == num_recvd )
      {
	std::fprintf(stderr, "recvfrom got no data, orderly peer shutdown %d\n", num_recvd);
	buf.clear();
	return num_recvd;
      }
    else
      {
	buf.resize( num_recvd );
	return num_recvd;
      }
  }
  
  void listen_and_recvupto_fromany_into( const int size, std::vector<std::byte>& buf )
  {
    //std::fprintf(stdout, "LISTEN_RECVUPTO_FROMANY_INTO\n");
    //printsocket();
    
    buf.resize(size);
        
    //LOCK
    {
      const std::lock_guard<std::mutex> lock(mu);
      
      if( !recvok || !isopen )
	{
	  std::fprintf(stderr, "LISTEN&RECV FROMANY INTO: Socket does not exist (no fd/sd) OR you have not connected (bound) it using bind() yet\n");
	  buf.clear();
	  return;// buf;
	}
      if( sendok )
	{
#if DEBUG_LEVEL>1
	  std::fprintf(stderr, "WARNING: you are calling listen_and_recvupto_fromany, but you already are connected to a specific endpoint...this will overwrite your other_addr!\n");
#endif
	}
    }
    //UNLOCK
    
    sockaddr_in6 tmpaddr;
    socklen_t tmplen;
        
    tmplen = sizeof(tmpaddr);
    memset(&tmpaddr, 0, tmplen);
    tmpaddr.sin6_family = AF_INET6;
    
    int num_recvd = tryrecvfrom( size, buf, tmpaddr, tmplen);

    int tmpport = ntohs( tmpaddr.sin6_port);
    std::string tmpaddrstr = ip6_str_from_struct( tmpaddr, tmplen );
    

    if( !tmpaddrstr.empty() && num_recvd>0 )
      {
#if (DEBUG_LEVEL>0)
	if( num_recvd > 0 )
	  {
	    std::fprintf(stdout, "(LISTEN&RECV FROMANY) Received %d bytes from remote address: %s (port: %d)\n", num_recvd, tmpaddrstr.c_str(), tmpport );
	    std::fprintf(stdout, "Msg: [%s]\n", std::string(reinterpret_cast<char*>(buf.data())).c_str() );
	  }
#endif	
	
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);

	  //If everything worked, I should be able to now set SENDOK and set other_ to proper
	  other_len= tmplen;
	  other_addr = tmpaddr;
	  other_addr_str = tmpaddrstr;
	  other_port = tmpport;
	  
	  sendok = true;
	}
	//UNLOCK
	
      }
    else
      {
	//std::fprintf(stderr, "Wtf error receiving?\n");
      }
    
    return;
  }

  //REV: will this work without bind? It won't know port to listen on? :(
  //REV: unbound will cause an error in recv.
  std::vector<std::byte> recvupto_from( const int size, const struct sockaddr_in6& other, const int otherport )
  {
    std::vector<std::byte> buf(size);
    recvupto_from_into( size, other, otherport, buf );
    return buf;
  }

  
  
  //REV: do I wnat to set sendOK true even if receive fails?
  //void recvupto_from_into( const int size, const struct sockaddr_in6& other, const int otherport, std::vector<std::byte>& buf )
  void recvupto_from_into( const int size, const struct sockaddr_in6& other, const int otherport, std::vector<std::byte>& buf )
  {
    //std::fprintf(stdout, "RECVUPTO_FROM_INTO\n");
    //printsocket();
    
    buf.resize(size);    

    //LOCK
    {
      const std::lock_guard<std::mutex> lock(mu);
      
      if( !recvok || !isopen )
	{
	  std::fprintf(stderr, "LISTEN&RECV INTO Socket does not exist (no fd/sd) OR you have not connected (bound) it using bind() yet\n");
	  buf.clear();
	  return;// buf;
	}
      if( sendok )
	{
#if DEBUG_LEVEL>1
	  std::fprintf(stderr, "WARNING: you are calling recvupto_from, but you already are connected to a specific endpoint...this will overwrite your other_addr!\n");
#endif
	}
    }
    //UNLOCK

    //REV: just make empty addr?

    //REV: It's impossible for DGRAM to set which target I will receive from -- oh well.
    //I could "peek" and check if it is from the right other guy? Oh well.
    //Check source IP etc. and error if it is not correct? Assume his IP does not change...?
        
    //tmpaddr.sin6_addr = other.sin6_addr;
    
    //REV: can I actually filter source IP of incoming msgs?
    //NO, I can't. So, just fucking ignore it.
    
    sockaddr_in6 tmpaddr = other;
    socklen_t tmplen = sizeof(tmpaddr);
    int tmpport = otherport;
    tmpaddr.sin6_family = AF_INET6;
    if( tmpport >= 0 )
      {
	tmpaddr.sin6_port = htons(tmpport); //what will -1 port do? lol
      }
    //sockaddr_in6 tmpaddr;
    //socklen_t tmplen = sizeof(tmpaddr);
    //memset( &tmpaddr, 0, tmplen );
    //int tmpport = -1;
    
    std::string tmpaddrstr = ip6_str_from_struct( tmpaddr, tmplen );

#if (DEBUG_LEVEL>0)
    std::fprintf(stdout, "(RECVFROM INTO) Will attempt to receive %d bytes from [%s]:%d\n", size, tmpaddrstr.c_str(), tmpport );
    printsocket();
#endif

    if( tmpaddrstr.empty() )
      {
	return;
      }

    int num_recvd = tryrecvfrom( size, buf, tmpaddr, tmplen);
    
    if( num_recvd > 0 )
      {
	tmpport = ntohs(tmpaddr.sin6_port);
	
#if (DEBUG_LEVEL>0)
	if( num_recvd > 0 )
	  {
	    
	    std::fprintf(stdout, "(RECVFROM INTO) Received %d bytes from remote address: %s (port: %d)\n", num_recvd, tmpaddrstr.c_str(), tmpport );
	    std::fprintf(stdout, "Msg: [%s]\n", std::string(reinterpret_cast<char*>(buf.data())).c_str() );
	  }
#endif	
	
	//LOCK
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  
	  //If everything worked, I should be able to now set SENDOK and set other_ to proper
	  //REV: I should check if the sending IP is the same? But in DGRAM, they can put anything in the source IP thing.
	  other_len= tmplen;
	  other_addr = tmpaddr;
	  other_addr_str = tmpaddrstr;
	  other_port = tmpport;
	  
	  sendok = true;
	}
	//UNLOCK
	
      }
    else
      {
	std::fprintf(stderr, "Wtf error receiving?\n");
      }
    
    return;
  }
  
  
  std::vector<std::byte> recvupto( const int size )
  {
    //std::fprintf(stdout, "RECVUPTO\n");
    //printsocket();
    
    std::vector<std::byte> buf( size );
    recvupto_into( size, buf );
    return buf;
  }
  
  void recvupto_into( const int size, std::vector<std::byte>& buf )
  {
    //std::fprintf(stdout, "RECVUPTO_INTO\n");
    //printsocket();

    buf.resize(size);

    sockaddr_in6 tmpaddr;
    socklen_t tmplen;
    int tmpport;
    
    
    //LOCK
    {
      const std::lock_guard<std::mutex> lock(mu);
      
      if( !recvok || !isopen )
	{
	  std::fprintf(stderr, "RECVINTO Socket does not exist (no fd/sd) OR you have not connected (bound) it using bind() yet\n");
	  buf.clear();
	  return;
	}
      if( !sendok )
	{
	  std::fprintf(stderr, "ERROR: you must have sendok to use recvupto_into\n");
	  buf.clear();
	  return;
	}

      //REV: no need to set... will be overwrriten anyways?
      tmpaddr = other_addr;
      tmplen = other_len;
      tmpport = other_port;
    
    }
    //UNLOCK
    
    std::string tmpaddrstr = ip6_str_from_struct( tmpaddr, tmplen );
    
    if( tmpaddrstr.empty() )
      {
	return;
      }

#if (DEBUG_LEVEL>0)
    int realport = ntohs(other_addr.sin6_port);
    std::fprintf(stdout, "(RECV INTO) Will attempt to receive %d bytes from [%s]:%d (actual port from struct: %d)\n", size, tmpaddrstr.c_str(), tmpport, realport );
    printsocket();
#endif
    
    int num_recvd = tryrecvfrom( size, buf, tmpaddr, tmplen );
    
    if( num_recvd > 0 )
      {
#if (DEBUG_LEVEL>0)
	if( num_recvd > 0 )
	  {
	    std::fprintf(stdout, "(LISTEN&RECV FROMANY) Received %d bytes from remote address: %s (port: %d)\n", num_recvd, tmpaddrstr.c_str(), tmpport );
	    std::fprintf(stdout, "Msg: [%s]\n", std::string(reinterpret_cast<char*>(buf.data())).c_str() );
	  }
#endif	
      }
    else
      {
	std::fprintf(stderr, "Wtf error receiving?\n");
      }
    
    return;
  }
  
  const struct sockaddr_in6 get_other_addr( socklen_t& itslen )
  {
    const std::lock_guard<std::mutex> lock(mu);
    //REV: make a copy of it? It has some pointers right? :(
    itslen = other_len;
    return other_addr;
  }

  const struct sockaddr_in6 get_listen_addr( socklen_t& itslen )
  {
    const std::lock_guard<std::mutex> lock(mu);
    //REV: make a copy of it? It has some pointers right? :(
    itslen = listen_len;
    return listen_addr;
  }

  int get_other_port()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return other_port;
  }

  int get_listen_port()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return listen_port;
  }
  
  std::string get_other_addr_str()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return other_addr_str;
  }
  
  std::string get_listen_addr_str()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return listen_addr_str;
  }
  
  
  
};
