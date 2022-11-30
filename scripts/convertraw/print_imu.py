

import sys
import struct
fn = sys.argv[1];

#REV:  use normal char? E.g. use ascii.. so 'd' for double, etc.
#REV:  tell endianness etc.?
def get_next_u8_time_from_filehandle( f ):
    bytes = f.read(1);
    return struct.unpack('<b', bytes)[0];

def get_next_u64_time_from_filehandle( f ):
    bytes = f.read(8);
    return struct.unpack('<Q', bytes)[0];

def get_next_f32_time_from_filehandle( f ):
    bytes = f.read(4);
    return struct.unpack('<f', bytes)[0];

def get_next_f64_time_from_filehandle( f ):
    bytes = f.read(8);
    return struct.unpack('<d', bytes)[0];


imudata = open( fn, "rb" );

for i in range( 10 ):
    
    x = get_next_f32_time_from_filehandle( imudata );
    y = get_next_f32_time_from_filehandle( imudata );
    z = get_next_f32_time_from_filehandle( imudata );
    print( "[{}]: {}, {}, {}".format( i, x, y, z ) );


