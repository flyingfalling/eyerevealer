#REV: this is just first step, which adds from CSV to database.

#REV: it saves some parameters into its own table...specifically the units
# of the raw gaze in terms of the original "video" (i.e. stimulus).

import sqlite3
import pandas as pd
import numpy as np
import sys


#REV: will CREATE the DB and overwrite gaze if necessary.
#REV: Note, if I run this on an already created DB with other tables referencing my gaze table it will fuck everything up as ID (index) keys may differ etc...
incsvfname = sys.argv[1];
dbfname = sys.argv[2];

dbconn = sqlite3.connect( dbfname, timeout=6000000 );

#group_export.to_sql(con = db, name = config.table_group_export, if_exists = 'replace', flavor = 'mysql', index = False)
#REV: I need to create TABLE with correct schema and then append to it?
#REV: Problem is I will not know what the IDs are since they will be added on DB side?

gazetable='rawgaze';

sep=',';
gazedf = pd.read_csv( incsvfname, sep=sep );

#REV: I must rename them all to the same names for tobii2, tobii3 etc.? No, just tell what to do each time I use this DB...one thing I know is that the
#REV: read in guy has some time...

#REV: need to tell it TIME and UNITS and DIRECTION and ETC!
