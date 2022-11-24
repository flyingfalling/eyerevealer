#pragma once

#include <limits>

// RTEYE2 Timed buffer
// Sorted by time, contains arbitrary type payload <T>
// binary search log2(N) for closest. constant add/remove time?
enum relative_timing
  {
    BEFORE,
    AFTER,
    EITHER
  };

//timed_buffer_element are stored in LOCAL TIME TYPE (e.g. uint64_t, double, etc.). We don't know correspondence to real seconds
//unless user tells us... (via set_time_type in the timed_buffer itself?) Storing per-element is wasteful.
template <typename T, typename TT>
struct timed_buffer_element
{
public:
  T payload; //contents, data, etc.
  TT timestamp; //REV: should have used a differnet timestamp? E.g. double?
  uint64_t sizebytes;
  
  timed_buffer_element( const T& p, const TT t, const uint64_t _sizebytes )  // = DEFAULT_TBUF_PERELEMSIZE_BYTES )
    : payload(p), timestamp(t), sizebytes(_sizebytes)
  {  return; }
  
  ~timed_buffer_element()
  {
  }
  
  //Overload < operator (for binary search/sorting).
  friend bool operator<(const timed_buffer_element<T,TT>& l, const timed_buffer_element<T,TT>& r)
  {
    return (l.timestamp < r.timestamp);
  }

  friend bool operator<(const timed_buffer_element<T,TT>& l, const TT& r)
  {
    return (l.timestamp < r);
  }

  friend bool operator<( const TT& l, const timed_buffer_element<T,TT>& r)
  {
    return (l < r.timestamp);
  }
};

//REV: *thread safe!* (although wasteful for multiple readers)
//REV: even better with "rotating" buffer that uses old allocations of T etc. and overwrites them when I run out of space.
//REV: really should use an abstract type...but assume user won't be stupid and will give a type that has < defined.
//REV: for doubles, == may be difficult...should use SMALL_NUM error check. For us it does not matter though, since we never care ==
template <typename T, typename TT>
struct timed_buffer
{
private:
  //REV: push to end (i.e. newest). Pop from front. this way it is smallest at beginning to largest at end, assuming mono increment
  // time
  //REV: need to lock?
  std::deque<timed_buffer_element<T,TT>> buffer;
  //std::set<timed_buffer_element<T,TT>> buffer;
  TT max_time_interval; //max time to hold. Will start dropping oldest after this (do I really need to do each time?)
  
  bool zeroed;
  TT zero_time;
  
  double zero_time_clocksec; //This will be set to the clock-time (application time) corresponding to the local zero time of this
  // timed buffer. Used to easily access elements from application time without problems of unsigned underflow in local time type.
  
  uint64_t maxsize;
  uint64_t maxsizebytes;
  bool memorydrop; //Drop oldest frames when estimated memory size exceeds limits
  
  double timebase_hz_sec; //Information about the timebase (units) -- how many are needed to make 1 second.
  // if msec, 1000
  // if 1/90000 -> 90000
  // if nsec -> 1e9 etc.
  
  size_t nmissedrequests;
  size_t ndroppedmemory;

  size_t dropped_skip_thresh;
  const size_t DROP_SKIP = 200;
  
    
private:
  double tb_hz_sec() const
  {
    if( timebase_hz_sec <= 0 )
      {
	fprintf(stderr, "WARNING, you are accessing timebase without setting it first!\n");
	exit(1);
      }
    return timebase_hz_sec;
  }
  
  
  static bool issigned()
  {
    return std::is_signed<TT>::value;
  }
  
  //REV: should this get FIRST (oldest), or LAST (newest)? Assumedly the most recent data will represent "now"
  //But it means that I will not be able to zero-refer to other entries in my array.
  
  void _rezero_time( const TT& ts )
  {
    zero_time = ts;
    zeroed = true;
    return;
  }
  
  void _set_timebase_hz_sec( const double hz_sec )
  {
    if( hz_sec <= 0 )
      {
	fprintf(stdout, "Setting timebase <= 0...you're off your fucking chain, mate\n");
	exit(1);
      }
    timebase_hz_sec = hz_sec;
    return;
  }

  //May return illegal times?
  //Disallow negative times.

  //Returns zero time (offset from zero) of raw PTS input.
  TT _zerooffset_from_time( const TT& actualpts ) const
  {
    if( zeroed )
      {
	if (actualpts > zero_time )
	  {
	    return (actualpts - zero_time);
	  }
	else
	  {
	    if( !issigned() )
	      {
#if DEBUG_LEVEL > 0
		fprintf(stderr, "WARNING/ERROR -- zero_time > requested time (result would be negative). Returning 0.\n");
#endif
		TT zero = 0;
		return zero;
	      }
	    else //signed
	      {
		return (actualpts - zero_time);
	      }
	  }
      }
    else
      {
	//std::fprintf(stderr, "WARNING/ERROR: REQUESTED ZEROED_TIME, but not set yet?!\n");
	return actualpts;
      }
  }
  uint64_t _get_size_bytes() const
  {
    uint64_t sbytes=0;
    for( auto& i : buffer )
      {
	sbytes += i.sizebytes;
      }
    return sbytes;
  }
  
