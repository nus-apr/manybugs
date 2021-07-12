script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 4 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
fix_id=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
dir_name=/data/$benchmark_name/$project_name/$fix_id

mkdir $dir_name/prophet

export PHP_AUTOHEADER=/deps/php/autoconf-2.13-build/bin/autoheader PHP_AUTOCONF=/deps/php/autoconf-2.13-build/bin/autoconf
export PATH=/deps/php/bison-2.2-build/bin:$PATH

cat <<EOF > $dir_name/prophet/prophet.conf
revision_file=/data/$benchmark_name/$project_name/$fix_id/prophet/prophet.revlog
src_dir=/data/$benchmark_name/$project_name/$fix_id/src
test_dir=/data/$benchmark_name/$project_name/$fix_id/tests
bugged_file=Zend/zend_compile.c
fixed_out_file=$project_name-fix-$fix_id.c
build_cmd=/prophet-gpl/tools/$project_name-build.py
test_cmd=/prophet-gpl/tools/$project_name-test.py
dep_dir=/prophet-gpl/benchmarks/$project_name-deps
localizer=profile
single_case_timeout=600
EOF

cat <<EOF > $dir_name/prophet/prophet.revlog
-
-
Diff Cases: Tot 1
7
Positive Cases: Tot 4
8 2 3 6
Regression Cases: Tot 0

EOF


