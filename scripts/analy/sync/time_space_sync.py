import cv2
import math
import numpy as np
import sys

import pandas as pd

tobii2vid = sys.argv[1];
tobii2ts = sys.argv[2]; #REV: text, FIDX SEC
rsvid = sys.argv[3];
rsts = sys.argv[4]; #REV: this will be binary, with header...

outptsfn = sys.argv[5];
outoffsetfn = sys.argv[6];

import numpy as np
from scipy.linalg import lstsq
from scipy.special import binom
import numpy.polynomial.polynomial



#REV: TODO: do polynomial fitting in 3 dimensions (for X, Y and depth). To fit X, Y of target.
#REV: because right now it only works if they are far away I.e. outside car (ish)...

############# POLYNOMIAL FITTING #############################

def _get_coeff_idx(coeff):
    idx = np.indices(coeff.shape)
    idx = idx.T.swapaxes(0, 1).reshape((-1, 2))
    return idx


def _scale(x, y):
    # Normalize x and y to avoid huge numbers
    # Mean 0, Variation 1
    offset_x, offset_y = np.mean(x), np.mean(y)
    norm_x, norm_y = np.std(x), np.std(y)
    x = (x - offset_x) / norm_x
    y = (y - offset_y) / norm_y
    return x, y, (norm_x, norm_y), (offset_x, offset_y)


def _unscale(x, y, norm, offset):
    x = x * norm[0] + offset[0]
    y = y * norm[1] + offset[1]
    return x, y


def polyvander2d(x, y, degree):
    A = np.polynomial.polynomial.polyvander2d(x, y, degree)
    return A


def polyscale2d(coeff, scale_x, scale_y, copy=True):
    if copy:
        coeff = np.copy(coeff)
    idx = _get_coeff_idx(coeff)
    for k, (i, j) in enumerate(idx):
        coeff[i, j] /= scale_x ** i * scale_y ** j
    return coeff


def polyshift2d(coeff, offset_x, offset_y, copy=True):
    if copy:
        coeff = np.copy(coeff)
    idx = _get_coeff_idx(coeff)
    # Copy coeff because it changes during the loop
    coeff2 = np.copy(coeff)
    for k, m in idx:
        not_the_same = ~((idx[:, 0] == k) & (idx[:, 1] == m))
        above = (idx[:, 0] >= k) & (idx[:, 1] >= m) & not_the_same
        for i, j in idx[above]:
            b = binom(i, k) * binom(j, m)
            sign = (-1) ** ((i - k) + (j - m))
            offset = offset_x ** (i - k) * offset_y ** (j - m)
            coeff[k, m] += sign * b * coeff2[i, j] * offset
    return coeff





def polyfit2d(x, y, z, degree=1, max_degree=None, scale=True, plot=False):
    """A simple 2D polynomial fit to data x, y, z
    The polynomial can be evaluated with numpy.polynomial.polynomial.polyval2d

    Parameters
    ----------
    x : array[n]
        x coordinates
    y : array[n]
        y coordinates
    z : array[n]
        data values
    degree : {int, 2-tuple}, optional
        degree of the polynomial fit in x and y direction (default: 1)
    max_degree : {int, None}, optional
        if given the maximum combined degree of the coefficients is limited to this value
    scale : bool, optional
        Wether to scale the input arrays x and y to mean 0 and variance 1, to avoid numerical overflows.
        Especially useful at higher degrees. (default: True)
    plot : bool, optional
        wether to plot the fitted surface and data (slow) (default: False)

    Returns
    -------
    coeff : array[degree+1, degree+1]
        the polynomial coefficients in numpy 2d format, i.e. coeff[i, j] for x**i * y**j
    """
    # Flatten input
    x = np.asarray(x).ravel()
    y = np.asarray(y).ravel()
    z = np.asarray(z).ravel()
    
    # Remove masked values
    mask = ~(np.ma.getmask(z) | np.ma.getmask(x) | np.ma.getmask(y))
    x, y, z = x[mask].ravel(), y[mask].ravel(), z[mask].ravel()
    
    # Scale coordinates to smaller values to avoid numerical problems at larger degrees
    if scale:
        x, y, norm, offset = _scale(x, y)
        
    if np.isscalar(degree):
        degree = (int(degree), int(degree))
    degree = [int(degree[0]), int(degree[1])]
    coeff = np.zeros((degree[0] + 1, degree[1] + 1))
    idx = _get_coeff_idx(coeff)
    
    # Calculate elements 1, x, y, x*y, x**2, y**2, ...
    A = polyvander2d(x, y, degree)
    
    # We only want the combinations with maximum order COMBINED power
    if max_degree is not None:
        mask = idx[:, 0] + idx[:, 1] <= int(max_degree)
        idx = idx[mask]
        A = A[:, mask]
        
    # Do the actual least squares fit
    C, *_ = lstsq(A, z)
    
    # Reorder coefficients into numpy compatible 2d array
    for k, (i, j) in enumerate(idx):
        coeff[i, j] = C[k]
        
    # Reverse the scaling
    if scale:
        coeff = polyscale2d(coeff, *norm, copy=False)
        coeff = polyshift2d(coeff, *offset, copy=False)
        
    return coeff


