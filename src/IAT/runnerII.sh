#!/bin/bash -x
#
# $Id$
# Copyright 2009 Google Inc. All Rights Reserved
# Author: smohapatra@google.com (Sweta Mohapatra) 
#
# IAT: runnerII.sh - part of IAT runner script
# Move to directory on prod machine where files
# are and remove files for cleanup after
# variables are set

#check for needed tar files
PFMONDIR=$1
COMMAND=$2
cd ${PFMONDIR}

chmod o+x *
if [[ -f allfiles.tar ]]; then
  tar -zxvf allfiles.tar > /dev/null
else
  echo 'allfiles.tar: not found'
  exit 4 
fi

#pfmon loop
INDEXFILE=./executablefiles.txt

if [[ ! -d results/ ]]; then 
  mkdir results/
fi

for EXECUTABLEFILE in $(cat $INDEXFILE); do
  pfmon -e "$COMMAND" ./"${EXECUTABLEFILE}".exe > success.txt
  
  if [[ $? -ne 0 ]]; then
    echo "$COMMAND: command failed." 
    echo "$COMMAND" >> ./results/"$EXECUTABLEFILE"_failedcommands.txt
  else
    cat success.txt >> ./results/"$EXECUTABLEFILE"_results.txt
  fi
done

#tar result files to rsync back
cd ./results
tar -czvf results.tar ./* > /dev/null
mv results.tar ${PFMONDIR}
cd ..

exit 0