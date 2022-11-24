#pragma once

//standard libs
#include <cstdio>
#include <cstdlib>
#include <iostream>

//Other libs
//JSON
#include <nlohmann/json.hpp>

//BOOST ASIO (asynch IO)
#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

//HTTP client. (BOOST BEAST)
//#include <boost/network/protocol/http/client.hpp>
//#include <boost/network/uri.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <boost/exception/diagnostic_information.hpp>

// my includes
#include <utilities.hpp>

using namespace nlohmann;


json delete_json( const std::string& baseurl, const std::string& api_action, const std::string& port="80")
{
  try
    {
      fprintf(stdout, "Will attempt DELETE json with [%s]\n", baseurl.c_str());
      boost::asio::ip::address addr = boost::asio::ip::address::from_string(baseurl);
#if HTTP_DEBUG_LEVEL > 10
      std::cout << "Valid IP address " << baseurl << std::endl;
#endif
      
      //Create TCP socket
      auto const host = baseurl;
      //auto const port = "80";
      auto const target = api_action;
  
      boost::asio::io_context ioc;
      boost::asio::ip::tcp::resolver resolver{ioc};
      boost::asio::ip::tcp::socket socket{ioc};
      auto const results = resolver.resolve(host, port); //this gives us the IP. Note, it will be an IP already lol.
      //std::cout << results << std::endl;
      std::cout << results.size() << std::endl;
    
      //Connect TCP socket
      boost::asio::connect(socket, results.begin(), results.end() );
  
      //construct a (POST) request string...
      const int version = 11;
      boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::delete_, target, version};
      req.set(boost::beast::http::field::host, host);
      req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  
      req.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
  
      std::cout << req << std::endl;
      req.prepare_payload();
  
      //Write to socket
      boost::beast::http::write( socket, req );
  
      //Wait and read response
      boost::beast::flat_buffer buffer;
      boost::beast::http::response<boost::beast::http::dynamic_body> res;
  
      boost::beast::http::read(socket, buffer, res);
  
      std::string mybody = boost::beast::buffers_to_string( res.body().data()  );

#ifdef HTTP_DEBUG_LEVEL
#if HTTP_DEBUG_LEVEL > 10
      std::cout << "Result (DELETE): " << res.result() << std::endl;
      std::fprintf(stdout, "BODY: [%s]\n", mybody.c_str());
#endif
#endif
    
      //https://www.boost.org/doc/libs/master/boost/beast/http/status.hpp e.g. 200, 201, 202
      if( /*res.result() == boost::beast::http::status::ok ||
	    res.result() == boost::beast::http::status::created ||
	    res.result() == boost::beast::http::status::accepted ||*/
	  res.result() == boost::beast::http::status::no_content
	  )
	{
	  auto response_json = json::parse( mybody );
	  return response_json;
	}
      else
	{
	  json j;
	  return j;
	}
    }
  catch( boost::exception& e )
    {
      fprintf(stderr, "REV: caught some error from boost, returning empty (failure...)\n");
      std::string info = boost::diagnostic_information(e);
      std::cerr << info << std::endl;
      json j;
      return j;
    }
  
} // end delete_json



json get_json( const std::string& baseurl, const std::string& api_action, const std::string& port="80" )
{
  try
    {
  boost::asio::ip::address addr = boost::asio::ip::address::from_string(baseurl);
  #if HTTP_DEBUG_LEVEL > 10
  std::cout << "Valid IP address " << baseurl << std::endl;
  #endif
  
  //Create TCP socket
  auto const host = baseurl;
  //auto const port = "80";
  auto const target = api_action;
  boost::asio::io_context ioc;

  boost::asio::ip::tcp::resolver resolver{ioc};
  boost::asio::ip::tcp::socket socket{ioc};
  auto const results = resolver.resolve(host, port); //this gives us the IP. Note, it will be an IP already lol.

  //Connect TCP socket
  boost::asio::connect(socket, results.begin(), results.end() );
  
  //construct a (GET) request string...
  const int version = 11;
  boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::get, target, version};
  req.set(boost::beast::http::field::host, host);
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  
  req.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
  req.prepare_payload();

  //Write request (GET request) to the socket.
  boost::beast::http::write( socket, req );


  //Wait and read response
  boost::beast::flat_buffer buffer;
  boost::beast::http::response<boost::beast::http::dynamic_body> res;
  
  boost::beast::http::read(socket, buffer, res);

  std::string mybody = boost::beast::buffers_to_string( res.body().data()  );
#ifdef HTTP_DEBUG_LEVEL
#if HTTP_DEBUG_LEVEL > 10
  std::cout << "Result (API&PAYLOAD): " << res.result() << std::endl;
  std::fprintf(stdout, "BODY: [%s]\n", mybody.c_str());
#endif
#endif
    
  //https://www.boost.org/doc/libs/master/boost/beast/http/status.hpp e.g. 200, 201, 202
  if( res.result() == boost::beast::http::status::ok ||
      res.result() == boost::beast::http::status::created ||
      res.result() == boost::beast::http::status::accepted )
    {
      auto response_json = json::parse( mybody );
      return response_json;
    }
  else
    {
      json j;
      return j;
    }
  }
  catch( boost::exception& e )
    {
      fprintf(stderr, "REV: caught some error from boost, returning empty (failure...)\n");
      std::string info = boost::diagnostic_information(e);
      std::cerr << info << std::endl;
      json j;
      return j;
    }
}

