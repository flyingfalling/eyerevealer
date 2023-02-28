#REV: rectilinear lens? But higher angles makes objects "stretched"
#REV: like mercantor projection...
#REV: won't "undistort" fix that? No, it only fixes parallel lines, not sizes...?

#REV: pixels in image correspond to PIXELS OF THE (flat) sensor. Which get weird mapping, and its flat, not curved like retina on eye...but
#REV: even eye is curved in circle...lens passes light not at "real" angles.

#REV: So, because of tangent function, we get "less" total angle on our vertical part than our horizontal part. And furthermore diagnals get
#REV: even more messed up? As "distance"? No...actually lens should be basically "round" so distance from "center" should be all that matters.
#REV: of course corners are "furthest" away...
#90 deg. 16:9 format   and 82 x 52.
# so...assuming 16:9 pixels...is 1.7777. I.e. angle will be ...    tan(x) = opp/adj. Lower left, so opp is height, adj is wid
#  So, angle is...29 deg? wtf? math.atan2(9, 16) = 0.51238 radians... hyp=94 if we use width dva...or 106 if we use the height?

## undistort
#mapx, mapy = cv.initUndistortRectifyMap(mtx, dist, None, newcameramtx, (w,h), 5)
#dst = cv.remap(img, mapx, mapy, cv.INTER_LINEAR)

#REV: lets think from the tangent point of view. I want to map degrees represented by physical tangent of given size.

#ang=math.atan2(9, 16)
#hyp = adj/math.cos(ang)
#adj=82;
#opp=52; #REV: or 48...

#REV: lets do 16:9, given a rectangle sensor, what is the ratio if we do sin for each size (at a given distance? Uh? Focal length?)
#REV: Focal length is: 1136.65002441...does it matter? How about physical sensor size? No idea doesn't matter?

#ang1=82/2;
#ang2=52/2;
#opp1 = 16/2;
#opp2 = 9/2;
#d1 = opp1/math.tan(math.radians(ang1));
#d2 = opp2/math.tan(math.radians(ang2));

#REV: For this to work, D (towards me, i.e. in z), is:
#>>> d1
#9.202947257768077
#>>> d2
#9.226367287106832

#REV: that's not bad. So, this explains it. They are simply doing trig at some distance (focal point)
#REV: the ones on the edge will gather less light (the square will be distorted/skewed). That's the skew I wanted to do though...
#REV: really, the only way to figure this out is to compute it all from trig? I know my "distance" that satisfies these...so I can use that to compute
#REV: e.g. polar angle and stuff from X, Y
#REV: I can't really undistort though, so first I will just have to separately use X and Y pixel sizes. I really need to convert to angle though.
#REV: I.e. for each one, figure out the true angle by doing the actual trig from my dist of 9.214 ish.
#REV: note "center of gaze" from the eye does not go from the camera lol, it goes from below it, so that fucks shit up too. No matter for long distances...
#REV: although disparity will shift by a few pixels...

#REV: For computing velocity, I really need to do that shit. Anyways...these represent angle of view...wtf? Yea... Note middle  or so is pretty linear.
#REV: here we use 82? So, +11... we at about 5% error at 25 deg. 20%+ error at 41 deg.

import cv2
import numpy as np

fs = cv2.FileStorage(str('calibration.xml'), cv2.FILE_STORAGE_READ);
r=fs.root();
mtx = r.getNode('cameraMatrix').mat();
rdist = r.getNode('radialDistortion').mat();
tdist = r.getNode('tangentialDistortion').mat();
dist = r.getNode('distCoeff').mat();

import sys;
fname = sys.argv[1];

cap = cv2.VideoCapture( fname );
w  = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH));
h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT));
fps = cap.get(cv2.CAP_PROP_FPS);

newcameramtx, roi = cv2.getOptimalNewCameraMatrix(mtx, dist, (w,h), 1, (w,h));

while(True):
    ret, frame = cap.read();
    if( False == ret ):
        break;
    
    dst = cv2.undistort(frame, mtx, dist, None, newcameramtx)
    dst = cv2.resize( dst, None, fx=0.5, fy=0.5 );
    frame = cv2.resize( frame, None, fx=0.5, fy=0.5 );
    cv2.imshow("Undist", dst );
    cv2.imshow("Dist", frame );
    key = cv2.waitKey(0);
    if( key == ord('q')):
        break;
    
fs.release();
