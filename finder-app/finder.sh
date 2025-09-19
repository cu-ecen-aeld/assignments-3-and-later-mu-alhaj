#!/bin/sh

#Example invocation: finder.sh /tmp/aesd/assignment1 linux


# check for arguments
if [ $# != 2 ]
then
    echo "Example invocation: finder.sh /tmp/aesd/assignment1 linux"
    exit 1
fi

filesdir=$1
searchstr=$2

# check for valid args
if [ ! -d "$filesdir" ]
then
    echo "$filesdir is not directory"
    exit 1
fi

#echo "searching for $searchstr in $filesdir "

nr_files=$(find $filesdir -type f | wc -l)
nr_maching_lines=$(grep -r $searchstr $filesdir | wc -l)

echo "The number of files are ${nr_files} and the number of matching lines are ${nr_maching_lines}"