json post_json( const std::string& baseurl, const std::string& api_action, const std::string& port="80" )
{
  try
    {
  fprintf(stdout, "Will attempt post json with [%s]\n", baseurl.c_str());
  boost::asio::ip::address addr = boost::asio::ip::address::from_string(baseurl);
  #if HTTP_DEBUG_LEVEL > 10
  std::cout << "Valid IP address " << baseurl << std::endl;
  #endif
  
  //Create TCP socket
  auto const host = baseurl;
  //auto const port = "80";
  auto const target = api_action;
  
  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver resolver{ioc};
  boost::asio::ip::tcp::socket socket{ioc};
  auto const results = resolver.resolve(host, port); //this gives us the IP. Note, it will be an IP already lol.
  //std::cout << results << std::endl;
  std::cout << results.size() << std::endl;
    
  //Connect TCP socket
  boost::asio::connect(socket, results.begin(), results.end() );
  
  //construct a (POST) request string...
  const int version = 11;
  boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::post, target, version};
  req.set(boost::beast::http::field::host, host);
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  
  req.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");

  std::cout << req << std::endl;
  req.prepare_payload();
  
  //Write to socket
  boost::beast::http::write( socket, req );
  
  //Wait and read response
  boost::beast::flat_buffer buffer;
  boost::beast::http::response<boost::beast::http::dynamic_body> res;
  
  boost::beast::http::read(socket, buffer, res);

  std::string mybody = boost::beast::buffers_to_string( res.body().data()  );
#ifdef HTTP_DEBUG_LEVEL
#if HTTP_DEBUG_LEVEL > 10
  std::cout << "Result (API&PAYLOAD): " << res.result() << std::endl;
  std::fprintf(stdout, "BODY: [%s]\n", mybody.c_str());
#endif
#endif
    
  //https://www.boost.org/doc/libs/master/boost/beast/http/status.hpp e.g. 200, 201, 202
  if( res.result() == boost::beast::http::status::ok ||
      res.result() == boost::beast::http::status::created ||
      res.result() == boost::beast::http::status::accepted )
    {
      auto response_json = json::parse( mybody );
      return response_json;
    }
  else
    {
      json j;
      return j;
    }
  }
  catch( boost::exception& e )
    {
      fprintf(stderr, "REV: caught some error from boost, returning empty (failure...)\n");
      std::string info = boost::diagnostic_information(e);
      std::cerr << info << std::endl;
      json j;
      return j;
    }
}


