
//REV: just a stupid simple mutexed deque (with max size to stop big errors)

//See real streaming...
//Note files read "back" i.e. buffer represents LAST 4096 bytes read, so at beginning I read 4096 bytes [0,4095],
// buffer start is 0 in file, buffer end is 4095, and I will read next 4096 byte, i.e. pos.


//OK, I could have the condition variable assume to use the same mutex mu used internally.
// But that may get in the way of lots of fast writes?

// As it is I expect user to use pmu and pcv. Note, the whole point of pmu is to lock for the checked condition itself, not
// for the condition_variable?


//https://ffmpeg.org/doxygen/3.4/aviobuf_8c_source.html#l01291
#pragma once

#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>

template <typename TT>
struct mutexed_buffer_element
{
private:
  TT value;
  uint64_t sizebytes;
  
public:
  mutexed_buffer_element( TT& v, const uint64_t szbytes=DEFAULT_MBUF_PERELEMSIZE_BYTES )
    : value(v), sizebytes(szbytes)
  {return;}
  
};

//end is newest, beginning is oldest (time)
template <typename T>
struct mutexed_buffer
{
private:
  std::deque<T> buf;
  std::deque<T> bubuf;

  std::deque<uint64_t> bufszbytes;
  std::deque<uint64_t> bubufszbytes;
  
  std::mutex mu; //made it into public mutex
  bool bu;
  uint64_t written;
  
  size_t maxsize;
  size_t maxsizebytes;
    
  //network should either write directly into this in recv(), or keep its own buffer and block etc.? for IO?
  //after it receives, it mutex (copies) into this. And this is copied out of. Fine. No loss.

public:
  mutexed_buffer( const size_t sz=MAX_MBUF_ELEMSIZE, const uint64_t szbytes=MAX_MBUF_BYTESIZE )
    : bu(false), maxsize(sz), maxsizebytes(szbytes)
  { return; }
  
  std::mutex pmu; //made it into public mutex
  std::condition_variable cv; //public cv
  
  void backup_on()
  {
    const std::lock_guard<std::mutex> lock(mu);

    if( bu )
      {
	fprintf(stderr, "WTF bu already on?!?!?!?!?\n");
	exit(1);
	return;
      }
    
    //REV: burn previously existing?
    bubuf.clear();
    //std::copy( buf.begin(), buf.end(), std::back_inserter(bubuf) );
    written=0;
    std::fprintf(stdout, "MBUF -- backup ON (initial backup buf size %ld)\n", bubuf.size() );
    bu = true;
    return;
  }

  
  void backup_off()
  {
    const std::lock_guard<std::mutex> lock(mu);
    bu = false;
    fprintf(stdout, "TOTAL WRITTEN during BU period [%ld] (Current Size: [%ld]   Backup Dregs: [%ld])\n", written, buf.size(), bubuf.size());
    return;
  }

