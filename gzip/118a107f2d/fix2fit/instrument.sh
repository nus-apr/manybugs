script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 4 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
fix_id=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
src_dir_name=/data/$benchmark_name/$project_name/$fix_id/src
test_dir_name=/data/$benchmark_name/$project_name/$fix_id/tests
target_dir=/experiments/benchmark/$benchmark_name/$project_name/$fix_id
cd $test_dir_name

# Copy Seed Files
mkdir $target_dir/seed-dir
find . -type f -iname '*.gz' -exec cp  {} $target_dir/seed-dir/ \;

cd $src_dir_name
make clean

if [ ! -f "$src_dir_name/INSTRUMENTED_FIX2FIT" ]; then
    touch "$src_dir_name/INSTRUMENTED_FIX2FIT"
fi