def map_poly_point( x, y, xcoeffs, ycoeffs ):
    xval = np.polynomial.polynomial.polyval2d(x, y, xcoeffs);
    yval = np.polynomial.polynomial.polyval2d(x, y, ycoeffs);
    return xval, yval;


def map_poly_points( xs, ys, xcoeffs, ycoeffs ):
    xvals = np.polynomial.polynomial.polyval2d(xs, ys, xcoeffs);
    yvals = np.polynomial.polynomial.polyval2d(xs, ys, ycoeffs);
    return xvals, yvals;






TOBII_PTS = [(849, 255), (519, 212), (944, 103), (1134, 106), (1222, 98), (1353, 95), (1527, 84), (1684, 73), (1463, 69), (1656, 216), (1508, 223), (1366, 223), (1070, 209), (1746, 383), (1406, 454), (608, 445), (138, 75), (1305, 42), (994, 434), (726, 508), (1209, 534), (803, 70), (413, 96), (389, 443), (1705, 431), (1215, 8), (950, 21), (654, 9), (175, 200), (1156, 322), (875, 375), (741, 287), (1629, 9), (267, 380), (476, 365), (233, 495), (1229, 435), (618, 312), (1084, 302), (964, 302), (1568, 308), (1681, 317), (1561, 454), (1392, 319)];

RS_PTS = [(550, 352), (295, 322), (627, 239), (766, 239), (831, 234), (932, 227), (1065, 219), (1179, 214), (1016, 210), (1175, 315), (1060, 322), (951, 325), (719, 314), (1270, 439), (1003, 504), (356, 500), (17, 220), (884, 188), (658, 488), (445, 551), (842, 570), (507, 213), (208, 235), (169, 509), (1218, 472), (826, 166), (732, 173), (406, 170), (25, 308), (795, 396), (572, 442), (466, 374), (1135, 166), (43, 450), (213, 436), (31, 543), (886, 501), (368, 390), (737, 378), (639, 382), (1115, 377), (1206, 384), (1079, 457), (978, 389)];


if( len(TOBII_PTS) != len(RS_PTS) ):
    print("FAIL");
    exit(1);

OFFSET_SEC = 0.5196928710936461;

#rstobii2_timeoffset = OFFSET_SEC;
tobii2wid = 1920.0;
tobii2hei = 1080.0;
rswid = 1280.0;
rshei = 720.0;
x1 = numpy.array([ float(x) for (x,y) in TOBII_PTS ]) / tobii2wid;
y1 = numpy.array([ float(y) for (x,y) in TOBII_PTS ]) / tobii2hei;
x2 = numpy.array([ float(x) for (x,y) in RS_PTS ]) / rswid;
y2 = numpy.array([ float(y) for (x,y) in RS_PTS ]) / rshei;
    

