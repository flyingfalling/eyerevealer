

import sys
import struct
fn = sys.argv[1];
tb_hz_sec = float(sys.argv[2]);
if( len(sys.argv) > 3 ):
    outfn = sys.argv[3];
    outf = open(outfn, "w");
else:
    outf = None;

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



vidtimes = open( fn, "rb" );

tstype = get_next_u8_time_from_filehandle( vidtimes );
if tstype is not None:
    print( "Type {}".format( int(tstype)) );
else:
    exit(0);

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

idx=0;

##HEADER
outf.write( "{} {} {}\n".format( "FRAME", "TSEC", "TS" ) );

zerotime=-1;
while( True ):
    val = read_unpack_next_tagtype( vidtimes, tstype );
    if( val == None ):
        break;
    else:
        if( 0 == idx ):
            zerotime = val;
        assec = (val - zerotime) / tb_hz_sec;
        print("Timestamp: {}  (Zeroed Sec: {})".format( val, assec ) );
        if outf is not None:
            outf.write( "{} {} {}\n".format( idx, assec, val ) );
        idx+=1;

exit(0);


#for x in range( 10 ):
#timesec = get_next_f64_time_from_filehandle( vidtimes );
#    print( "[{}]: {}".format( x, timesec ) );


