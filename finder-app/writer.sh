#!/bin/sh

#Example invocation: writer.sh /tmp/aesd/assignment1/sample.txt ios


# check for arguments
if [ $# != 2 ]
then
    echo "Example invocation: writer.sh /tmp/aesd/assignment1/sample.txt ios"
    exit 1
fi

writefile=$1
writestr=$2

# extract directory from file path
dir=$(dirname "$writefile")

# create directory
mkdir -p "$dir"

# check last executed command status with $?
if [ $? -ne 0 ]
then
    echo "Failed to create directory $dir"
    exit 1
fi

# write to file, overwriting. check last executed command with || 
echo "$writestr" > "$writefile" || { echo "Failed to write to file $writefile"; exit 1;}

#echo "check result"
#echo "$writefile"
#cat $writefile