

INDIR=$1


dbfiles=`find $INDIR -name "*.db";`

for f in $dbfiles;
do
    echo "Vaccuuming: $f"
    sqlite3 $f vacuum &
done

echo "Waiting VACUUM"
wait
echo "Finished VACUUM"
