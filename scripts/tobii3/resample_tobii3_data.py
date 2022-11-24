# REV: takes
# 1) JSON input filename datafname
# ( Implicit 2) jsonfname + ".ts" file (time stamps)
# 2) sample timebase per sec is 90k (we just know it...)
# 3) output fname
# 4) (optional) resample rate (hz/sec). Default is 200... (tobii sample rate)

import sys
import struct
import json
import pandas as pd
import numpy as np

#REV:  use normal char? E.g. use ascii.. so 'd' for double, etc.
#REV:  tell endianness etc.?
def get_next_u8_time_from_filehandle( f ):
    toread=1;
    bytes = f.read(toread);
    if( len(bytes) < toread ):
        return None;
    return struct.unpack('<b', bytes)[0];

def get_next_u64_time_from_filehandle( f ):
    toread=8;
    bytes = f.read(toread);
    if( len(bytes) < toread ):
        return None;
    return struct.unpack('<Q', bytes)[0];

def get_next_u32_time_from_filehandle( f ):
    toread=4;
    bytes = f.read(toread);
    if( len(bytes) < toread ):
        return None;
    return struct.unpack('<I', bytes)[0];

def get_next_f32_time_from_filehandle( f ):
    toread=4;
    bytes = f.read(toread);
    if( len(bytes) < toread ):
        return None;
    return struct.unpack('<f', bytes)[0];

def get_next_f64_time_from_filehandle( f ):
    toread=8;
    bytes = f.read(toread);
    if( len(bytes) < toread ):
        return None;
    return struct.unpack('<d', bytes)[0];


tagtype = { 0 : 'uint64', 1 : 'float64', 2 : 'uint32', 3 : 'float32' };

def read_unpack_next_tagtype( myf, ts ):
    if( ts in tagtype ):
        tag = tagtype[ts];
    else:
        print("Unrecognized type...?");
        return None
    if( tag == 'uint64' ):
        return get_next_u64_time_from_filehandle(myf);
    elif( tag == 'uint32' ):
        return get_next_u32_time_from_filehandle(myf);
    elif( tag == 'float32' ):
        return get_next_f32_time_from_filehandle(myf);
    elif( tag == 'float64' ):
        return get_next_f64_time_from_filehandle(myf);
    else:
        return None;

#REV: this could get LARGE
def timestamps_from_file( fn, tb_hz_sec ):
    times = open( fn, "rb" );
    tstype = get_next_u8_time_from_filehandle( times );
    if tstype is not None:
        print( "Type {}".format( int(tstype)) );
        pass;
    else:
        exit(0);
        pass;

    res = [];
    idx=0;
    while( True ):
        val = read_unpack_next_tagtype( times, tstype );
        if( val == None ):
            break;
        else:
            if( 0 == idx ):
                zerotime = val;
                pass;
            assec = (val - zerotime) / tb_hz_sec;
            #print("Timestamp: {}  (Zeroed Sec: {})".format( val, assec ) );
            #if outf is not None:
            #    outf.write( "{} {} {}\n".format( idx, assec, val ) );
            res.append( [ idx, assec, val ] );
            idx+=1;
            pass;
        pass;
    
    return res;


#REV: I now have idx, sec, val for each. Note first is "zeroed"...
def df_from_type( colnames, myname, data ):
    if( False == isinstance( data, list ) ):
        data = [data];
        pass;
    
    for d in range(len(data)):
        colnames.append(myname+"_"+str(d));
        pass;
    
    df = pd.DataFrame( columns=colnames );
    return df;


