
#REV: recover ghetto-encoded CV MAT videos


import cv2
import numpy as np
import sys


fn = sys.argv[1];


cap = cv2.VideoCapture( fn );

retval = true;

retval, img = cap.read();
while( retval ):
    
    
    retval, img = cap.read();
    
