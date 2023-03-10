#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
bug_id=$(echo $script_dir | rev | cut -d "/" -f 1 | rev)
dir_name=$1/$benchmark_name/$project_name/$bug_id


SRC_FILE=$dir_name/src/gzip.c
TRANS_FILE=$script_dir/valkyrie/gzip.c
ANNOTATE_SCRIPT=$script_dir/../../../../scripts/transform/annotate.py
MERGE_SCRIPT=$script_dir/../../../../scripts/transform/merge.py

if [[ ! -f $TRANS_FILE ]]; then
  mkdir -p $(dirname $TRANS_FILE)
  clang-tidy $SRC_FILE -fix -checks="readability-braces-around-statements"
  clang-format -style=LLVM  $SRC_FILE > $TRANS_FILE
  cp $TRANS_FILE $SRC_FILE
  clang -Xclang -ast-dump=json $SRC_FILE > $TRANS_FILE.ast
  tr --delete '\n' <  $TRANS_FILE.ast  >  $TRANS_FILE.ast.single
  # check for multi-line if condition / for condition  / while condition
  python3 $MERGE_SCRIPT $TRANS_FILE $TRANS_FILE.ast.single
  mv merged.c $TRANS_FILE
  cp $TRANS_FILE $SRC_FILE
  clang -Xclang -ast-dump=json $SRC_FILE > $TRANS_FILE.ast.merged
  tr --delete '\n' <  $TRANS_FILE.ast.merged  >  $TRANS_FILE.ast.merged.single
  python3 $ANNOTATE_SCRIPT $TRANS_FILE $TRANS_FILE.ast.merged.single
  mv annotated.c $TRANS_FILE
fi

cp  $TRANS_FILE $SRC_FILE
bash build.sh $1



#copy shell-scripts
cp $dir_name/src/zdiff test-suite
cp $dir_name/src/znew test-suite
cp $dir_name/src/zless test-suite
cp $dir_name/src/zmore test-suite
cp $dir_name/src/zfgrep test-suite
cp $dir_name/src/zgrep test-suite
cp $dir_name/src/zcat test-suite
cp $dir_name/src/zcmp test-suite


# copy binary executables
#find -type f -executable -exec file -i '{}' \; | grep 'charset=binary' | grep -v "shellscript" | awk '{print $1}'
cp -f $dir_name/src/gzip test-suite/gzip.orig

# copy test cases
cp -rf $dir_name/src/tests test-suite
find $script_dir/test-suite -type d -exec chmod 775 {} \;

# update path for test case
sed -i 's#/experiment//manybugs/gzip/$bug_id/src/tests#/tmp#g' test-suite/tests/hufts

cd $script_dir/test-suite/tests
script_list=($(find -type f -executable -exec file -i '{}' \; |grep  "shellscript" |awk '{print $1}'))
for i in "${script_list[@]}"
do
  script_path=${i::-1}
  sed -i "s#out#\${PATCH_ID:+\$PATCH_ID-}out#g" $script_path
done


