#REV: just visualize :)
#REV: now with saliency....

import cv2
import numpy as np
import pandas as pd

import sys

#REV: use offset, etc. to access correct frames at same times ;)

tobii2vid = sys.argv[1];
tobii2ts = sys.argv[2]; #REV: text, FIDX SEC
tobii2gaze = sys.argv[3];
rsvid = sys.argv[4];
rsts = sys.argv[5];
rsgaze = sys.argv[6]

rstobii2_timeoffsetfn = sys.argv[7];
offsetf = open(rstobii2_timeoffsetfn, "r");
offsetsec = float(offsetf.readline());

salvidfn=sys.argv[8];

def draw_circles( img, ptlist, col=(255,255,0), radpx=20, linepx=3 ):
    for pt in ptlist:
        cv2.circle(img,pt,radpx,col,linepx);
    return;

scaledown=0.5;

tobii2cap = cv2.VideoCapture( tobii2vid );

#tobii2tsdf = pd.read_csv( tobii2ts, sep=' ' );
tobii2tsf = open( tobii2ts, "r" );
head=tobii2tsf.readline();

tobii2tsdict={};
for line in tobii2tsf.readlines():
    splt = line.split(' ');
    idx = int(splt[0]);
    tsec = float(splt[1]);
    tobii2tsdict[idx] = tsec;



rscap = cv2.VideoCapture( rsvid );


rssalcap = cv2.VideoCapture( salvidfn );


rstsf = open( rsts, "r" );
head=rstsf.readline();

rstsdict={};
for line in rstsf.readlines():
    splt = line.split(' ');
    idx = int(splt[0]);
    tsec = float(splt[1]);
    rstsdict[idx] = tsec;



#REV: get timepoints
tobii2gazedf = pd.read_csv(tobii2gaze, sep=' ');
rsgazedf = pd.read_csv(rsgaze, sep=' ');






tobii2time=tobii2tsdict[0]; #REV: I just know zeroindex...
tobii2idx=0;
tobii2ret, tobii2frame = tobii2cap.read();
tobii2frame = cv2.resize( tobii2frame, (int(tobii2frame.shape[1]*scaledown), int(tobii2frame.shape[0]*scaledown)) );


print("OFFSETTING RS by {} sec".format(offsetsec));
rstime=rstsdict[0] + offsetsec;
rsidx=0;
rsret, rsframe = rscap.read();
rssalret, rssalframe = rssalcap.read();
while( rstsdict[rsidx] < rstime ):
    rsret, rsframe = rscap.read();
    rssalret, rssalframe = rssalcap.read();
    rsframe = cv2.resize( rsframe, (int(rsframe.shape[1]*scaledown),int(rsframe.shape[0]*scaledown))  );
    rssalframe = cv2.resize( rssalframe, (int(rsframe.shape[1]),int(rsframe.shape[0]))  );
    if( rsret and rssalret ):
        rsidx += 1;
    else:
        print("RS ended!");
        exit(1);


dtsec=0.020;



gazedsec = 0.010;

cv2.namedWindow('tobii2');
cv2.namedWindow('rs');
cv2.namedWindow('rs SAL');

while(rsret and tobii2ret ):
    print( "TOBII Time (sec): [{:>7.3f}]".format( tobii2time ) );
    print( "RS Time (sec): [{:>7.3f}]".format( rstime ) );
    
    
    
    #REV: draw points of gaze!
    
    #REV: get rstime and tobii2time corresponding points ;)
    rsss = rsgazedf[ (rsgazedf.TSEC > rstime-gazedsec ) & (rsgazedf.TSEC < rstime+gazedsec) ];
    tobii2ss = tobii2gazedf[ (tobii2gazedf.TSEC > tobii2time-gazedsec ) & (tobii2gazedf.TSEC < tobii2time+gazedsec) ];
    
    rsxs = rsss.X * rsframe.shape[1];
    rsys = rsss.Y * rsframe.shape[0];
    tobii2xs = tobii2ss.X * tobii2frame.shape[1];
    tobii2ys = tobii2ss.Y * tobii2frame.shape[0];
    
    rspts = [ (int(x),int(y)) for x, y in zip(rsxs, rsys) ];
    tobii2pts = [ (int(x),int(y)) for x, y in zip(tobii2xs, tobii2ys) ];
    
    draw_circles( rsframe, rspts );
    draw_circles( rssalframe, rspts );
    draw_circles( tobii2frame, tobii2pts );
    
    
    cv2.imshow( 'rs', rsframe );
    cv2.imshow( 'rs SAL', rssalframe );
    cv2.imshow( 'tobii2', tobii2frame );
    
    key = cv2.waitKey(0);
    
    if( key == ord('q') ):
        print("Q pressed, quitting...")
        break;
    
    else:
        #REV: advance both forward (but by dtsec!?) -- when I "flip"
        rstime += dtsec;
        tobii2time += dtsec;
        
        while( rstsdict[rsidx] < rstime ):
            print("Advancing RS (Time={}, IDX={}, IDXTIME={})!".format(rstime, rsidx, rstsdict[rsidx]));
            rsret, rsframe = rscap.read();
            rssalret, rssalframe = rssalcap.read();
            if( rsret and rssalret):
                rsidx += 1;
                rsframe = cv2.resize( rsframe, (int(rsframe.shape[1]*scaledown),int(rsframe.shape[0]*scaledown))  );
                rssalframe = cv2.resize( rssalframe, (int(rsframe.shape[1]),int(rsframe.shape[0])) );
                #rstime = rstsdict[rsidx];
            else:
                print("RS ended!");
                break;
            
        while( tobii2tsdict[tobii2idx] < tobii2time ):
            print("Advancing TOBII1!");
            tobii2ret, tobii2frame = tobii2cap.read();
            if( tobii2ret ):
                tobii2idx += 1;
                tobii2frame = cv2.resize( tobii2frame, (int(tobii2frame.shape[1]*scaledown), int(tobii2frame.shape[0]*scaledown)) );
                
                #tobii2time = tobii2tsdict[tobii2idx];
            else:
                print("Tobii2 ended!");
                break;

exit(0);