#REV: just take CLOSEST!
if(__name__ == "__main__"):
    jsonfname = sys.argv[1];
    tsfname = jsonfname + ".ts";
    tb_hz_sec = 90000;

    resample_hz_sec = float(sys.argv[2]);
    
    tss = timestamps_from_file( tsfname, tb_hz_sec );
    
    jsonfh = open( jsonfname, "r" );
    if not jsonfh:
        print("Could not open JSON filename {}".format(jsonfname));
        exit(1);
        pass;

    dicts = {};

    mint = None;
    maxt = None;
    
    idx=0;
    while( True ):
        line = jsonfh.readline();
        if not line:
            print("End of json file {}".format(jsonfname));
            break;
        
        jline = json.loads(line); #REV: ok to have \n?
        if( idx > len(tss) ):
            print("Error?");
            exit(1);
            pass;

        tidx = tss[idx][0];
        tsec = tss[idx][1];
        tval = tss[idx][2];

        if( mint is None or mint > tsec ):
            mint = tsec;
            pass;

        if( maxt is None or maxt < tsec ):
            maxt = tsec;
            pass;
        
        if( tidx != idx ):
            print("We have a problem");
            exit(1);
            pass;
        
        #print("Line {:4d}: {:5.3f} [{}]".format(idx, tsec, jline) );

        
        
        #REV: Note: may be out of order, e.g. accel first, then previous gaze, etc.
        #REV: so, parse each json "elemet" separately.
        
        mystrs = ["accelerometer", "gyroscope", "eyeleft", "eyeright", "gaze2d", "gaze3d"];
        for mystr in mystrs:
            if(mystr in jline):
                if( mystr == "eyeleft" or mystr == "eyeright" ):
                    str2s = ["gazedirection", "gazeorigin", "pupildiameter"];
                    for mystr2 in str2s:
                        if( mystr2 in jline[mystr] ):
                            val = jline[mystr][mystr2];
                            if( False == isinstance(val, list) ):
                                val = [val];
                                pass;
                            myname = mystr+"_"+mystr2;
                            if( myname not in dicts ):
                                colnames=["Tsec"];
                                dicts[myname] = df_from_type(colnames, myname, val);
                                pass;
                            
                            toappend = [tsec] + val;
                            dicts[myname].loc[ len(dicts[myname].index) ] = toappend;
                            #print("{:5.3f} {} {}".format(tsec, myname, val));
                            pass;
                        pass;
                    pass;
                else:
                    myname = mystr;
                    val = jline[mystr];
                    if( False == isinstance(val, list) ):
                        val = [val];
                        pass;
                    
                    if( myname not in dicts ):
                        colnames=["Tsec"];
                        dicts[myname] = df_from_type(colnames, myname, val);
                        pass;
                                        
                    toappend = [tsec] + val;
                    dicts[myname].loc[ len(dicts[myname].index) ] = toappend;
                    #print("{:5.3f} {} {}".format(tsec, myname, val));
                    pass;
                pass;
            pass;

        stop=-1; #12.0; #-1; #12.0
        if(stop>0 and tsec > stop):
            break;
        
        PRINTSKIP=500;
        if( idx % PRINTSKIP == 0 ):
            print("Line {}/{} ({:4.1f}% )".format(idx,len(tss), float(idx)/len(tss)*100));
        idx+=1;
        pass;
    
    print("Will print");
    #print(dicts);
    for mydict in dicts:
        print(mydict);
        print(dicts[mydict]);
        pass;


    #REV: create resampled (of each?)
    tdelta = 1.0 / resample_hz_sec;
    sidx = 0;
    
    stidx = sidx * tdelta;

    #REV: create it (need all column names)
    newcolnames = ["Tsec"];
    
    for mydict in dicts:
        mydictcols = dicts[mydict].columns[ dicts[mydict].columns != "Tsec" ];
        newcolnames = newcolnames + mydictcols.tolist();
        pass;

    resampdf = pd.DataFrame( columns=newcolnames );

    print( maxt );
    while( stidx <= maxt ):
        stidx2 = stidx+tdelta;
        newrow = [stidx];
        
        for mydict in dicts:
            subdict = dicts[mydict][ (dicts[mydict].Tsec >= stidx) & (dicts[mydict].Tsec < stidx2) ];
            mydictcols = dicts[mydict].columns[ dicts[mydict].columns != "Tsec" ];
            if( len( subdict ) < 1 ):
                #REV: empty. Paste null values?
                toadd = len(mydictcols) * [None];
                pass;
            else:
                toadd = subdict[ mydictcols ].iloc[0].values.tolist();
                pass;
            newrow = newrow + toadd;
            pass;

        PRINTSKIP=500;
        if( sidx % PRINTSKIP == 0 ):
            print(tdelta);
            maxidx = (maxt/tdelta);
            print("Line {}/{} ({:4.1f}% )".format(sidx, maxidx, float(sidx)/maxidx*100));
            print("Adding row {} (t={:4.3f})".format(sidx, stidx));
            pass;
        
        #print(newrow);
        #print(len(newrow));
        resampdf.loc[ len(resampdf) ] = newrow;
        
        sidx += 1;
        stidx = sidx * tdelta;
        pass;
    
    print_out_csvs = True;
    if(print_out_csvs):
        mydir = '/'.join( jsonfname.split('/')[:-1] );

        resampfname = "resampled.csv";
        fullpath = mydir + "/" + resampfname; #REV: if exist, what to do?
        resampdf.to_csv( fullpath, index=False );
        
        for mydict in dicts:
            ofname = mydict + ".csv";
            fullpath = mydir + "/" + ofname; #REV: if exist, what to do?
            dicts[mydict].to_csv( fullpath, index=False );
            pass;
        
        pass;
    
    pass;

