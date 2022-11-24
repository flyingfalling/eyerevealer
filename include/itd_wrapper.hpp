// System libs
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <iostream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <fstream>
#include <any>
#include <optional>

// Open CV
#include <opencv2/opencv.hpp>

// ISC (ITD camera) lib
#include <ISCSDKLib.h>

// My libs
#include <Timer.hpp>

//REV: define this if you want to do full AWB/Alignment with the depth image of the RGB image.
//#define FULL_PROCESSING

#define MYMETHOD

const int flipoverx = 0;
const int flipovery = 1;



enum ITD_GRAB_MODE
  {
    DISPARITY=2, //REV: disparity mode
    ALIGNED=3,   //REV: aligned mode
    UNALIGNED=4  //REV: unaligned mode
  };

template <typename T>
struct nvector
{
  T* myptr=nullptr;
  size_t mysize=0;
  bool autofree=false;

  T* buptr=nullptr;
  
  nvector( const bool doautofree=false )
  {
    autofree = doautofree;
  }

  ~nvector()
  {
    if( myptr || buptr )
      {
	if( myptr != buptr )
	  {
	    fprintf(stderr, "LOL wtf double free on nvector?\n");
	  }
      }
    
    if( autofree && myptr )
      {
	delete[] myptr;
	mysize=0;
      }
  }
  
  T* data() const
  {
    return myptr;
  }
  
  void resize( const size_t newsize )
  {
    if( myptr )
      {
	delete[] myptr;
	mysize=0;
      }

    myptr = new T[newsize];
    mysize = newsize;

    buptr = myptr; //point to same memory target
    
    return;
  }

  const size_t size() const
  {
    return mysize;
  }
  
};





//REV: User should be able to decide which ones I have, don't
//Try to get illegally?
//Contained ones will depend on settings of the ITDLib

//REV: I will "fill" this.
struct itd_frameset
{
  itdtime_t timestamp;
  bool converted=false;
  
  cv::Mat matL;
  cv::Mat matR;
  cv::Mat matDisparity;
  
  cv::Mat matYUV;
  cv::Mat matBGR;
  
  cv::Mat matDepth;

  itd_frameset( const itdtime_t ts )
    : timestamp(ts), converted(false)
  {
  }


  void flip_and_convert()
  {
    if( converted )
      {
	return;
      }

#ifdef MYMETHOD
    yuv_to_bgr_flipy();
#endif
    
    flipallx();

#ifdef MYMETHOD
    awb();
#endif
    //REV: all images should only show the "overlapping" part, which is the right 4/5 of the image
    // (without doing their Correct_Image_RGB shit?)
    //
    
    converted = true;
  }

  void flipallx()
  {
    //REV: I also remove the non-overlapping part of the image (the left 4/5 of the image)
    //Before I flip over X, that corresponds to the right 1/5 of the image.
    
        
    
    //REV: Left mat needs what part cropped? Impossible to bring into line...(will have blind area)
    //matL = matL( ycolrange, xrowrange );
    
    cv::flip( matBGR, matBGR, flipoverx );
    cv::flip( matDisparity, matDisparity, flipoverx );
    cv::flip( matR, matR, flipoverx );
    cv::flip( matL, matL, flipoverx );
    cv::flip( matDepth, matDepth, flipoverx );

    auto ycolrange = cv::Range( 0, matBGR.size().height );
    //auto xrowrange = cv::Range( 0, (matBGR.size().width*4)/5 );
    auto xrowrange = cv::Range( matBGR.size().width/5, matBGR.size().width );
    //REV: fuck could this make it non-contiguous? I need to clone?
    
    matBGR = matBGR( ycolrange, xrowrange );
    matDisparity = matDisparity( ycolrange, xrowrange );
    matR = matR( ycolrange, xrowrange );
    matDepth = matDepth( ycolrange, xrowrange );

    double mi, ma;
    cv::minMaxLoc( matDepth, &mi, &ma );
    //fprintf(stdout, "min/max: %lf %lf\n", mi, ma );
    if( mi < 0 || ma >= 256 )
      {
	fprintf(stderr, "ITD output depth mat outside expected [0,256) range?! [%lf, %lf]\n", mi, ma );
	exit(1);
      }
    
    //REV: make depth uint16
    double scale = 256.0; //Because ITD camera outputs floats in range (0,255), and uint16 holds 256*256, so...
    double uint16max = 256*256-1;
    if( ma * scale > uint16max )
      {
	fprintf(stdout, "Scaled will be outside UINT16 range?! (%lf), should be <(%lf)\n", ma*scale, uint16max);
	exit(1);
      }
    
    //cv::imshow( "ITD Depth", matDepth/255 );
    //cv::waitKey(1);
    
    matDepth.convertTo( matDepth, CV_16UC1, scale );
    //Downsample (decimate?) nearest?

  }
  