#print("Doing with REALSENSE -> TOBII2 time offset {} sec".format(rstobii2_timeoffset));
    

#calibdata = np.genfromtxt(calibptsfn, delimiter=' '); #REV: header? Or use panda?

xcoeffs = polyfit2d( x1, y1, x2, degree=2 );
ycoeffs = polyfit2d( x1, y1, y2, degree=2 );

grayxs = np.linspace(0.0, 1.0, 20);
grayys = np.linspace(0.0, 1.0, 20);

graypts=[];
for gx in grayxs:
    for gy in grayys:
        graypts.append( (gx,gy) ); #tobii

allgx=[ x for (x,y) in graypts ];
allgy=[ y for (x,y) in graypts ];
rsgrayxs, rsgrayys = map_poly_points( allgx, allgy, xcoeffs, ycoeffs );
rsgraypts = [ (x*rswid, y*rshei) for x, y in zip( rsgrayxs, rsgrayys ) ];

tobii2graypts = [ (x*tobii2wid, y*tobii2hei) for x, y in zip( allgx, allgy ) ];

print(rsgraypts);
print(tobii2graypts);

#REV: plot some grey guys on an xy source grid.


tobii2pts=[];
rspts=[];


dfin = pd.read_csv(outptsfn, sep=' ');
if(len(dfin)>0):
    tobii2pts = [ (x,y) for x, y in zip( dfin['X1'], dfin['Y1'] ) ];
    rspts = [ (x,y) for x, y in zip( dfin['X2'], dfin['Y2'] ) ];


rsframe = None;
tobii2frame = None;

#REV: need good way to erase last point? Right click is inside circle -> erase it?

def draw_circles( img, ptlist, col=(255,255,0), radpx=20, linepx=3 ):
    for pt in ptlist:
        cv2.circle(img,(int(pt[0]), int(pt[1])),radpx,col,linepx);
    return;

def click_tobii2(event,x,y,flags,param):
    if event == cv2.EVENT_LBUTTONDBLCLK:
        cv2.circle(tobii2frame,(x,y),20,(255,255,0),3);
        print("Appended points {} to tobii2".format((x,y)));
        tobii2pts.append( (x,y) );
    return;


def click_rs(event,x,y,flags,param):
    if event == cv2.EVENT_LBUTTONDBLCLK:
        cv2.circle(rsframe,(x,y),20,(255,255,0),3);
        print("Appended points {} to rs".format((x,y)));
        rspts.append( (x,y) );
    return;



tobii2cap = cv2.VideoCapture( tobii2vid );


tobii2tsf = open( tobii2ts, "r" );
head=tobii2tsf.readline();

tobii2tsdict={};
for line in tobii2tsf.readlines():
    splt = line.split(' ');
    idx = int(splt[0]);
    tsec = float(splt[1]);
    tobii2tsdict[idx] = tsec;



rscap = cv2.VideoCapture( rsvid );

rstsf = open( rsts, "r" );
head=rstsf.readline();

rstsdict={};
for line in rstsf.readlines():
    splt = line.split(' ');
    idx = int(splt[0]);
    tsec = float(splt[1]);
    rstsdict[idx] = tsec;




######### REGISTER MOUSE CALLBACKS #########
cv2.namedWindow('tobii2');
cv2.setMouseCallback('tobii2',click_tobii2);

cv2.namedWindow('rs');
cv2.setMouseCallback('rs',click_rs);



rstime=rstsdict[0];
rsidx=0;

tobii2time=tobii2tsdict[0]; #REV: I just know zeroindex...
tobii2idx=0;

rsret, rsframe = rscap.read();
tobii2ret, tobii2frame = tobii2cap.read();

dtsec=0.010;

scaledown=0.5;

