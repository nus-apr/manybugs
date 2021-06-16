script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 4 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
fix_id=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
dir_name=/data/$benchmark_name/$project_name/$fix_id

cd $dir_name/src/test

# Copy Seed Files
mkdir $dir_name/seed-dir
find . -type f -iname '*.tiff' -exec cp  {} $dir_name/seed-dir/ \;
find . -type f -iname '*.bmp' -exec cp  {} $dir_name/seed-dir/ \;
find . -type f -iname '*.gif' -exec cp  {} $dir_name/seed-dir/ \;
find . -type f -iname '*.pgm' -exec cp  {} $dir_name/seed-dir/ \;
find . -type f -iname '*.ppm' -exec cp  {} $dir_name/seed-dir/ \;
find . -type f -iname '*.pbm' -exec cp  {} $dir_name/seed-dir/ \;

cd $dir_name/src

make clean

if [ ! -f "$dir_name/src/INSTRUMENTED_CPR" ]; then
    touch "$dir_name/src/INSTRUMENTED_CPR"
fi


## Prepare for KLEE
# Fix fabs calls (not supported by KLEE).
sed -i 's/fabs/fabs_trident/g' libtiff/tif_luv.c
sed -i 's/fabs/fabs_trident/g' tools/tiff2ps.c
#sed -i 's/fabs_trident/fabs/g' libtiff/tif_luv.c
#sed -i 's/fabs_trident/fabs/g' tools/tiff2ps.c

make CC=$TRIDENT_CC CXX=$TRIDENT_CXX -j32

cd $dir_name

## Instrument Short_TAG
sed -i '43d ' src/test/short_tag.c
sed -i '43i const char      *filename = "short_test.tiff";' src/test/short_tag.c
sed -i '81i main(int argc, char** argv)' src/test/short_tag.c
sed -i '82d' src/test/short_tag.c
sed -i '151i \\tfilename = argv[1];'  src/test/short_tag.c


## Instrument Long_TAG
sed -i '122i \\tfilename = argv[1];'  src/test/long_tag.c


## Instrument libtiff component.
sed -i '33i // KLEE' src/libtiff/tif_dirwrite.c
sed -i '34i #include <klee/klee.h>' src/libtiff/tif_dirwrite.c
sed -i '35i #ifndef TRIDENT_OUTPUT' src/libtiff/tif_dirwrite.c
sed -i '36i #define TRIDENT_OUTPUT(id, typestr, value) value' src/libtiff/tif_dirwrite.c
sed -i '37i #endif' src/libtiff/tif_dirwrite.c

sed -i '810i \\t\t\tklee_print_expr("val=",v[0]);' src/libtiff/tif_dirwrite.c
sed -i '811i \\t\t\tTRIDENT_OUTPUT("obs", "i32", v[0]);' src/libtiff/tif_dirwrite.c

sed -i '343i \\t\t\tif(__trident_choice("L1634", "bool", (int[]){fip->field_tag, TIFFTAG_DOTRANGE}, (char*[]){"x", "y"}, 2, (int*[]){}, (char*[]){}, 0)) {' src/libtiff/tif_dirwrite.c
sed -i '344i \\t\t\t\tgoto bad;' src/libtiff/tif_dirwrite.c
sed -i '345i \\t\t\t} else if (!TIFFWriteNormalTag(tif, dir, fip))' src/libtiff/tif_dirwrite.c
sed -i '346d' src/libtiff/tif_dirwrite.c

cd src
make CXX=$TRIDENT_CXX CC=$TRIDENT_CC CFLAGS="-ltrident_proxy -L/concolic-repair/lib -lkleeRuntest -I/klee/source/include" -j32
cd ./test
make CXX=$TRIDENT_CXX CC=$TRIDENT_CC CFLAGS="-ltrident_proxy -L/concolic-repair/lib -lkleeRuntest -I/klee/source/include" -j32 long_tag.log short_tag.log ascii_tag.log strip_rw.log
extract-bc short_tag

## Copy remaining files to run CPR.
cd $script_dir
cp repair.conf $dir_name
cp spec.smt2 $dir_name
cp -rf components $dir_name
cp -rf test-input-files $dir_name
cp -rf test-expected-output $dir_name
cp $script_dir/seed-dir/* $dir_name/seed-dir
cp $script_dir/test-config.json $dir_name
cp $script_dir/seed-config.json $dir_name

# Convert shell to binary as a driver
cd $dir_name/src/test
sed -i "s/IMG_UNCOMPRESSED/1/g" tiffcp-split.sh
sed -i "s/IMG_UNCOMPRESSED/1/g" tiffcp-split-join.sh
CC=$TRIDENT_CC CXX=$TRIDENT_CXX shc -f tiffcp-split.sh
CC=$TRIDENT_CC CXX=$TRIDENT_CXX shc -f tiffcp-split-join.sh
mv tiffcp-split.sh.x tiffcp-split
mv tiffcp-split-join.sh.x tiffcp-split-join