  void yuv_to_bgr_flipy()
  {
    cv::cvtColor(matYUV, matBGR, cv::COLOR_YUV2RGB_YUY2 );
    cv::flip( matBGR, matBGR, flipovery );
    
  }

  void awb()
  {
    //split into 3 channels
    //std::vector< cv::Mat > grayPlanes(3);
    //cv::split( matBGR, grayPlanes );
    
    //average each channel
    //double avgs[3];
    std::vector<double> gains(3);
    
    /*for( size_t x=0; x<3; ++x )
      {
	avgs[x] = cv::mean( grayPlanes[x] ) [ 0 ];
	}*/
    auto avgs = cv::mean( matBGR );
    for( size_t x=0; x<3; ++x )
      {
	gains[x] = (avgs[0]+avgs[1]+avgs[2])/avgs[x];
      }
    auto sgains( gains );
    
    //calculate channel gain: (sum 3 chan avgs) / 3*myavg
    //redistribute: pixel * mygain
    cv::multiply( matBGR, sgains, matBGR );
  }


  std::optional<cv::Mat> get_bgr()
  {
    if(!converted) { return std::nullopt; }
    if(matBGR.empty()) { return std::nullopt; }
    return matBGR;
  }

  std::optional<cv::Mat> get_depth()
  {
    if(!converted) { return std::nullopt; }
    if(matDepth.empty()) { return std::nullopt; }
    return matDepth;
  }

  std::optional<cv::Mat> get_disparity()
  {
    if(!converted) { return std::nullopt; }
    if(matDisparity.empty()) { return std::nullopt; }
    return matDisparity;
  }

  std::optional<cv::Mat> get_L()
  {
    if(!converted) { return std::nullopt; }
    if(matL.empty()) { return std::nullopt; }
    return matL;
  }

  std::optional<cv::Mat> get_R()
  {
    if(!converted) { return std::nullopt; }
    if(matR.empty()) { return std::nullopt; }
    return matR;
  }
  

};



struct ITD_wrapper
{
private:

  typedef float float32_t;
  
    
  uint64_t frameidx;
  bool matsfilled;
  bool buffersfilled;
    
  bool iscolor;
  int fps;
  
  ITD_GRAB_MODE grabmode;
  
  uint32_t imgwid, imghei;
  uint32_t depwid, dephei;
  uint32_t depstartx;//=imgwid/5;
  uint32_t dependx;//=imgwid;
  uint32_t depstarty;//=0;
  uint32_t dependy;//=imghei;
  uint32_t frameskipmode;
  
  cv::Mat matL;
  cv::Mat matR;
  cv::Mat matDisparity;
  cv::Mat matBGR;
  cv::Mat matDepth;
  
  cv::Mat matYUV;
  
  //REV: either raw left/right B/W images (mode=3,mode=4) **OR** (mode=2) left B/W image and disparity **IMAGE** (not depth!)
  nvector<uint8_t> imgbufL;
  nvector<uint8_t> imgbufR; //REV: would contains disparity if mode=2, right B/W otherwise
  nvector<uint8_t> imgbufDisparity; //will pass this instead
  