  double _time_in_seconds( const TT& arg ) const
  {
    return arg / tb_hz_sec();
  }
  
  //REV: *RAW* local time from raw seconds (unzeroed)
  TT _seconds_in_time( const double secs ) const
  {
    return (secs * tb_hz_sec()); //+0.5...?if I know it is an int type.
  }

  //REV: adjust zero time forward or backwards by secs. This is the same as setting the zero_time_clocksec...
  /*void _adjust_zerotime_secs( const double secs )
  {
    //REV: can't guarantee unsigned types wouldn't go less than 0? :(
    //Try using doubles...
    double currzsecs = _time_in_seconds( zero_time );
    double newzsecs = currzsecs + secs;
    TT newz = _seconds_in_time( newzsecs );
    if( newzsecs < 0 && !issigned() )
      {
	//REV: can't adjust it any further!
	fprintf(stderr, "REV: can't adjust zerotime any further backwards than 0 in unsigned type!\n");
	zero_time = 0;
      }
    else
      {
	zero_time = newz;
      }
      }*/

  //Given clock seconds (implicitly zeroed), what is my true raw timestamp
  
  TT _time_from_clock_secs( const double secs ) const
  {
    TT mytime = _seconds_in_time( secs - zero_time_clocksec ); //raw local time corresponding to  input (offset by clock offset)
    TT truetime = _time_from_zerooffset( mytime ); //zeroed local time (time from local zero)
    /*if( std::is_same<TT,uint64_t>::value )
      {
	fprintf(stdout, "For [%lf]   Mine: [%ld]  Zero: [%ld]   (True: [%ld])\n", secs, mytime, zero_time, truetime);
	}*/
#if DEBUG_LEVEL>1000
    std::cout << "For requested sampletime " << secs <<
      "   Mine: " << mytime <<
      "    Zero: " << zero_time <<
      "  (Truetime: " << truetime <<
      ")" << std::endl;
#endif
    return truetime;
  }
  
  //secs is from user's assumed zero. Returns true timebase corresponding to that offset in seconds from set zero.
  //i.e. passed time is lower than "true" saved timestamps.
  TT _time_from_zeroed_secs( const double secs ) const
  {
    return _time_from_clock_secs( secs );
  }

  //Given a (raw) local time, what is the zeroed clock seconds
  double _clock_secs_from_time( const TT& arg ) const
  {
    //zerosecs corresponds to my zero time in seconds
    //This would be zerotime in seconds if raw zero was clocksec zero (but it is not)
    double zerosecs = _time_in_seconds( zero_time );

    //This would be arg in seconds if raw zero was clocksec zero (but it is not)
    double argsecs = _time_in_seconds( arg );

#if DEBUG_LEVEL > 100
    std::cout << "Native zero: " << zero_time << " Native arg: " << arg << "  Time base: " << tb_hz_sec() << "   Zero in sec: " << zerosecs << "    argsecs:  " << argsecs << std::endl;
#endif

    //If they passed me the raw time corresponding to my zero, I should return the zero_time_clocksec.
    //If they passed me a time corresponding to 1 second past my zero, it should return zero_time_clocksec+1, etc.
    return (argsecs - zerosecs) + zero_time_clocksec;
  }

  double _zeroed_secs_from_time( const TT& arg ) const
  {
    return _clock_secs_from_time( arg );
  }
  

  //If I pass zero to this function, it will return my zero_time.
  //If I pass -1, it will return zerotime-1, etc.
  TT _time_from_zerooffset( const TT& fromzero ) const
  {
    if( zeroed )
      {
	//May still return negative number if fromzero < zero?
	if( issigned() )
	  {
	    return (fromzero + zero_time);
	  }
	else //LOL what...it doesn't matter right?
	  {
	    return (fromzero + zero_time);
	  }
      }
    else
      {
	std::fprintf(stderr, "WARNING/ERROR: REQUESTED ZEROED_TIME, but not set yet?!\n");
	return 0; //fromzero;
      }
  }
  
