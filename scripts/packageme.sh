#!/bin/bash
TARFILE=rteye2.tar.gz
URL=rveale.com:~/veale.science/public

tar cfz $TARFILE rteye2
scp $TARFILE $URL
scp rteye2/scripts/install_rteye2.sh $URL