  //REV: *depth* (e.g. in meters)
  nvector<float32_t> imgbufDep;
  
  nvector<uint8_t> imgbufYUV;
  nvector<uint8_t> imgbufRGB;
  nvector<uint8_t> imgbufAWB;
  nvector<uint8_t> imgbufAligned;
  
  std::shared_ptr<ISCSDKLib> lib;//=nullptr; //if not nullptr, it is open
  ISCSDKLib::CameraParamInfo camera_params;
  bool isconnected;
  
  Timer timestamp_timer;
  double last_timestamp=0;
  const double timebase_hz_sec=1.0;
  
  std::mutex mu;
  
private:

  void _unload_module()
  {
    const std::string modname = "ftdi_sio";
    bool detected = _kernel_module_loaded( modname );
    if(detected)
      {
	fprintf(stdout, "Module [%s] detected, attempting to remove...\n");
	std::string cmd = "sudo rmmod " + modname; //REV: add this string to allowed userspace calls of this user...
	std::system( cmd.c_str() );
      }
  }

  bool _kernel_module_loaded( const std::string& modname )
  {
    int detect=0;
    std::vector<char> line(512);
    
    
    FILE* pf=fopen("/proc/modules", "rt");
    //fgets reads up to N chars, stops if newline is detected. Null pointer if failure, char* to line if successful
    while( std::fgets(reinterpret_cast<char*>(line.data()), line.size(), pf) ) //Read all existing module, compare to "ftdio_sio", the module we want to unload
      {
	if(0 == std::strncmp(line.data(), modname.c_str(), modname.size()))
	  {
	    detect=1;
	    break;	 //found driver
	  }
      }
    std::fclose(pf);
    if(detect)
      {
	return true;
      }
    return false;
  }

  void _reinit()
  {
    imgwid=0;
    imghei=0;
    
    isconnected=false;
    
    frameidx=0;
    matsfilled=false;
    buffersfilled=false;
  }
  
  
  
  bool _disconnect()
  {
    _reinit();
    
    try
      {
	std::fprintf(stdout, "ITD _disconnect...\n");
	//lib means connected...
	if( lib )
	  {
	    int state;
	    fprintf(stdout, "ITD: _disconnect -- was open so Will stop grab (disconnect)\n");
	    state = lib->StopGrab();
	    if( state != 0 )
	      {
		fprintf(stderr, "ITD: STOP GRAB: failed with error [%d (disconnect: ret false)]\n", state);
		return false;
	      }
	    fprintf(stdout, "ITD: was open FINISHED stop grab (disconnect)\n");
	  }
	else
	  {
	    fprintf(stdout, "ITD: _disconnect was not open?\n");
	  }
      }
    catch( const std::exception& e )
      {
	std::cerr << e.what() << std::endl;
	return false;
      }
    catch( ... )
      {
	fprintf(stderr, "REV: ITD: caught something!\n");
	return false;
      }
    return true; //true even if not already open.
  }

  
  bool _is_connected()
  {
    return isconnected;
  }

  
  
  bool _close()
  {
    _reinit();
    
    try
      {
	if( lib )
	  {
	    _disconnect();
	    fprintf(stdout, "Calling CLOSEISC()\n");
	    int state = lib->CloseISC();
	    fprintf(stdout, "FINISHED CLOSEISC()\n");

	    lib.reset(); //Fuck it, just delete it and hope no memory leak?
	    _unload_module();
	    
	    if( state != 0 )
	      {
		fprintf(stderr, "ITD: Close ISC: failed with error [%d] (we still released the shptr though...memleak?)\n", state);
		return false;
	      }
	  }
		
      }
    catch( const std::exception& e )
      {
	std::cerr << e.what() << std::endl;
	return false;
      }
    catch( ... )
      {
	fprintf(stderr, "REV: ITD: caught something!\n");
	return false;
      }
    
    return true;
  }
	
 