  std::deque<timed_buffer_element<T,TT>> _copy_data() const
  {
    auto ret = buffer; //deep copy...?
    return ret;
  }

  
  void _dump_data() const
  {
    std::cout << "DUMPING TIMED BUFFER\n\n";
    TT lastts = std::numeric_limits<TT>::min();
    for( auto& x : buffer )
      {
	if( x < lastts )
	  {
	    std::fprintf(stderr, "ERROR, timed buffer not in order!\n");
	  }
	std::cout << x.timestamp << std::endl;
      }
    //auto ret = buf; //deep copy
    //return ret;
  }
  
  
  std::deque<timed_buffer_element<T,TT>>::const_iterator _get_timed_element_beforeeq( const TT& target_time ) const
  {
    if( buffer.empty() )
      {
	return buffer.end();
      }
    else
      {
	/*typename*/ /*std::deque<timed_buffer_element<T,TT>>::const_iterator*/ auto iter = std::lower_bound( buffer.begin(), buffer.end(), target_time ); //uses operator<
	//iter points to first element NOT less than target_time

	//if iter is beginning, it means there are none less than, i.e. begin() is >= target
	if( buffer.begin() == iter )
	  {
	    if( target_time == iter->timestamp ) //equal
	      {
		return iter;
	      }
	    //else doesnt exist (all strictly > target value)
	  }
	else //if( buffer.end() == iter ) or other
	  {
	    //iter-1 must point to a legal value (either end()-1, or some other x-1, where x is not begin()
	    iter--;
	    if( iter->timestamp <= target_time )
	      {
		return iter;
	      }
	    //else none exist before target (all >)
	  }
      } //else NOT EMPTY
    return buffer.end();
  }


  std::deque<timed_buffer_element<T,TT>>::const_iterator _get_timed_element_aftereq( const TT& target_time ) const
  {
    if( buffer.empty() )
      {
	return buffer.end();
      }
    else
      {
	/*typename*/ /*std::deque<timed_buffer_element<T,TT>>::const_iterator*/ auto iter = std::lower_bound( buffer.begin(), buffer.end(), target_time ); //uses operator<
	//iter points to first element NOT less than target_time
	
	//if iter is beginning, it means there are none less than, i.e. begin() is >= target
	if( buffer.begin() == iter )
	  {
	    return iter;
	  }
	else //if( buffer.end() == iter ) or other
	  {
	    if( buffer.end() == iter ) // no element is >= targ
	      {
		return iter;
	      }
	    else //some element is >= targ
	      {
		return iter;
	      }
	  }
      } //else NOT EMPTY
    return buffer.end(); 
  }



  std::deque<timed_buffer_element<T,TT>>::const_iterator _get_timed_element_closest( const TT target_time ) const
  {
    if( buffer.empty() )
      {
	return buffer.end();
      }
    else
      {
	auto iter = std::lower_bound( buffer.begin(), buffer.end(), target_time ); //uses operator<
	//iter points to first element NOT less than target_time
	
	//if iter is beginning, it means there are none less than, i.e. begin() is >= target
	if( buffer.begin() == iter )
	  {
	    return iter;
	  }
	else //if( buffer.end() == iter ) or other
	  {
	    if( buffer.end() == iter ) //only need to check the one before it (that must be closet), because none
	      //evaluated to <. First element in range NOT less than. So none were not less than, i.e. none >=
	      //So, all must be less than targtime
	      {
		iter--;
		return iter;
	      }
	    else //else one is before and one is after.
	      {
		//iter points to first element >= target
		TT deltaafter = iter->timestamp - target_time;
		iter--;
		TT deltabefore = target_time - iter->timestamp;
		if( deltabefore < deltaafter )
		  {
		    return iter;
		  }
		else
		  {
		    iter++; //go back
		    return iter;
		  }
	      }
	  }
      } //else NOT EMPTY
    return buffer.end();
  }

  //REV: this will drop before (not including returned) only if successfully returned within maxdelta.
  std::vector<timed_buffer_element<T,TT>> _get_timed_element_range( const TT target_time_min, const TT target_time_max, const TT maxdelta, const bool dropbefore )
  {
    std::vector<timed_buffer_element<T,TT>> result;
    if( target_time_min >= target_time_max ) { return result; }

    //get lowest one inside [targ_time_min-maxdelta, targ_time_max]   (note, weird case where targ max is really close to min...ignore)
    //Shit, handling unsigned things is nasty in here... I really should just use signed everywhere.
    TT tmpmin = maxdelta > target_time_min ? 0 : target_time_min - maxdelta;
    auto startiter = _get_timed_element_aftereq( tmpmin );
    if( startiter == buffer.end() || startiter->timestamp > target_time_max+maxdelta )
      {
	return result;
      }
    auto enditer = _get_timed_element_beforeeq( target_time_max + maxdelta );
    if( enditer == buffer.end() || enditer->timestamp < tmpmin )
      {
	return result;
      }

    //I have two legal iterators, which may be equal. I should be able to get to enditer, using startiter...
    ++enditer;
    result = std::vector<timed_buffer_element<T,TT>>( startiter, enditer );
    //REV: drop before.
    if( dropbefore )
      {
	buffer.erase( buffer.begin(), startiter );
#ifdef SHRINK_TBUF_TO_FIT
	buffer.shrink_to_fit();
#endif
      }
    return result;
  }
  