  bool is_backup()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return bu;
  }

  
  /*uint64_t get_size_bytes()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_size_bytes();
    }*/
  
  //Used to be add()
  bool copyfrom( const std::vector<T>& toadd, std::vector<uint64_t> szbytes = std::vector<uint64_t>() )
  {
    bool good=true;
    
    {
      const std::lock_guard<std::mutex> lock(mu);

      if( szbytes.size() != toadd.size() )
	{
	  szbytes.resize(  toadd.size(), DEFAULT_MBUF_PERELEMSIZE_BYTES );
	}
      
      if(bu) { _backup( toadd, szbytes ); }
      
      std::copy( toadd.begin(),
		 toadd.end(),
		 std::back_inserter(buf) );
      
      std::copy( szbytes.begin(),
		 szbytes.end(),
		 std::back_inserter(bufszbytes) );
      
      uint64_t mybytes = _get_size_bytes( bufszbytes );
      if( maxsizebytes > 0 &&
	  mybytes > maxsizebytes )
	{
	  fprintf(stderr, "^^^^^^^^^^ WARNING/ERROR (REV MBUF) -- byte size > max size ([%lu] vs max [%lu]) (%lu elements)\n", mybytes, maxsizebytes, buf.size());
	  good = false;
	}
      
      _fixsizes();
    }
    
    cv.notify_all();
    return good;
  }

  void set_maxsize( const size_t sz )
  {
    maxsize=sz;
  }

  void set_maxsize_bytes( const size_t sz )
  {
    maxsizebytes=sz;
  }

  //REV: drop from beginning!!
  bool _fixsize( std::deque<T>& _buf, std::deque<uint64_t>& _bufszbytes )
  {
    bool erasedsome=false;
    if( maxsize > 0)
      {
	const size_t nowsize=_buf.size();
	if( nowsize > maxsize )
	  {
	    auto torm = nowsize-maxsize;
	    _buf.erase(_buf.begin(),
		       _buf.begin()+torm);
	    _bufszbytes.erase(_bufszbytes.begin(),
			      _bufszbytes.begin()+torm);
	    erasedsome=true;
	  }
      }
    
    while( maxsizebytes > 0 &&
	   _get_size_bytes( _bufszbytes ) > maxsizebytes )
      {
	_buf.pop_front();
	_bufszbytes.pop_front();
	erasedsome=true;
      }
    return erasedsome;
  }

  uint64_t _get_size_bytes( const std::deque<uint64_t>& _bufszbytes ) const
  {
    uint64_t sz=0;
    for( size_t i=0; i < _bufszbytes.size(); ++i )
      {
	sz += _bufszbytes[i];
      }
    return sz;
  }

  
  
  void _fixsizes()
  {
    bool erasedsome = _fixsize(buf, bufszbytes);
    bool erasedsomebu = _fixsize(bubuf, bubufszbytes);
    if( erasedsome ) { fprintf(stderr, "REV: WARNING, erased some from mbuf!\n"); }
    if( erasedsomebu ) { fprintf(stderr, "REV: WARNING, erased some from mbuf BACKUP!\n"); }
  }
  
  
  //REV: THIS MAY MAKE MEMORY CONTAINED IN PASSED CONTAINER TRASH!
  bool movefrom( const std::vector<T>& toadd, std::vector<uint64_t> szbytes = std::vector<uint64_t>() )
  {
    bool good=true;
    {
      const std::lock_guard<std::mutex> lock(mu);

      if( szbytes.size() != toadd.size() )
	{
	  szbytes.resize(  toadd.size(), DEFAULT_MBUF_PERELEMSIZE_BYTES );
	}
      
      if(bu) { _backup( toadd, szbytes ); }
      
      std::move( toadd.begin(),
		 toadd.end(),
		 std::back_inserter(buf) );

     
      
      std::move( szbytes.begin(),
		 szbytes.end(),
		 std::back_inserter(bufszbytes) );
       
      uint64_t mybytes = _get_size_bytes( bufszbytes );
      if( maxsizebytes > 0 &&
	  mybytes > maxsizebytes )
	{
	  fprintf(stderr, "^^^^^^^^^^ WARNING/ERROR (REV MBUF) -- byte size > max size ([%lu] vs max [%lu])\n", mybytes, maxsizebytes);
	  good = false;
	}
            
      _fixsizes();
    }
    
    cv.notify_all();
    return good;
  }

  void _backup( const T& toadd, const uint64_t _elemszbytes )
  {
    bubuf.push_back(toadd);
    bubufszbytes.push_back(_elemszbytes);
    written += 1;
    return;
  }
  
  bool addone( const T& toadd, const uint64_t _elemszbytes=DEFAULT_MBUF_PERELEMSIZE_BYTES )
  {
    bool good = true;
    {
      const std::lock_guard<std::mutex> lock(mu);
      
      if(bu) { _backup( toadd, _elemszbytes ); }
      
      buf.push_back(toadd);
      bufszbytes.push_back(_elemszbytes);
      
      uint64_t mybytes = _get_size_bytes( bufszbytes );
      
      if( maxsizebytes > 0 &&
	  mybytes > maxsizebytes )
	{
	  fprintf(stderr, "^^^^^^^^^^ WARNING/ERROR (REV MBUF) -- byte size > max size ([%lu] vs max [%lu])\n", mybytes, maxsizebytes);
	  good = false;
	}
      
      _fixsizes();
    }
    cv.notify_all();
    return good;
  }

  //REV: returns copy without popping of course.
  std::vector<T> peekfront( const size_t ntopeek )
  {
    std::vector<T> result;
    const std::lock_guard<std::mutex> lock(mu);
    size_t copied = buf.size() > ntopeek ? ntopeek : buf.size();
    std::copy( buf.begin(),
	       buf.begin()+copied,
	       std::back_inserter(result) );
    return copied;
  }
  
  void _backup( const std::vector<T>& toadd, std::vector<uint64_t> szbytes )
  {
    std::copy( toadd.begin(),
	       toadd.end(),
	       std::back_inserter(bubuf) );
    
    std::copy( szbytes.begin(),
	       szbytes.end(),
	       std::back_inserter(bubufszbytes) );
    
    written+=toadd.size();
    
    return;
  }
  
  size_t popto( T* topopto, const size_t ntopop )
  {
    const std::lock_guard<std::mutex> lock(mu);
    size_t moved = buf.size() > ntopop ? ntopop : buf.size();
    std::move( buf.begin(),
	       buf.begin()+moved,
	       topopto );
    
    buf.erase(buf.begin(),
	      buf.begin()+moved);

    bufszbytes.erase(bufszbytes.begin(),
		     bufszbytes.begin()+moved);
    return moved;
  }

  size_t popto( std::vector<T>& topopto, const size_t ntopop )
  {
    const std::lock_guard<std::mutex> lock(mu);
    size_t moved = buf.size() > ntopop ? ntopop : buf.size();
    std::move( buf.begin(),
	       buf.begin()+moved,
	       std::back_inserter(topopto) );
    buf.erase(buf.begin(),
	      buf.begin()+moved);
    bufszbytes.erase(bufszbytes.begin(),
		     bufszbytes.begin()+moved);
    return moved;
  }
  
  size_t popto( std::deque<T>& topopto, const size_t ntopop )
  {
    const std::lock_guard<std::mutex> lock(mu);
    size_t moved = buf.size() > ntopop ? ntopop : buf.size();
    std::move( buf.begin(),
	       buf.begin()+moved,
	       std::back_inserter(topopto) );
    buf.erase(buf.begin(),
	      buf.begin()+moved);
    bufszbytes.erase(bufszbytes.begin(),
		     bufszbytes.begin()+moved);
    return moved;
  }

  //abstract container type? instead of vector etc. etc.
  size_t popallto( std::vector<T>& topopto )
  {
    const std::lock_guard<std::mutex> lock(mu);
    size_t moved = buf.size();
    std::move( buf.begin(),
	       buf.begin()+moved,
	       std::back_inserter(topopto) );
    buf.clear();
    bufszbytes.clear();
    return moved;
  }
  
  
  
  //abstract container type? instead of vector etc. etc.
  size_t popallto( std::deque<T>& topopto )
  {
    const std::lock_guard<std::mutex> lock(mu);
    size_t moved = buf.size();
    std::move( buf.begin(),
	       buf.begin()+moved,
	       std::back_inserter(topopto) );
    buf.clear();
    bufszbytes.clear();
    return moved;
  }


  size_t popallbackupto( std::vector<T>& topopto )
  {
    const std::lock_guard<std::mutex> lock(mu);
    size_t moved = bubuf.size();
    std::move( bubuf.begin(),
	       bubuf.begin()+moved,
	       std::back_inserter(topopto) );
    bubuf.clear();
    bubufszbytes.clear();
    return moved;
  }
  

  size_t size()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return buf.size();
  }

  void clear() const
  {
    const std::lock_guard<std::mutex> lock(mu);
    buf.clear();
    return;
  }
};