  //REV: need a clever way to detect that it has been disconnected...
  bool _is_open()
  {
    //REV: check some other way that it is fucked up?
    if( !lib )
      {
	return false;
      }

    /*
    try {
      //int autocalibmode;
    //int state = lib->GetAutoCalibration(&autocalibmode);
      uint32_t noisefilt;
      int state = lib->GetNoiseFilter(&noisefilt);
      //uint32_t w,h;
      // state = lib->GetImageSize(&w,&h);
       //check w, h?
    if( state != 0 )
      {
	fprintf(stdout, "GetAutoCalib failed with [%d]\n", state);
	return false;
      }
    else
      {
	fprintf(stdout, "GetAutoCalib succeeded?! with [%d]\n", state);
      }
    }
    catch( std::exception& e )
      {
	std::cerr << e.what() << std::endl;
	return false;
      }
    catch( ... )
      {
	fprintf(stderr, "REV: ITD: caught something!\n");
	return false;
      }
//REV: NOTHING triggers failure of these if I disconnect :(
    */
    
    return true;

    //try something that will not break anything I am doing?
    
  }
  
  bool _open( const bool closefail=true )
  {
    try
      {
	if( !lib )
	  {
	    lib = std::make_shared<ISCSDKLib>();
	  }
	fprintf(stdout, "Calling OPENISC()\n");
	int state = lib->OpenISC();
	fprintf(stdout, "RETURNED OPENISC()\n");
	
	if( state != 0 )
	  {
	    fprintf(stderr, " ITD ERROR --- ITD: OPEN ITC err [%d] (-101 is already open, -4 not ready yet?)\n", state );
	    /*while( state == -4 )
	      {
		fprintf(stdout, "Calling (again) OPENISC on -4 after sleep\n");
		std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
		_close();
		lib = std::make_shared<ISCSDKLib>();
		state = lib->OpenISC();
		fprintf(stdout, "Finished (again) OPENISC (%d)\n", state);
		}*/
	    if( state != -101 )
	      {
		if( closefail ) { bool closed = _close(); }
		return false;
	      }
	  }

	fprintf(stdout, "ITD SUCCESS -- Opened ITC (got 0 return)\n");
	
      } //end try
    catch( const std::exception& e )
      {
	std::cerr << e.what() << std::endl;
	return false;
      }
    catch( ... )
      {
	fprintf(stderr, "REV: ITD: caught something!\n");
	return false;
      }

    
    return true;
  }
  

  

  
  bool _connect( const int whatfps, const bool getcolor=true )
  {
    _reinit();
    if( !_is_open() ) //isopen
      {
	fprintf(stderr, " ITD: can't connect without open()\n");
	return false;
      }
    
    if( _is_connected() )
      {
	_disconnect(); //will reconnect, maybe different params...
      }

    fps = whatfps;
    iscolor = getcolor;
        
    //REV: this will not change depending on state...?!
    //REV: this will work before call to startGrab!??!?!
    int state = lib->GetImageSize(&imgwid,&imghei);
    if( state != 0 )
      {
	fprintf(stderr, "Error [%d] in GetImageSize\n", state );
	return false;
      }
    fprintf(stdout, "Got image size: [%d] [%d]\n", imgwid, imghei);
    
    ///////////// CALCULATE GHETTO IMAGE ALIGNMENT?!
#ifdef ALIGNDEPTH
    depstartx=imgwid/5;
    dependx=imgwid;
    depstarty=0;
    dependy=imghei;
    
    depwid = dependx - depstartx;
    dephei = dependy - depstarty;
#else
    depwid = imgwid;
    dephei = imghei;
#endif
    fprintf(stdout, "FPS: %d\n", fps);
    ///////////// CALCULATE FRAMESKIPMODE
    frameskipmode = (60/fps)-1; //0 for 60, 1 for 30...
    if( false == ( frameskipmode == 0  || frameskipmode == 1 ) )
      {
	fprintf(stderr, "Unsupported ITD frames to skip (i.e. FPS) [%d]\n", frameskipmode );
	return false;
      }
    
    ///////////// SET COLOR MODE
    int rgbmode = (true == iscolor) ? 1 : 0;
    state = lib->Set_RGB_Enabled( rgbmode );
    if( state != 0 )
      {
	fprintf(stderr, "Set RGB err [%d]\n", state );
	return false;
      }
    fprintf(stdout, "Set RGB Mode [%d]\n", rgbmode);
    
        
    ////////////  START GRABBING (with MODE)
    grabmode = ITD_GRAB_MODE::DISPARITY;
    if( iscolor )
      {
	grabmode = ITD_GRAB_MODE::DISPARITY;
	std::fprintf(stdout, "Forcefully setting DISPARITY mode (since you requested color images)?!\n");
      }
    state = lib->StartGrab( grabmode );
    
    
    if( state != 0 )
      {
	fprintf(stderr, "Error [%d] in StartGrab\n", state );
	return false;
      }
    
    fprintf(stdout, "Started Grab (mode [%d])\n", grabmode);
    
    
    /////////// ALLOCATE MATS and BUFFERS
    matL = cv::Mat( cv::Size( imgwid, imghei ), CV_8UC1 );
    matR = cv::Mat( cv::Size( imgwid, imghei ), CV_8UC1 );
    
    //Disparity image //What is correct image size? original? Or less sized?
    //REV: actually copied from matL?
    matDisparity = cv::Mat( cv::Size( depwid, dephei ), CV_8UC1 ); //only reason to separate from matR is size difference?
    matDepth = cv::Mat( cv::Size( depwid, dephei ), CV_32F );
    
    imgbufL.resize(imgwid*imghei);
    imgbufR.resize(imgwid*imghei);
    imgbufDisparity.resize(depwid*dephei); //uint8_t
    imgbufDep.resize(depwid*dephei); //should be float...?
    
    if( iscolor )
      {
	imgbufYUV.resize( imgwid*imghei*2);
	imgbufRGB.resize( imgwid*imghei*3);
	imgbufAWB.resize( imgwid*imghei*3);
	imgbufAligned.resize( imgwid*imghei*3);
	matBGR = cv::Mat( cv::Size( imgwid, imghei ), CV_8UC3 );
	matYUV = cv::Mat( cv::Size( imgwid, imghei ), CV_8UC2 ); //CV_8UC2 );
      }

    timestamp_timer.reset();
    
    isconnected = true;

    fprintf(stdout, "Finished ITD connect\n");
    return true;
  } //end _connect