  //REV: this will drop before (not including returned) only if successfully returned within maxdelta.
  std::optional<timed_buffer_element<T,TT>> _get_timed_element( const TT target_time, const TT maxdelta, const relative_timing reltiming, const bool dropbefore )
  {
    if( buffer.empty() )
      {
	return std::nullopt;
      }

    if( relative_timing::BEFORE == reltiming )
      {
	auto resultiter = _get_timed_element_beforeeq( target_time ); //Will this make a true (deep?) copy?
	
	if( resultiter == buffer.end() )
	  {
	    return std::nullopt;
	  }
	
	if( (resultiter->timestamp + maxdelta) < target_time ) //outside maxdelta...
	  {
	    return std::nullopt;
	  }
	else
	  {
	    //drop before
	    if( dropbefore )
	      {
		buffer.erase(buffer.begin(), resultiter); //Erase removes [first, last)
#ifdef SHRINK_TBUF_TO_FIT
		buffer.shrink_to_fit();
#endif
	      }
	    return *resultiter; //copy?
	  }
      } //if BEFORE
    else if( relative_timing::AFTER == reltiming )
      {
	auto resultiter = _get_timed_element_aftereq( target_time );
	if( resultiter == buffer.end() )
	  {
	    return std::nullopt;
	  }
	
	if( (target_time + maxdelta) < resultiter->timestamp ) //outside maxdelta...
	  {
	    return std::nullopt;
	  }
	else
	  {
	    if( dropbefore )
	      {
		buffer.erase(buffer.begin(), resultiter); //Erase removes [first, last)
#ifdef SHRINK_TBUF_TO_FIT
		buffer.shrink_to_fit();
#endif
	      }
	    return *resultiter;
	  }
      } //if AFTER
    else if( relative_timing::EITHER == reltiming )
      {
	//auto result = _get_timed_element_closest( target_time );
	auto resultiter = _get_timed_element_closest( target_time );
	if( resultiter == buffer.end() )
	  {
	    return std::nullopt;
	  }
	
	const TT t = resultiter->timestamp;
	TT delta;
	if( t > target_time )
	  { delta = t - target_time; }
	else
	  { delta = target_time - t; }
	if( delta <= maxdelta )
	  {
	    if( dropbefore )
	      {
		buffer.erase(buffer.begin(), resultiter); //Erase removes [first, last)
#ifdef SHRINK_TBUF_TO_FIT
		buffer.shrink_to_fit();
#endif
	      }
	    return *resultiter;
	  }
	else
	  {
	    return std::nullopt;
	  }
      } //if EITHER
    
    fprintf(stdout, "If you got here, something is broken!\n");
    exit(1);
    //catch everything else (should be nothing)
    return std::nullopt;
  } //end get_timed_element



  
  //deletes everything before me and returns me. Gets the most recent time LESS OR EQUAL TO target_time
  //shit should have returned iter to it?
  std::optional<timed_buffer_element<T,TT>> _get_consume_upto( const TT target_time )
  {
    
    if( buffer.empty() )
      {
	return std::nullopt;
      }
    else
      {
	auto iter = std::lower_bound( buffer.begin(), buffer.end(), target_time ); //uses operator<
	//iter points to first element NOT less than target_time

	//if iter is beginning, it means there are none less than, i.e. begin() is >= target
	if( buffer.begin() == iter )
	  {
	    if( target_time == iter->timestamp ) //equal
	      {
		//delete all up to iter (not including)
		buffer.erase( buffer.begin(), iter );
		auto retval = *iter;
		buffer.pop_front(); //delete front
#ifdef SHRINK_TBUF_TO_FIT
		buffer.shrink_to_fit();
#endif
		return retval;
	      }
	    //else //doesnt exist (all strictly > target value)
	  }
	else //if( buffer.end() == iter ) or other
	  {
	    //iter-1 must point to a legal value (either end()-1, or some other x-1, where x is not begin()
	    iter--;
	    if( iter->timestamp <= target_time )
	      {
		buffer.erase( buffer.begin(), iter );
		auto retval = *iter;
		buffer.pop_front(); //delete front
#ifdef SHRINK_TBUF_TO_FIT
		buffer.shrink_to_fit();
#endif
		return retval;
	      }
	  }
      } //else NOT EMPTY
    return std::nullopt; //catch.
  } //consume_upto

  
  
  std::optional<timed_buffer_element<T,TT>> _get_consume_first()
  {
    if( buffer.empty() )
      {
	return std::nullopt;
      }
    else
      {
	auto retval = buffer.front();
	buffer.pop_front(); //delete front
#ifdef SHRINK_TBUF_TO_FIT
	buffer.shrink_to_fit();
#endif
	return retval;
      }
    return std::nullopt; //catch
  }

  std::optional<timed_buffer_element<T,TT>> _get_consume_last()
  {
    if( buffer.empty() )
      {
	return std::nullopt;
      }
    else
      {
	auto retval = buffer.back();
	buffer.pop_back(); //delete front
#ifdef SHRINK_TBUF_TO_FIT
	buffer.shrink_to_fit();
#endif
	return retval;
      }
    return std::nullopt; //catch
  }

   
  std::optional<timed_buffer_element<T,TT>> _get_first() const
  {
    if( buffer.empty() )
      {
	return std::nullopt;
      }
    else
      {
	return buffer.front();
      }
    return std::nullopt; //catch
  }

    
  std::optional<timed_buffer_element<T,TT>> _get_last() const
  {
    if( buffer.empty() )
      {
	return std::nullopt;
      }
    else
      {
	return buffer.back();
      }
    return std::nullopt; //catch
  }
  
  

