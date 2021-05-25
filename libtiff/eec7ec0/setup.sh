project_name=libtiff
bug_id=eec7ec0
scenario_id=libtiff-bug-2010-11-27-eb326f9-eec7ec0
diff_file=tools/tiff2pdf.c-eb326f9
dir_name=$1/manybugs/$project_name/$bug_id
download_url=https://repairbenchmarks.cs.umass.edu/ManyBugs/scenarios/${scenario_id}.tar.gz
current_dir=$PWD
mkdir -p $dir_name
cd $dir_name
wget $download_url
tar xf ${scenario_id}.tar.gz
mv ${scenario_id} src
rm ${scenario_id}.tar.gz
mv src/* .
rm -rf src
rm -rf  coverage* \
        configuration-oracle \
        local-root \
        limit* \
        *.cache \
        *.debug.* \
        sanity \
        compile.pl \
        *~ \
        test \
        reconfigure \
        preprocessed \
        fixed-program.txt
mv bugged-program.txt manifest.txt
mv *.lines bug-info
mv fix-failures bug-info
mv libtiff src
cd $dir_name/src
cp $dir_name/diffs/${diff_file} $dir_name/src/$(echo $diff_file| cut -d'-' -f 1)
make distclean
chown -R root $dir_name

# Compile libtiff.
make clean
./configure CFLAGS='-g -O0' --enable-static --disable-shared
sed -i '978 s/./\t&/' test/Makefile
make CFLAGS="-march=x86-64" -j32
cd $dir_name