  //Fills buffers from camera
  bool _fillbuffers()
  {
    if( !isconnected )
      {
	fprintf(stderr, " ITD fillbuffers can't work without connect() (after open() first!)\n");
	return false;
      }
    
    matsfilled = false;
    buffersfilled = false;
    
    
    int state;
    
    /////////////   GET B/W IMAGES AND DEPTH INFO
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
      {
	state = lib->GetImage( imgbufDisparity.data(), imgbufR.data(), frameskipmode );
      }
    else
      {
	state = lib->GetImage( imgbufL.data(), imgbufR.data(), frameskipmode );
      }
    
    if( state != 0 )
      {
#if DEBUG_LEVEL>0
	fprintf(stderr, "REV: err grabimage [%d]\n", state );
#endif
	return false;
      }

#define STRICT_ITD //REV: OFF -> "illegally" get depth even if I request aligned/unaligned B/W images (mode 3 or 4)
#ifdef STRICT_ITD
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
#endif
      {
	state = lib->GetDepthInfo( imgbufDep.data() );
	if( state != 0 )
	  {
	    fprintf(stderr, "REV: err get Depth Info [%d]\n", state );
	    return false;
	  }
      }
    
    //////////////   GET COLOR IMAGES
    if( iscolor )
      {
	state = lib->Get_YUV_Image( imgbufYUV.data(), frameskipmode );
	if( state != 0 )
	  {
#if DEBUG_LEVEL>0
	    fprintf(stderr, "REV: Failed to get COLOR (YUV) data [%d]\n", state );
#endif
	    return false;
	  }
	
#ifdef FULL_PROCESSING
	lib->YUV_TO_RGB( imgbufYUV.data(), imgbufRGB.data(), imgbufRGB.size() );  // ~15 msec WTF?!?!?
	lib->RGB_TO_AWB( imgbufRGB.data(), imgbufAWB.data() ); // ~ 8 msec
	lib->Correct_RGB_Image( imgbufAWB.data(), imgbufAligned.data() );  // ~25 msec
#endif
      }

    //REV: update timestamp!
    last_timestamp = timestamp_timer.elapsed();
    buffersfilled=true;
    
    return true;
  }

