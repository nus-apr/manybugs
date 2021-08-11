#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
bug_id=$(echo $script_dir | rev | cut -d "/" -f 1 | rev)
dir_name=/data/$benchmark_name/$project_name/$bug_id
cd $dir_name/src

# Compile gzip
make -e  CFLAGS="-g -O0 -static" -j`nproc`

cd tests
make mixed.log znew-k.log
chmod +x mixed
chmod +x znew-k