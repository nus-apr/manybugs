script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
bug_id=$(echo $script_dir | rev | cut -d "/" -f 1 | rev)
dir_name=/data/$benchmark_name/$project_name/$bug_id
scenario_id=libtiff-bug-2010-11-27-eb326f9-eec7ec0
cd $dir_name


## fix the test harness and the configuration script
sed -i "s#/root/mountpoint-genprog/genprog-many-bugs/${scenario_id}#/data/manybugs/libtiff/${bug_id}#g" test.sh
sed -i "s#/data/manybugs/libtiff/${bug_id}/limit#timeout 5#g" test.sh
sed -i "s#/usr/bin/perl#perl#g" test.sh
sed -i "s#cd libtiff#cd src#g" test.sh

# Run passing test cases
for i in `seq -s " " -f "p%g"  1 76`
do
bash test.sh $i /data
done

# Run failing test cases
for i in `seq -s " " -f "n%g"  1 1`
do
bash test.sh $i /data
done