  const TT _get_held_time_interval() const
  {
    if( buffer.size() < 2 )
      {
	return 0; //how to indicate empty? What to do in empty case, if requested? Return empty std::any?
      }
    //REV: assert or check buffer contains more than 1 element? If it contains 1, time interval held is 0 (only most recent)
    const TT interval = buffer.back().timestamp - buffer.front().timestamp;
    return interval;
  }


  //REV: no, this was not shadowing with timestamp, since timestamp was a member of the element, not the buffer.
  //REV: note, this is reference to elem, but we want to support move semantics? Always "copy" into here? User is responsible for
  //REV: that I guess
  //void _add( const T& elem, const TT _timestamp, const uint64_t _sizebytes )
  void _add( const T& elem, const TT _timestamp, const uint64_t _sizebytes )
  {
    //REV: emplace at location based on lower_bound -- first one that does not evaulate less than.
    
    if( false == buffer.empty() )
      {
	const TT lasttime = buffer.back().timestamp;
	if( lasttime < _timestamp )
	  {
	    buffer.emplace_back( elem, _timestamp, _sizebytes );
	  }
	else if( _timestamp == lasttime )
	  {
	    //#if DEBUG_LEVEL > 0
	    //std::fprintf(stderr, "WARNING: attempting to push back time to buffer, whose last element (multiple same timestamps)\n", _timestamp, lasttime );

#if DEBUG_LEVEL > 50
	    std::cerr << std::setprecision(std::numeric_limits<TT>::digits10) << "WARNING: attempting to push back time [" << _timestamp << "] to buffer, whose last element is [" << lasttime << "] (multiple same timestamps)" << std::endl;
#endif
	    if( _timestamp != lasttime )
	      {
		fprintf(stderr, "Wtf, it was equal, and now it is not?!\n");
		exit(1);
	      }
	    buffer.emplace_back( elem, _timestamp, _sizebytes ); //REV: 7 nov 2022 -- do it anyways...?
	    //#endif
	  }
	else //nasty, have to find it
	  {
#if DEBUG_LEVEL > 50
	    //upper bound means I will put it at the "end" of same-copied guys...
	    std::cerr <<  std::setprecision(std::numeric_limits<TT>::digits10) << "WARNING: attempting to push back time [" << _timestamp << "] to buffer, whose last element is [" << lasttime << "] (out of order, i.e. will need to sort -- slow!)" << std::endl;
#endif

	    //REV: why does buffer.emplace_back work, but buffer.emplace(buffer.end(), ... ) does not
	    auto it = std::upper_bound( buffer.begin(), buffer.end(), _timestamp );

	    //REV: emplace causes error for some reason (template args are of same type? void to some deque iter shit?)
	    //insert in place (intentionally not BACK!)
	    //buffer.emplace( myit, elem, _timestamp, _sizebytes );
	    buffer.insert( it, timed_buffer_element( elem, _timestamp, _sizebytes ) );
	  }
      }
    else
      {
	buffer.emplace_back( elem, _timestamp, _sizebytes );
      }
    
    //REV: MAX SIZE
    if( maxsize>0 )
      {
	while( buffer.size() > maxsize )
	  {
	    fprintf(stderr, "(WARNING ERROR) TIMED BUFFER -- REV: removing elements down to maximum size...from FRONT! (front is oldest, back is newest) (Current size [%lu]  Max: [%lu]\n", buffer.size(), maxsize);
	    buffer.pop_front();
	  }
#ifdef SHRINK_TBUF_TO_FIT
	buffer.shrink_to_fit();
#endif
      }
  } //end _add

  void _reset()
  {
    buffer.clear();
    zeroed = false;
  }

  
  std::optional<TT> _time_after_last( const TT& question )
  {
    if( _get_last().has_value() )
      {
	if( false == std::is_signed<TT>::value )
	  {
	    if( question < _get_last().value().timestamp )
	      {
		fprintf(stderr, "Asking for time BEFORE last?! (i.e. may return negative, so returning 0...?)\n");
		return 0;
	      }
	  }
	TT retval = question - _get_last().value().timestamp;
	return retval;
      }
    return std::nullopt;
  }

  std::optional<TT> _time_before_first( const TT& question )
  {
    if( _get_first().has_value() )
      {
	if( false == std::is_signed<TT>::value )
	  {
	    if( question > _get_first().value().timestamp )
	      {
		fprintf(stderr, "Asking for time AFTER first?! (i.e. may return negative, so returning 0...?)\n");
		return 0;
	      }
	  }
	TT retval = _get_first().value().timestamp - question;
	return retval;
      }
    return std::nullopt;
  }

public:
  std::mutex mu; //public (and private) mu
  std::mutex pmu; //public (and private) mu
  std::condition_variable cv; //public cv

