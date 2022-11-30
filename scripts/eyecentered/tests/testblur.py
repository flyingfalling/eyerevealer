import sys
import cv2
import numpy as np


inimgname=sys.argv[1];
inimg = cv2.imread( inimgname );

bgwidpx=2000;
bgheipx=1500;
#bgcolbgr=[0,0,0];
bgcolbgr=[100,100,100];

xpos=200; #REV: cv, from left!
ypos=200; #REV: cv, from top!

#REV: create "foreground"?
fgimg = np.zeros( (bgheipx, bgwidpx, 3), dtype=np.uint8 );
fgimg[int(ypos):int(ypos+inimg.shape[0]),int(xpos):int(xpos+inimg.shape[1]),:] = inimg;

bgimg = np.full( (bgheipx, bgwidpx, 3), bgcolbgr, dtype=np.uint8 );

#REV: should depend on height of largest scale pyramid?
opx=60;

#REV: mask is 1 where OK, black where not. Note will use "constant" outside value...? Of...zero?
mask = np.zeros( (bgheipx, bgwidpx), dtype=np.uint8 );
#mask[ int(ypos):int(ypos+inimg.shape[0]),int(xpos):int(xpos+inimg.shape[1]) ] = 255;
mask[ int(ypos+opx):int((ypos+inimg.shape[0])-opx),int(xpos+opx):int((xpos+inimg.shape[1])-opx) ] = 255; #255

sigmax=opx/3;
mask_blurred  = cv2.GaussianBlur( mask, ksize=(0,0), sigmaX=sigmax, sigmaY=0 ); #0 is outside value? border?
mask_blurred_3chan = cv2.cvtColor(mask_blurred, cv2.COLOR_GRAY2BGR).astype('float') / 255.0;

cv2.imshow("Mask", mask_blurred_3chan );
cv2.waitKey(0);

fgimg  = fgimg.astype('float') / 255.0;
bgimg = bgimg.astype('float') / 255.0;

res  =  bgimg * (1 - mask_blurred_3chan)  +  fgimg * mask_blurred_3chan;

#res  =  fgimg * mask_blurred_3chan;

cv2.imshow("Result", res);
key = cv2.waitKey(0);

exit(0);
