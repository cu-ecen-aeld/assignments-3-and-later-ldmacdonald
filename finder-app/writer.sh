#!/bin/bash
# 

writefile=$1
writestr=$2

if [ $# != 2 ]
then
  echo "ERROR: Invalid Number of Arguments."
  echo "Total number of arguments should be 2."
  echo "The order of the arguments should be:"
  echo "  1)The Full Path to the File to Write."
  echo "  2)String to be written into the designated file."
  exit 1
fi

mkdir -p $(dirname $writefile)
echo "$writestr" > $writefile;

if [ $? -eq 1 ]
then
  echo "The file could not be created"
  exit 1
fi