  //REV: can't set zero_time etc. because we don't know if
  //the type is a plain data type (we could try to static_cast
  // from 0 or something?)
  
public:
  timed_buffer( const bool _memorydrop=false, const uint64_t sz=MAX_TBUF_ELEMSIZE, const uint64_t szbytes=MAX_TBUF_BYTESIZE )
    : zeroed(false), zero_time_clocksec(0.0), maxsize(sz), maxsizebytes(szbytes), memorydrop(_memorydrop), nmissedrequests(0), ndroppedmemory(0), dropped_skip_thresh(0)
  { return; }
  
  double get_timebase_hz_sec()
  {
    const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
    return tb_hz_sec();
  }

  void set_timebase_hz_sec( const double hzsec )
  {
    const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
    timebase_hz_sec = hzsec;
    return;
  }
  
  void rezero_time_last()
  {
    const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
    if( !buffer.empty() )
      {
	_rezero_time( _get_last().value().timestamp );
      }
    return;
  }

  std::size_t size()
  {
    const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
    return buffer.size();
  }
  
  void rezero_time_first()
  {
    const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
    if( !buffer.empty() )
      {
	_rezero_time( _get_first().value().timestamp );
      }
    return;
  }

  //REV: I don't care about the T of the other TB, just that the TT is the same type...? And in the same units etc ;)
  //Assume I'm not done? I need to check timescale
  //REV: I don't want to compile this shit...
  //REV: I should make nested (derived) class. Base is just a pointer to a holder of SOMETHING? Don't know the map type.. :(
  //REV: won't work, need to know map type to fully resolve class anyways.
  template <typename TX>
  void rezero_time( const std::shared_ptr<timed_buffer<TX,TT>> othertb )
  {
    auto maybetheirzero = othertb->get_zerotime();
    auto theirclocksec = othertb->get_zerotime_clocksec();
    if( othertb->get_timebase_hz_sec() != get_timebase_hz_sec() )
      {
	fprintf(stderr, "REV: WARNING -- rezero time from another timed buffer -- timebase of self != timebase of other!\n");
	//REV: try zeroing from localtime? (clocktime)
      }
    if( maybetheirzero.has_value() )
      {
	const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
	_rezero_time( maybetheirzero.value() );
	zero_time_clocksec = theirclocksec;
      }
    return;
  }
  
  void rezero_time( const TT& ts )
  {
    const std::lock_guard<std::mutex> lock(mu); //if I already hold the lock, will the lock() inside get_first error?
    _rezero_time( ts );
    return;
  }
  