  std::string _get_serial()
  {
    /*
float	fD_INF;
unsigned int nD_INF;
float fBF;
float fBaseLength;
float fViewAngle;
unsigned int nImageWidth;
unsigned int nImageHeight;
unsigned int nProductNumber;
unsigned int nSerialNumber;
unsigned int nFPGA_version;
unsigned int nDistanceHistValue;
unsigned int  nParallaxThreshold;
     */
    if( !lib )
      {
	return {};
      }
    ISCSDKLib::CameraParamInfo params;
    int state = lib->GetCameraParamInfo(&params);
    if( state != 0 )
      {
	fprintf(stderr, "Error in get params [%d]\n", state );
      }
    std::stringstream ss;
    ss << params.nProductNumber << "-" << params.nSerialNumber; //nFPGA_version
    return ss.str();
  }
  
  //REV: Fills external mats from buffers (only call after successfully _fillbuffers())
  std::optional<itd_frameset> _fillmats_external( )
  {
    if( !isconnected )
      {
	fprintf(stderr, " ITD fillbuffers (external) can't work without connect() (after open() first!)\n");
	return std::nullopt;
      }
    
    if( !buffersfilled )
      {
	return std::nullopt;
      }

    //REV: I never set these again (I may mess up the state)
    buffersfilled = false;
    matsfilled = false;
    
    
    //REV: write last update time?
    itd_frameset itdfs( last_timestamp );
    
    //Raw (left/right b&w or left b&w and disparity image)
    //uint8_t
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
      {
	std::memcpy(matDisparity.data, imgbufDisparity.data(), imgbufDisparity.size()*sizeof(uint8_t));
	std::memcpy(matR.data, imgbufR.data(), imgbufR.size()*sizeof(uint8_t));

	itdfs.matDisparity = matDisparity.clone();
	itdfs.matR = matR.clone();
	
	//cv::flip( matDisparity, matDisparity, flipoverx );
	//cv::flip( matR, matR, flipoverx );
      }
    else
      {
	std::memcpy(matL.data, imgbufL.data(), imgbufL.size()*sizeof(uint8_t));
	std::memcpy(matR.data, imgbufR.data(), imgbufR.size()*sizeof(uint8_t));

	itdfs.matL = matL.clone();
	itdfs.matR = matR.clone(); //REV: clone so I own the data!
	
	//cv::flip( matL, matL, flipoverx );
	//cv::flip( matR, matR, flipoverx );
      }
    
    // depth data (float32)
#ifdef STRICT_ITD
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
#endif
      {
	std::memcpy(matDepth.data, imgbufDep.data(), imgbufDep.size()*sizeof(float32_t));

	itdfs.matDepth = matDepth.clone(); //clone so I own the data!
	//cv::flip( matDepth, matDepth, flipoverx );
      }
    
    if( iscolor )
      {
	//REV: fuck it just do it myself their code is so slow...
#ifdef MYMETHOD
	matYUV = cv::Mat( matYUV.size().height, matYUV.size().width, CV_8UC2, imgbufYUV.data(), matYUV.size().width*2 );
	itdfs.matYUV = matYUV.clone(); //REV: clone so that I own the data!
#else //ITD METHOD
	lib->YUV_TO_RGB( imgbufYUV.data(), imgbufRGB.data(), imgbufRGB.size() );
	lib->RGB_TO_AWB( imgbufRGB.data(), imgbufAWB.data() ); // ~ 8 msec
	//std::memcpy( matBGR.data, imgbufRGB.data(), imgbufRGB.size()*sizeof(uint8_t));
	std::memcpy( matBGR.data, imgbufAWB.data(), imgbufAWB.size()*sizeof(uint8_t));
	itdfs.matBGR = matBGR.clone(); //clone so that I own the data!
#endif
	
	
	//cv::cvtColor(matYUV, matBGR, cv::COLOR_YUV2RGB_YUY2 );
	//cv::flip( matBGR, matBGR, flipoverx );
	//cv::flip( matBGR, matBGR, flipovery );
      }
        
    return itdfs;
    
  } //fillmats_external
  
