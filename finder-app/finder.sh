#!/bin/bash
# 

filesdir=$1
searchstr=$2



if [ $# != 2 ]
then
  echo "ERROR: Invalid Number of Arguments."
  echo "Total number of arguments should be 2."
  echo "The order of the arguments should be:"
  echo "  1)File Directory Path."
  echo "  2)String to be searched in the specified directory path."
  exit 1
fi

if [ ! -d "$filesdir" ]
then 
  echo "ERROR: File Directory Path does not exist."
  echo "Please enter a valid directory path to search."
  exit 1
fi

files=$(grep -rl "$searchstr" $filesdir | wc -l)

matches=0
for FILE in $filesdir/*; 
  do echo $FILE;
  filematch=$(grep -c "$searchstr" $FILE)
  matches=$(($matches + $filematch))
done

echo "The number of files are $files and the number of matching lines are $matches"