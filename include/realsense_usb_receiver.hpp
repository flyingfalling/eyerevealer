#pragma once

struct realsense_usb_receiver
  : public device_stream_publisher<rs2::frame>
{
protected:
  std::thread mythread;
  rs2::pipeline_profile pipe_profile;
  rs2::pipeline pipe;
  rs2::config cfg;
  std::string serial;
  bool laseron;

  
public:
  realsense_usb_receiver( const std::string& sn, const bool withlaser=false, const size_t mbufsize=0 )
    : device_stream_publisher(mbufsize), serial(sn), laseron(withlaser)
  {
    if (!serial.empty())
      {
	cfg.enable_device(serial);
      }
  }
  
  ~realsense_usb_receiver()
  {
    stop();
  }
  
  void start( loopcond& loop )
  {
    if( serial.empty() )
      {
	fprintf(stderr, "ERROR: realsense usb receiver -- Attempting to start without serial set..\n");
	exit(1);
      }
    mythread = std::thread( &realsense_usb_receiver::doloop, this, std::ref(loop)); //thread is in background via callbacks.
  }

  void stop()
  {
    fprintf(stdout, "IN: STOP: realsense_usb_receiver\n");
    localloop.stop();
    JOIN( mythread );
    fprintf(stdout, "OUT: STOP: realsense_usb_receiver\n");
  }
  
  void doloop( loopcond& loop )
  {
    //REV: make context etc....?
    //Create one "from" something, like serial
    //cfg.enable_stream(RS2_STREAM_DEPTH); //, wid, hei, RS2_FORMAT_ANY, fps ); //RS2_FORMAT_RGB8, 30);
    //cfg.enable_stream(RS2_STREAM_COLOR); //, wid, hei, RS2_FORMAT_ANY, fps );
    //cfg.enable_stream(RS2_STREAM_INFRARED, 1);
    //cfg.enable_stream(RS2_STREAM_INFRARED, 2);
    cfg.enable_all_streams();

    //REV: estimate size of rs2::frame
    //REV: can't take reference to rs2::frame?
    pipe_profile = pipe.start(cfg,
			   [&](rs2::frame frame)
			   {
			     frame.keep();
			     mbuf.addone( frame, rsframesize(frame) );
			   }
			   );
    
    std::fprintf(stdout, "REV: starting to stream realsense into mutexed buffer!\n");
    
    auto depth_stream = pipe_profile.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
    auto color_stream = pipe_profile.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();

    int depfps = depth_stream.fps();
    int colfps = color_stream.fps();

    params["depth_fps"] = depfps;
    params["color_fps"] = colfps;
    
    auto di = depth_stream.get_intrinsics();
    auto ci = color_stream.get_intrinsics();
    
    float dfov[2], cfov[2];
    rs2_fov(&di, dfov);
    rs2_fov(&ci, cfov);
    
    params["depth_fov"] = dfov;
    params["color_fov"] = cfov;
    
    std::fprintf(stdout, "Depth FOV: %f %f\nColorFOV: %f %f\n", dfov[0], dfov[1], cfov[0], cfov[1]);
    
    auto sensor = pipe_profile.get_device().first<rs2::depth_sensor>();
    auto dscale =  sensor.get_depth_scale();
    std::cout << "Depth scale: " << dscale << std::endl;
    
    params["depth_scale_m"] = dscale;
  
    //baseline is distance physical (extrinsic) between the two stereo cameras?
    auto ir1_stream = pipe_profile.get_stream(RS2_STREAM_INFRARED, 1);
    auto ir2_stream = pipe_profile.get_stream(RS2_STREAM_INFRARED, 2);
    
    int i1fps = ir1_stream.fps();
    int i2fps = ir2_stream.fps();
    
    params["infraL_fps"] = i1fps;
    params["infraR_fps"] = i2fps;
    
    rs2_extrinsics e = ir1_stream.get_extrinsics_to(ir2_stream);
    auto baseline = e.translation[0];
    
    params["infra_baseline"] = baseline;
    //params["infra_intrinsics"] = 
    
    
    std::cout << "Disparity Baseline: " << baseline << std::endl;
    
    //REV: get models
    auto i = depth_stream.get_intrinsics();
    auto principal_point = std::make_pair(i.ppx, i.ppy);
    auto focal_length = std::make_pair(i.fx, i.fy);
    rs2_distortion model = i.model;
    
    std::cout << "Principal point: " << principal_point.first << " " << principal_point.second << std::endl;
    std::cout << "Focal length: " << focal_length.first << " " << focal_length.second << std::endl;
    std::cout << "Distortion enum: " << model << std::endl;
    std::cout << "Distortion coeffs: " << i.coeffs[0];
    
    //REV: apparently for d400 cameras, distortion coeffs are all 0 on purpose.
    for( int x=1; x<5; ++x ) { std::cout << ", " << i.coeffs[x]; }
    std::cout << std::endl;
    
    //auto firstmot = pipe_profile.get_device().first<rs2::motion_sensor>();
    //Note, also have FISHEYE (what is different from RGB? :()
    
    bool hasmot = false;
    for( auto& s : pipe_profile.get_streams() )
      {
	//REV: try pose, GPIO etc. also? :(
	
	if( s.stream_type() == RS2_STREAM_GYRO || s.stream_type() == RS2_STREAM_ACCEL )
	  {
	    hasmot = true;
	  }
      }
    
    if( !hasmot )
      {
	fprintf(stdout, "Device has no motion sensor!\n");
	fprintf(stdout, "FPS: Color: %d   Depth: %d    I1: %d  I2: %d\n", colfps, depfps, i1fps, i2fps );
      }
    else
      {
	//baseline is distance physical (extrinsic) between the two stereo cameras?
	auto acc_stream = pipe_profile.get_stream(RS2_STREAM_ACCEL);
	auto gyr_stream = pipe_profile.get_stream(RS2_STREAM_GYRO);
    
	int accfps = acc_stream.fps();
	int gyrfps = gyr_stream.fps();

	params["accel_fps"] = accfps;
	params["gyro_fps"] = gyrfps;

	fprintf(stdout, "FPS: Color: %d   Depth: %d    I1: %d  I2: %d  Gyr: %d  Accel: %d\n", colfps, depfps, i1fps, i2fps, gyrfps, accfps );
      }
    
    
    //Pipe is already started? Beginning may have laser inside them? :(
    if (sensor.supports(RS2_OPTION_EMITTER_ENABLED))
      {
	if(laseron)
	  {
	    sensor.set_option(RS2_OPTION_EMITTER_ENABLED, 1.f); // Enable emitter
	  }
	else
	  {
	    sensor.set_option(RS2_OPTION_EMITTER_ENABLED, 0.f); // Disable emitter
	  }
	bool islaseron = sensor.get_option(RS2_OPTION_EMITTER_ENABLED);
	if(islaseron)
	  {
	    params["laser_power"] = 1.f;
	  }
	else
	  {
	    params["laser_power"] = 0.f;
	  }
      }
    if (sensor.supports(RS2_OPTION_LASER_POWER))
      {
	// Query min and max values:
	auto range = sensor.get_option_range(RS2_OPTION_LASER_POWER);
	if(laseron)
	  {
	    sensor.set_option(RS2_OPTION_LASER_POWER, range.max); // Set max power
	  }
	else
	  {
	    sensor.set_option(RS2_OPTION_LASER_POWER, 0.f); // Disable laser
	  }
	double laserpower = sensor.get_option(RS2_OPTION_LASER_POWER); 
	params["laser_power"] = laserpower;
      }
    
    while( loop() && localloop() )
      {
	double sleeptime_sec = 0.500;
	loop.sleepfor( sleeptime_sec );
      }
    fprintf(stdout, "RS streaming producer -- Will stop pipeline!!\n");
    pipe.stop();
    
    std::fprintf(stdout, "realsense receiver: exiting doloop\n");
  }
  
}; //end realsense_usb_receiver