  //REV: Fills mats from buffers (only call after successfully _fillbuffers())
  bool _fillmats()
  {
    if( !isconnected )
      {
	fprintf(stderr, " ITD fillmats can't work without connect() (after open() first!)\n");
	return false;
      }
    
    if( !buffersfilled )
      {
	return false;
      }
    
    //Even better -- mux it?
    matsfilled = false;
    buffersfilled = false;

    //Raw (left/right b&w or left b&w and disparity image)
    //uint8_t
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
      {
	std::memcpy(matDisparity.data, imgbufDisparity.data(), imgbufDisparity.size()*sizeof(uint8_t));
	std::memcpy(matL.data, imgbufR.data(), imgbufR.size()*sizeof(uint8_t));

	cv::flip( matDisparity, matDisparity, flipoverx );
	cv::flip( matR, matR, flipoverx );
      }
    else
      {
	std::memcpy(matL.data, imgbufL.data(), imgbufL.size()*sizeof(uint8_t));
	std::memcpy(matR.data, imgbufR.data(), imgbufR.size()*sizeof(uint8_t));
	
	cv::flip( matL, matL, flipoverx );
	cv::flip( matR, matR, flipoverx );
      }
    
    // depth data (float32)
#ifdef STRICT_ITD
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
#endif
      {
	std::memcpy(matDepth.data, imgbufDep.data(), imgbufDep.size()*sizeof(float32_t));
	cv::flip( matDepth, matDepth, flipoverx );
      }
    
    if( iscolor )
      {
#ifdef FULL_PROCESSING
	std::memcpy(matBGR.data, imgbufAligned.data(), imgbufAligned.size()*sizeof(uint8_t));
	cv::flip( matCol, matCol, flipoverx );
	//REV: no need RGB->BGR because flip took care of it lol
#else
	//REV: fuck it just do it myself their code is so slow...
	//std::memcpy(matYUV.data, imgbufYUV.data(), imgbufYUV.size()*sizeof(uint8_t));
	//bytes_to_YUV( imgbufYUV, matBGR );
	matYUV = cv::Mat( matYUV.size().height, matYUV.size().width, CV_8UC2, imgbufYUV.data(), matYUV.size().width*2 );
	cv::cvtColor(matYUV, matBGR, cv::COLOR_YUV2RGB_YUY2 );
	cv::flip( matBGR, matBGR, flipoverx );
	cv::flip( matBGR, matBGR, flipovery );
#endif
	
      }
    
    matsfilled = true;
    
    return true;
  }

  
  
public:

  bool close()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _close();
  }
  
  void disconnect()
  {
    const std::lock_guard<std::mutex> lock(mu);
    _disconnect();
  }
  
  bool open( const bool closefail=true )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _open(closefail);
  }
  bool connect( const int whatfps, const bool getcolor )
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _connect(whatfps, getcolor);
  }

  bool is_open()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _is_open(); //_is_open();
  }

  std::string get_serial()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _get_serial();
  }
  
  bool is_connected()
  {
    const std::lock_guard<std::mutex> lock(mu);
    return _is_connected();
  }
  
  void trycorrect()
  {
    lib->Correct_RGB_Image( imgbufAWB.data(), imgbufAligned.data() );
  }
  
  ITD_wrapper()
  {
    _reinit();
    lib.reset();
    return;
  }

  ~ITD_wrapper()
  {
    const std::lock_guard<std::mutex> lock(mu);
    _close();
    return;
  }


  /*
  bool connect(  const int whatfps, const bool getcolor, const double connect_timeo_sec=10.0 )
  {
    bool success = false;
    Timer t;
    while( !success && t.elapsed() < connect_timeo_sec )
      {
	success = _connect( whatfps, getcolor );

	const uint64_t sleeptime_msec = 1000;
	std::this_thread::sleep_for( std::chrono::milliseconds( sleeptime_msec ) );
      }
    fprintf(stdout, "ITD failed to connect after timeout [%lf] sec\n", connect_timeo_sec );
    
    return success;
    }*/
  
  //REV: TODO -- make this use loopcond (so it will not wait 1 sec if it dc and loop ended)
  //Only return when I got a new image?
  bool fill( const double timeout_sec = 5.0 )
  {
    bool filled = false;
    Timer t;
    const uint64_t sleeptime_msec = 2;
    while( !filled && t.elapsed() < timeout_sec )
      {
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  filled = _fillbuffers();
	}
	if( !filled )
	  {
	    std::this_thread::sleep_for( std::chrono::milliseconds( sleeptime_msec ) );
	  }
      }
    
    if( filled )
      {
	//Timer t;
	const std::lock_guard<std::mutex> lock(mu);
	_fillmats();
	//fprintf(stdout, "FillMats: %lf\n", t.elapsed()*1e3);
      }
    
    return filled; //false means timed out, true means OK
  }


  
  std::optional<itd_frameset> fill_external( const double timeout_sec = 5.0 )
  {
    Timer t;
    bool filled=false;
    const uint64_t sleeptime_msec = 2; //REV: would rather have a callback from them :(
    while( !filled && t.elapsed() < timeout_sec )
      {
	{
	  const std::lock_guard<std::mutex> lock(mu);
	  filled = _fillbuffers();
	}
	if( !filled )
	  {
	    std::this_thread::sleep_for( std::chrono::milliseconds( sleeptime_msec ) );
	  }
      }
    
    if( filled )
      {
	const std::lock_guard<std::mutex> lock(mu);
	return _fillmats_external();
      }
    else
      {
	return std::nullopt;
      }
  }
  
  std::optional<cv::Mat> get_depth()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !matsfilled ) { return std::nullopt; }
#ifdef STRICT_ITD
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
#endif
      {
	return matDepth;
      }
    return std::nullopt;
  }

  std::optional<cv::Mat> get_col()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !matsfilled ) { return std::nullopt; }
    if( iscolor )
      {
	return matBGR;
      }
    return std::nullopt;
  }

  std::optional<cv::Mat> get_L()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !matsfilled ) { return std::nullopt; }
    if( grabmode == ITD_GRAB_MODE::DISPARITY )
      { return std::nullopt; }
    return matL;
  }

  std::optional<cv::Mat> get_R()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !matsfilled )
      { return std::nullopt; }
    return matR;
  }

  std::optional<cv::Mat> get_Disparity()
  {
    const std::lock_guard<std::mutex> lock(mu);
    if( !matsfilled )
      { return std::nullopt; }
    if( grabmode != ITD_GRAB_MODE::DISPARITY )
      { return std::nullopt; }
    return matDisparity;
  }
    
};