json post_json( const std::string& baseurl, const std::string& api_action, const json& payload, const std::string& port="80" )
{
#if HTTP_DEBUG_LEVEL > 10
  fprintf(stdout, "BASE: [%s]  API: [%s]\n", baseurl.c_str(), api_action.c_str() );
  std::cout << payload << std::endl;
#endif
  
  try
    {
  boost::asio::ip::address addr = boost::asio::ip::address::from_string(baseurl);
#if HTTP_DEBUG_LEVEL > 10
  std::cout << "Valid IP address " << baseurl << std::endl;
#endif
  
  //Create TCP socket
  auto const host = baseurl;
  //auto const port = "80";
  auto const target = api_action;
  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver resolver{ioc};
  boost::asio::ip::tcp::socket socket{ioc};
  auto const results = resolver.resolve(host, port); //this gives us the IP. Note, it will be an IP already lol.

  //Connect TCP socket
  boost::asio::connect(socket, results.begin(), results.end() );
  
  //construct a request string...
  const int version = 11;
  boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::post, target, version};
  req.set(boost::beast::http::field::host, host);
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
  req.body() = payload.dump();
  req.prepare_payload();

  //Write to socket
  boost::beast::http::write( socket, req );

  //Wait and read response
  boost::beast::flat_buffer buffer;
  boost::beast::http::response<boost::beast::http::dynamic_body> res;
  
  boost::beast::http::read(socket, buffer, res);

  std::string mybody = boost::beast::buffers_to_string( res.body().data()  );
#ifdef HTTP_DEBUG_LEVEL
#if HTTP_DEBUG_LEVEL > 10
  std::cout << "Result (API&PAYLOAD): " << res.result() << std::endl;
  std::fprintf(stdout, "BODY: [%s]\n", mybody.c_str());
#endif
#endif
    
  //https://www.boost.org/doc/libs/master/boost/beast/http/status.hpp e.g. 200, 201, 202
  if( res.result() == boost::beast::http::status::ok ||
      res.result() == boost::beast::http::status::created ||
      res.result() == boost::beast::http::status::accepted )
    {
      auto response_json = json::parse( mybody );
      return response_json;
    }
  else
    {
      json j;
      return j;
    }
  }
  catch( boost::exception& e )
    {
      fprintf(stderr, "REV: caught some error from boost, returning empty (failure...)\n");
      std::string info = boost::diagnostic_information(e);
      std::cerr << info << std::endl;
      json j;
      return j;
    }
  
}



json update_json( const std::string& baseurl, const std::string& api_action, const json& payload, const std::string& port="80" )
{
  try
    {
  boost::asio::ip::address addr = boost::asio::ip::address::from_string(baseurl);
#if HTTP_DEBUG_LEVEL > 10
  std::cout << "Valid IP address " << baseurl << std::endl;
#endif
  
  //Create TCP socket
  auto const host = baseurl;
  //auto const port = "80";
  auto const target = api_action;
  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver resolver{ioc};
  boost::asio::ip::tcp::socket socket{ioc};
  auto const results = resolver.resolve(host, port); //this gives us the IP. Note, it will be an IP already lol.
  
  //Connect TCP socket
  boost::asio::connect(socket, results.begin(), results.end() );
  
  //construct a request string...
  const int version = 11;
  boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::put, target, version};
  req.set(boost::beast::http::field::host, host);
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
  req.body() = payload.dump();
  req.prepare_payload();

  //Write to socket
  boost::beast::http::write( socket, req );

  //Wait and read response
  boost::beast::flat_buffer buffer;
  boost::beast::http::response<boost::beast::http::dynamic_body> res;
  
  boost::beast::http::read(socket, buffer, res);

  std::string mybody = boost::beast::buffers_to_string( res.body().data()  );
#ifdef HTTP_DEBUG_LEVEL
#if HTTP_DEBUG_LEVEL > 10
  std::cout << "Result (API&PAYLOAD): " << res.result() << std::endl;
  std::fprintf(stdout, "BODY: [%s]\n", mybody.c_str());
#endif
#endif
    
  //https://www.boost.org/doc/libs/master/boost/beast/http/status.hpp e.g. 200, 201, 202
  if( res.result() == boost::beast::http::status::ok ||
      res.result() == boost::beast::http::status::created ||
      res.result() == boost::beast::http::status::accepted )
    {
      auto response_json = json::parse( mybody );
      return response_json;
    }
  else
    {
      json j;
      return j;
    }
  }
  catch( boost::exception& e )
    {
      fprintf(stderr, "REV: caught some error from boost, returning empty (failure...)\n");
      std::string info = boost::diagnostic_information(e);
      std::cerr << info << std::endl;
      json j;
      return j;
    }
}



std::string wait_for_status( const size_t maxtimeouts, const std::string& baseurl, const std::string& api_action, const std::string& key, const std::vector<std::string>& acceptvalues )
{
  size_t ntries=0;
  const uint64_t sleepms=250;
  
  while( ntries < maxtimeouts )
    {
      auto rj = post_json( baseurl, api_action );
      if( rj.contains(key) && (std::find( acceptvalues.begin(), acceptvalues.end(), rj[key] ) != acceptvalues.end()) )
	{
	  return rj[key];
	}
      else
	{
	  ++ntries;
	  std::this_thread::sleep_for( std::chrono::milliseconds(sleepms) );
	}
    }
  return "";
}
