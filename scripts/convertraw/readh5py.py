import h5py
import sys
import cv2
import numpy as np

fn = sys.argv[1];
f = h5py.File( fn );
mylist = list(f.keys())
print(mylist);
d = f['data']
print(d.shape);


if( len(sys.argv) > 3 ):
    outfn = sys.argv[2];
    matchcolorfn = sys.argv[3];
    cap = cv2.VideoCapture(matchcolorfn);
    colwidth  = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))  # float
    colheight = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) # float
    fps = cap.get(cv2.CAP_PROP_FPS)
    
rows=d.shape[1];
cols=d.shape[2];

nmats=d.shape[0];

#REV make the vw

#REV: match FPS of the normal?
if( len(sys.argv) > 2 ):
    fourcc_str = 'FFV1';
    fourcc = cv2.VideoWriter_fourcc(*fourcc_str)
    #REV: isColor=False, but doesn't work, need C zero ;/
    vw = cv2.VideoWriter( outfn, fourcc, fps, (cols,rows), 0 );
    #vw = cv2.VideoWriter( outfn, fourcc, fps, (colwidth,colheight), 0 );
    
    

for i in range(nmats):
    img=d[i,0:rows-1,0:cols-1];
    
    #REV: convert it to 255...colorize it? :/
    
    img2 = img/257.0;
    img2 = img2.astype(np.uint8);
    if( 0 == i % 5000 ):
        print("Depth frame {:>5d}/{:>5d}  (min/max: {} {}    rescaled: {} {}".format(i, nmats, img.min(), img.max(), img2.min(), img2.max()) );
    #dst = cv2.resize( img2, (colwidth, colheight) );
    dst = cv2.resize( img2, (cols, rows) );
    
    if( len(sys.argv) > 2 and vw.isOpened() ):
        vw.write( dst );
    else:
        #dst = cv2.resize( img2, (int(img2.shape[1]*0.5), int(img2.shape[0]*0.5)) );
        cv2.imshow( "uint8", dst );
        cv2.waitKey(1);
    