#REV: seek etc. in them?
while(rsret and tobii2ret ):
    print( "TOBII Time (sec): [{:>7.3f}]".format( tobii2time ) );
    print( "RS Time (sec): [{:>7.3f}]".format( rstime ) );
    
    print("RS {}    TB {}".format(rsframe.shape, tobii2frame.shape) )
    cv2.imshow( 'rs', rsframe );
    cv2.imshow( 'tobii2', tobii2frame );
    offsetsec = rstime-tobii2time; #positive means RS offset relative to tobii offset, both in terms of lag and start time.
    key = cv2.waitKey(0);
    
    if( key == ord('q') ):
        print("Q pressed, quitting...")
        print("TOBII PTS", tobii2pts);
        print("RS PTS", rspts);
        print("OFFSET SEC", offsetsec);
                         
        df = pd.DataFrame( columns=[ "X1", "Y1", "X2", "Y2" ] );
        txs = [ x for (x,y) in tobii2pts ];
        tys = [ y for (x,y) in tobii2pts ];
        rxs = [ x for (x,y) in rspts ];
        rys = [ y for (x,y) in rspts ];
        print(txs);
        print(tys);
        print(rxs);
        print(rys);
        df['X1'] = txs;
        df['Y1'] = tys;
        df['X2'] = rxs;
        df['Y2'] = rys;
        df.to_csv( outptsfn, sep=' ' );

        offsetfh = open( outoffsetfn, "w" );
        wrotebytes = offsetfh.write( "{}".format(float(offsetsec)) );
        print("Wrote {} bytes! ({})".format( wrotebytes, float(offsetsec) ) );
        offsetfh.close();
        break; #REV: and print out
    
    elif( key == ord('[') ):
        #REV: advance one tobii2 forward +dt (just advance index to next one?)
        tobii2time += dtsec;
        while( tobii2tsdict[tobii2idx] < tobii2time ):
            tobii2ret, tobii2frame = tobii2cap.read();
            if( tobii2ret ):
                tobii2idx += 1;
                tobii2time = tobii2tsdict[tobii2idx];
                draw_circles( tobii2frame, tobii2pts );
                draw_circles(tobii2frame, TOBII_PTS);
                draw_circles( tobii2frame, tobii2graypts, col=(200,200,200), linepx=2, radpx=12);
            else:
                print("Tobii2 ended!");
                break;
        
    elif( key == ord(']') ):
        #REV: advance one rs forward +dt (just advance index?)
        rstime += dtsec;
        while( rstsdict[rsidx] < rstime ):
            rsret, rsframe = rscap.read();
            if( rsret ):
                rsidx += 1;
                rstime = rstsdict[rsidx];
                draw_circles( rsframe, rspts );
                draw_circles( rsframe, RS_PTS);
                draw_circles( rsframe, rsgraypts, col=(200,200,200), linepx=2, radpx=12);
                
                #REV: gray points, need to mult though.
            else:
                print("RS ended!");
                break;
    else:
        #REV: advance both forward (but by dtsec!?) -- when I "flip"
        rstime += dtsec;
        tobii2time += dtsec;
        
        while( rstsdict[rsidx] < rstime ):
            print("Advancing RS (Time={}, IDX={}, IDXTIME={})!".format(rstime, rsidx, rstsdict[rsidx]));
            rsret, rsframe = rscap.read();
            if( rsret ):
                rsidx += 1;
                #rstime = rstsdict[rsidx];
            else:
                print("RS ended!");
                break;
            
        while( tobii2tsdict[tobii2idx] < tobii2time ):
            print("Advancing TOBII1!");
            tobii2ret, tobii2frame = tobii2cap.read();
            if( tobii2ret ):
                tobii2idx += 1;
                #tobii2time = tobii2tsdict[tobii2idx];
            else:
                print("Tobii2 ended!");
                break;
        
        draw_circles( rsframe, rspts );
        draw_circles( tobii2frame, tobii2pts );
        
        
        draw_circles( rsframe, RS_PTS);
        draw_circles( tobii2frame, TOBII_PTS )
        draw_circles( rsframe, rsgraypts, col=(200,200,200), linepx=2, radpx=12);
        draw_circles( tobii2frame, tobii2graypts, col=(200,200,200), linepx=2, radpx=12);
        
    





exit(0);
