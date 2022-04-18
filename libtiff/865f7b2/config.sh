#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
bug_id=$(echo $script_dir | rev | cut -d "/" -f 1 | rev)
dir_name=$1/$benchmark_name/$project_name/$bug_id
cd $dir_name/src

# Config libtiff.
make clean

PROJECT_CFLAGS="-g -O0 -static"
if [[ -n "${CFLAGS}" ]]; then
  PROJECT_CFLAGS="${PROJECT_CFLAGS} ${CFLAGS}"
fi

PROJECT_CONFIG_OPTIONS="--enable-static --disable-shared \
            --disable-nls                                 \
            --disable-shared                              \
            --disable-cxx                                 \
            --disable-jpeg                                \
            --disable-zlib                                \
            --disable-pixarlog                            \
            --disable-jbig "

if [[ -n "${CONFIG_OPTIONS}" ]]; then
  PROJECT_CONFIG_OPTIONS="${PROJECT_CONFIG_OPTIONS} ${CONFIG_OPTIONS}"
fi

./configure CFLAGS="${PROJECT_CFLAGS}" ${PROJECT_CONFIG_OPTIONS}
