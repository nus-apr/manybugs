#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
bug_id=$(echo $script_dir | rev | cut -d "/" -f 1 | rev)
dir_name=$1/$benchmark_name/$project_name/$bug_id
scenario_id=php-bug-2011-10-15-0a1cc5f01c-05c5c8958e
cd $dir_name
TEST_ID=$2
POS_N=7728
NEG_N=1



if [ -z "$TEST_ID" ]
then
   # Run passing test cases
  for i in `seq -s " " -f "p%g"  1 $POS_N`
  do
  bash test.sh $i /data
  done

  # Run failing test cases
  for i in `seq -s " " -f "n%g"  1 $NEG_N`
  do
  bash test.sh $i /data
  done
else
    timeout 10 bash test.sh $TEST_ID
fi