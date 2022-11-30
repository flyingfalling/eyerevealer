import sys
import cv2
import numpy as np



def embed_edge_gauss_blur_create( inimg255, cxpx, cypx, bgwid, bghei, bgrgb255, blurlimpx, blursigmult=1/3.0 ):
    bgimg = np.full( (bghei, bgwid, 3), bgrgb255, dtype=np.uint8 );
    return embed_edge_gauss_blur( inimg255, cxpx, cypx, bgimg, blurlimpx, blursigmult );
    
def embed_edge_gauss_blur( inimg255, cxpx, cypx, bgimg255, blurlimpx, blursigmult=1/3.0 ):
    sigmax=blurlimpx*blursigmult;

    mask = np.zeros( inimg255.shape, dtype=np.uint8 );
    mask[ int(blurlimpx):int(mask.shape[0]-blurlimpx), int(blurlimpx):int(mask.shape[1]-blurlimpx), : ] = 255;
    
    mask_blurred  = cv2.GaussianBlur( mask, ksize=(0,0), sigmaX=sigmax, sigmaY=0, borderType=cv2.BORDER_CONSTANT );
    mask_blurred_3chan = mask_blurred/255.0; #cv2.cvtColor(mask_blurred, cv2.COLOR_GRAY2BGR).astype('float') / 255.0;

    cv2.imshow("Small Mask", mask_blurred_3chan );
    cv2.waitKey(0);
    
    res = bgimg255.copy();
    
    cxpx = int(cxpx);
    cypx = int(cypx);
    
    if( inimg255.shape[1] % 2 == 1 ):
        left = cxpx - int(inimg255.shape[1]/2); #e.g. if center is 10, and it is 11, we will get 10-5 to 10+5 = 5 to 15, total 11
        right = cxpx + int(inimg255.shape[1]/2 + 1);
        pass;
    else:
        left = cxpx - int(inimg255.shape[1]/2 - 1); #e.g. if center is 10, and it is 10, we will get 10-4 to 10+5 = 6 to 15, i.e. 10
        right = cxpx + int(inimg255.shape[1]/2 + 1);
        pass;
    
    if( inimg255.shape[0] % 2 == 1 ):
        top = cypx - int(inimg255.shape[0]/2); #e.g. if center is 10, and it is 11, we will get 10-5 to 10+5 = 5 to 15, total 11
        bot = cypx + int(inimg255.shape[0]/2 + 1);
        pass;
    else:
        top = cypx - int(inimg255.shape[0]/2 - 1); #e.g. if center is 10, and it is 10, we will get 10-4 to 10+5 = 6 to 15, i.e. 10
        bot = cypx + int(inimg255.shape[0]/2 + 1);
        pass;

    #REV: insert is totally outside image...
    if( left >= res.shape[1] or right < 0 or bot < 0 or top >= res.shape[0] ):
        return res;


    itop=0;
    ileft=0;
    ibot=inimg255.shape[0];
    iright=inimg255.shape[1];
    if( left < 0 ):
        ileft = -left;
        left=0;
        pass;
    if( right > res.shape[1] ):
        iright = inimg255.shape[1] - (right - res.shape[1]);
        right=res.shape[1];
        pass;
    if( top < 0 ):
        itop = -top;
        top=0;
        pass;
    if( bot > res.shape[0] ):
        ibot = inimg255.shape[0] - (bot - res.shape[0]);
        bot=res.shape[0];
        pass;
        
    res = res.astype(np.float32) / 255.0;
    inimg = inimg255.astype(np.float32) / 255.0;

    if( bot-top != inimg.shape[0] ):
        print("WARN: Bot-Top: {}-{} = {}  !=  Img Hei: {}".format(bot, top, bot-top, inimg.shape[0]));
        #exit(1);
        pass;
    
    if( right-left != inimg.shape[1] ):
        print("WARN: Right-Left: {}-{} = {}  !=  Img Wid: {}".format(right, left, right-left, inimg.shape[1]));
        #exit(1);
        pass;

    print( "(Res: {}    T:{} B:{} L:{} R:{}    Hei: {}    Wid: {}     img: {}  mask: {}".format(res.shape, top, bot, left, right, bot-top, right-left, inimg.shape, mask_blurred_3chan.shape));
    
    #print(res[ top:bot, left:right, : ].shape);
    res[ top:bot, left:right, : ] = (inimg[itop:ibot,ileft:iright,:] * mask_blurred_3chan[itop:ibot,ileft:iright,:])  + (res[ top:bot, left:right, : ] * (1.0 - mask_blurred_3chan[itop:ibot,ileft:iright,:]));
    return res;

        
inimgname=sys.argv[1];
inx=sys.argv[2];
iny=sys.argv[3];
inimg = cv2.imread( inimgname );

res = embed_edge_gauss_blur_create( inimg, inx, iny, 640, 480, [120,120,120], 60 );
cv2.imshow("Result", res);
cv2.waitKey(0);

exit(0);



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
print( bgimg.shape );
        

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