  //This gives fromzero + zero_time
  //I.e. *IF* I wanted time from zero fromzero, what PTS?
  //was time_from_zeroed
  TT time_from_zerooffset( const TT& fromzero )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _time_from_zerooffset( fromzero );
  }

  //was zeroed_from_time
  TT zerooffset_from_time( const TT& actualpts )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _zerooffset_from_time( actualpts );
  }
  
  std::optional<timed_buffer_element<T,TT>> get_consume_upto( const TT target_time )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_consume_upto( target_time );
  }

  
  std::optional<timed_buffer_element<T,TT>> get_consume_first()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_consume_first();
  }

  std::optional<timed_buffer_element<T,TT>> get_consume_last()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_consume_last();
  }
  
  void drop_first()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !buffer.empty() )
      {
	buffer.pop_front();
#ifdef SHRINK_TBUF_TO_FIT
	buffer.shrink_to_fit();
#endif
      }
  }

  //REV: unset zero, e.g. maybe user is re-connecting or some shit and needs to resync...
  void unzero_time()
  {
    const std::lock_guard<std::mutex> lock(mu);
    zeroed = false;
    return;
  }
  
  
  std::optional<timed_buffer_element<T,TT>> get_first()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_first();
  }

  std::optional<timed_buffer_element<T,TT>> get_last()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_last();
  }

  const TT get_held_time_interval()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_held_time_interval();
  }
  
  
  std::deque<timed_buffer_element<T,TT>> copy_data()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _copy_data();
  }
  
  void dump_data()
  {
    const std::lock_guard<std::mutex> lock(mu);
    _dump_data();
    return;
  }

  size_t drop_memory()
  {
    size_t ndropped=0;
    auto mybytes = _get_size_bytes();
    while( maxsizebytes > 0 &&
	   !buffer.empty() &&
	   mybytes > maxsizebytes )
      {
	buffer.pop_front(); //front is "oldest"
	mybytes = _get_size_bytes();
	++ndropped;
      }
    ndroppedmemory += ndropped;
#ifdef SHRINK_TBUF_TO_FIT
    buffer.shrink_to_fit();
#endif
    return ndropped;
  }

  //REV: note, should pass by value (not reference) for the elem? cv::Mat was showing newer memory even though the CV::MAT was
  //local to the function, wtf? Is that because of the -O2 optimization?
  //REV: not all objects are reference counted etc. though? So...maybe the optimization is doing it? Shit.

  bool add( const T& elem, const TT _timestamp,
  	    const uint64_t _sizebytes=DEFAULT_TBUF_PERELEMSIZE_BYTES, const bool _memorydrop=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    bool good = true;
    _add( elem, _timestamp, _sizebytes );
    auto mybytes = _get_size_bytes();
    if( maxsizebytes > 0 &&
	mybytes > maxsizebytes )
      {
#if DEBUG_LEVEL > 50
	fprintf(stderr, "^^^^^^^  WARNING/ERROR (REV: TIMED_BUFFER) -- stored estimated size in bytes [%lu] > max allowable set size [%lu] (Note: [%lu] elements)\n", mybytes, maxsizebytes, buffer.size() );
#endif
	good = false;

	if( _memorydrop )
	  {
	    auto dropped = drop_memory();
	    if( ndroppedmemory > dropped_skip_thresh )
	      {
		fprintf(stderr, "  +++++++  (TIMED BUFFER) DROPPED [%lu] elements to maintain max memory (Total dropped: [%lu])\n", dropped, ndroppedmemory );
		dropped_skip_thresh += DROP_SKIP;
	      }
	  }
	else
	  {
	    fprintf(stderr, "  -------  (TIMED BUFFER) Will NOT drop elements to maintain max memory (_memorydrop==false in add())\n");
	  }
      }
    cv.notify_all();
    return good;
  }

  bool iszeroed()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return zeroed;
  }

  void reset()
  {
    const std::lock_guard<std::mutex> lock(mu);
    _reset();
  }

  bool _get_spanned_time( TT& begin, TT& end )
  {
    if( buffer.empty() )
      {
	return false;
      }
    else
      {
	begin = _get_first().value().timestamp;
	end = _get_last().value().timestamp;
	return true;
      }
  }
  
  bool get_spanned_time( TT& begin, TT& end )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_spanned_time(begin,end);
  }

  bool _get_spanned_time_zeroed_secs( double& begin, double& end )
  {
    if( buffer.empty() || !zeroed )
      {
	return false;
      }
    else
      {
	//fprintf(stdout, "Timebase %lf\n", tb_hz_sec() );
	begin = _zeroed_secs_from_time( _get_first().value().timestamp );
	end = _zeroed_secs_from_time( _get_last().value().timestamp );
	return true;
      }
  }
  
  //First time will only be zero if you never drop any!! Otherwise you may drop it and your first may diverge from the zero time.
  bool get_spanned_time_zeroed_secs( double& begin, double& end )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_spanned_time_zeroed_secs(begin,end);
  }
    
  //REV: return what? pointer to element? Copy of it?! Yea...must be copy? Whatever. Hope it does not leave scope.
  //REV: may return an empty!? I'd like to know what the error is (nothing inside delta, empty buffer, etc...)
  std::optional<timed_buffer_element<T,TT>> get_timed_element( const TT target_time, const TT maxdelta, const relative_timing reltiming, const bool dropbefore=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_timed_element( target_time, maxdelta, reltiming, dropbefore );
  }

  //REV: returns a *range* of timed buffer elements.
  //Basically, all in range: [min-delta, max+delta]. Drop before drops before the lower bound.
  std::vector<timed_buffer_element<T,TT>> get_timed_element_range( const TT target_time_min, const TT target_time_max, const TT maxdelta, const bool dropbefore=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    //assert( target_time_max > target_time_min );
    return _get_timed_element_range( target_time_min, target_time_max, maxdelta, dropbefore );
  }
  
  std::vector<timed_buffer_element<T,TT>> get_timed_element_range_zeroed_secs( const double target_time_min, const double target_time_max, const double maxdelta, const bool dropbefore=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    //REV: extract equivalent timed element times of zeroed secs...
    auto tfirst = _time_from_zeroed_secs( target_time_min );
    auto tlast = _time_from_zeroed_secs( target_time_max );
    //REV: check for errors in first last?
    return _get_timed_element_range( tfirst, tlast, maxdelta, dropbefore );
  }

  //convenience (no max delta)
  std::optional<timed_buffer_element<T,TT>> get_timed_element( const TT target_time, const relative_timing reltiming, const bool dropbefore=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    const TT maxdelta = std::numeric_limits<TT>::max();
    return _get_timed_element( target_time, maxdelta, reltiming, dropbefore );
  }

  std::optional<timed_buffer_element<T,TT>> get_timed_element_fromzero( const TT target_time_fromzero, const TT maxdelta, const relative_timing reltiming, const bool dropbefore=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !zeroed )
      {
#if DEBUG_LEVEL >0
	fprintf(stderr, "Requested zeroed value, but this timed buffer is not yet zeroed!\n");
#endif
	return std::nullopt;
      }
    const TT target_time = _time_from_zerooffset( target_time_fromzero );
    return _get_timed_element( target_time, maxdelta, reltiming, dropbefore );
  }

  
  //convenience no max delta -- (max delta is size max)
  std::optional<timed_buffer_element<T,TT>> get_timed_element_fromzero( const TT target_time_fromzero, const relative_timing reltiming, const bool dropbefore=false )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !zeroed )
      {
#if DEBUG_LEVEL > 0
	fprintf(stderr, "Requested zeroed value, but this timed buffer is not yet zeroed!\n");
#endif
	return std::nullopt;
      }
    const TT target_time = _time_from_zerooffset( target_time_fromzero );
    const TT maxdelta = std::numeric_limits<TT>::max();
    return _get_timed_element( target_time, maxdelta, reltiming, dropbefore );
  }


  //This uses clock time zero
  std::optional<timed_buffer_element<T,TT>> get_timed_element_fromzero_secs( const double target_time_fromzero_secs, const double maxdelta_secs, const relative_timing reltiming, const bool dropbefore=false, double* startsec=nullptr, double* endsec=nullptr, bool* ok=nullptr )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !zeroed )
      {
#if DEBUG_LEVEL>0
	fprintf(stderr, "Requested zeroed value, but this timed buffer is not yet zeroed!\n");
#endif
	return std::nullopt;
      }
    
    
    const TT target_time = _time_from_clock_secs( target_time_fromzero_secs );
    
    const TT maxdelta = _seconds_in_time( maxdelta_secs );
    //std::fprintf(stdout, "REQUEST: [%ld] in  [%ld] - [%ld] (maxdelta %ld)\n", target_time, _get_first().value().timestamp, _get_last().value().timestamp, maxdelta );

    auto myelem = _get_timed_element( target_time, maxdelta, reltiming, dropbefore );
    //#if DEBUG_LEVEL > 0
    if( startsec && endsec && ok )
      {
	double d1, d2;
	bool ok2 = _get_spanned_time_zeroed_secs(d1,d2);
	if( ok2 ) //REV: need to return here!!!! if it's not zeroed I should ignore this shit!
	  {
	    const size_t MISSED_REQUESTS_SKIP = 20;
	    if( nmissedrequests % MISSED_REQUESTS_SKIP == 0 )
	      {
		fprintf(stdout, "Requested [%5.2lf] secs    (spanning: [%5.2lf] - [%5.2lf]) (Note: showing 1/%lu requests)\n", target_time_fromzero_secs, d1, d2, MISSED_REQUESTS_SKIP);
		nmissedrequests=0;
	      }
	    nmissedrequests++;
		
	    *startsec = d1;
	    *endsec = d2;
	    *ok = ok2;
	  }
	    
      }
    //#endif
    return myelem; 
  }
  
  
  //positive time will make my zero correspond to a higher
  // clock time (an identical clock time request will now return
  // further back in the past)
  //negative time will make my zero correspond to lower (earlier)
  // clock time (an identical clock time request will now
  // return further in the future)
  void adjust_zerotime_secs( const double sectoadjust )
  {
    const std::lock_guard<std::mutex> lock(mu);
    zero_time_clocksec += sectoadjust;
    return;
  }
  
  
  std::optional<TT> get_zerotime()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( zeroed )
      {
	return zero_time;
      }
    else
      {
	return std::nullopt;
      }
  }

  
  
  
  //REV: it will return a "closest" to a seconds query, but
  //not the seconds time unless I call "in seconds" again.
  void set_zerotime_clock_sec( const double clocktime_sec )
  {
    const std::lock_guard<std::mutex> lock(mu);
    zero_time_clocksec = clocktime_sec;
    return;
  }

  //Only return null if zeroed? Oh well.
  std::optional<double> clocktime_sec_of_timestamp( const TT ts )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !zeroed )
      {
	return std::nullopt;
      }
    return _clock_secs_from_time( ts );
  }
  

  bool zero_from_last_if_unzeroed( const double clockzerosec=0 )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !zeroed && !buffer.empty() )
      {
	_rezero_time( _get_last().value().timestamp );
	zero_time_clocksec = clockzerosec;
	return true;
      }
    return false;
  }

  bool zero_from_first_if_unzeroed( const double clockzerosec=0 )
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !zeroed && !buffer.empty() )
      {
	_rezero_time( _get_first().value().timestamp );
	zero_time_clocksec = clockzerosec;
	return true;
      }
    return false;
  }

  double get_zerotime_clocksec()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return zero_time_clocksec;
  }

  //REV: need a way to get by index....but things may be inserted/deleted asynchronously while I am accessing.
  //REV: better to "Freeze" it...
  std::deque<timed_buffer_element<T,TT>> freeze_copy()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return buffer;
  }

  double time_in_seconds( const TT& ts )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _time_in_seconds(ts);
  }

  uint64_t get_size_bytes()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_size_bytes();
  }

  void set_maxsize_bytes( const uint64_t newsz )
  {
    const std::lock_guard<std::mutex> lock(mu);
    maxsizebytes=newsz;
  }
  
};  //end struct timed_buffer
