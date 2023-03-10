#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 4 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
fix_id=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
src_dir_name=$1/$benchmark_name/$project_name/$fix_id/src
target_dir=/experiments/benchmark/$benchmark_name/$project_name/$fix_id
cd $src_dir_name/tests

# Copy Seed Files
mkdir $target_dir/seed-dir
find . -type f -iname '*.phpt' -exec cp  {} $target_dir/seed-dir/ \;

cd $src_dir_name
make clean

if [ ! -f "$src_dir_name/INSTRUMENTED_FIX2FIT" ]; then
    touch "$src_dir_name/INSTRUMENTED_FIX2FIT"
fi
