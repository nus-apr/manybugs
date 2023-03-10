#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
fix_id=$(echo $script_dir | rev | cut -d "/" -f 1 | rev)
dir_name=$1/$benchmark_name/$project_name/$fix_id
scenario_id=lighttpd-bug-1948-1949
diff_file=src/response.c-1948
bug_id=$(echo $scenario_id | rev | cut -d "-" -f 2 | rev)
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
        *~ \
        reconfigure \
        fixed-program.txt
mv bugged-program.txt manifest.txt
mv *.lines bug-info
mv fix-failures bug-info
mv $project_name src
cd $dir_name/src
cp $dir_name/diffs/${diff_file} $dir_name/src/$(echo $diff_file| cut -d'-' -f 1)
chown -R root $dir_name


# Prophet requires/works on git source
cd $dir_name
svn checkout -r $bug_id svn://svn.lighttpd.net/lighttpd/branches/lighttpd-1.4.x src-svn

cd $dir_name

## fix the test harness and the profile script
sed -i "s#/root/mountpoint-genprog/genprog-many-bugs/${scenario_id}#${dir_name}#g" test.sh
sed -i "s#${dir_name}/limit#timeout 5#g" test.sh
sed -i "s#/usr/bin/perl#perl#g" test.sh
sed -i 's#lt-\.\*#lt-\.\* \&\> /dev/null#g' test.sh
sed -i "s#cd ${project_name}#pushd ${dir_name}/src#g" test.sh
sed -i 's#cd ../../#popd#g' test.sh
sed -i "27,41d" test.sh
sed -i "s#run_test 2 #run_test 15 #g" test.sh
sed -i "s#run_test 1 #run_test 2 #g" test.sh


# fix an obnoxious bug in tests/core-request.t
sed -i 's#image.JPG#image.jpg#g' src/tests/core-request.t
sed -i '49,71 s/^/#/' src/tests/mod-cgi.t

# fix broken symlinks
cd src/tests/tmp/lighttpd/servers/www.example.org/pages
rm symlinked index.xhtml
ln -s expire symlinked
ln -s index.html index.xhtml

cd $dir_name
# fix test-case-id
cp src/tests/mod-rewrite.t src/tests/2.t
cp src/tests/fastcgi.t src/tests/15.t
cp src/tests/request.t src/tests/19.t
