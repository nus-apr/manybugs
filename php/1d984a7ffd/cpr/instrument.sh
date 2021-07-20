script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 4 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
fix_id=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
dir_name=/data/$benchmark_name/$project_name/$fix_id

mkdir $dir_name/cpr
cd $dir_name/src
make clean

if [ ! -f "$dir_name/src/INSTRUMENTED_CPR" ]; then
    touch "$dir_name/src/INSTRUMENTED_CPR"
fi

## Prepare for KLEE

if [ -e configured.mark ]; then
    echo "[php-transform] Already configured"

    # Makefile
    sed -i 's/all_targets = $(OVERALL_TARGET) $(PHP_MODULES) $(PHP_ZEND_EX) $(PHP_BINARIES) pharcmd/all_targets = $(OVERALL_TARGET) $(PHP_MODULES) $(PHP_ZEND_EX) $(PHP_BINARIES)/' ./Makefile
    sed -i 's/PHP_BINARIES = cli cgi/PHP_BINARIES = cli/' ./Makefile

    exit 0
fi

aux=$(readlink -f "/experiments/benchmark/manybugs/php/.aux")
main_c_appendix=$(readlink -f "/experiments/benchmark/manybugs/php/.aux/main/main.c.appendix")
php_h_appendix=$(readlink -f "/experiments/benchmark/manybugs/php/.aux/main/php.h.appendix")
test_univ=$(readlink -f "/experiments/benchmark/manybugs/php/.aux/TEST_UNIV_FULL")
# extend main.c
cp $main_c_appendix ./main/
cat ./main/main.c ./main/main.c.appendix > ./main/main.c.merge
cp ./main/main.c ./main/main.c.bak
cp ./main/main.c.merge ./main/main.c
$aux/get_test_script_file.awk $test_univ >> ./main/main.c

# extend php.h
cp $php_h_appendix ./main/
cp ./main/php.h ./main/php.h.bak
cat ./main/php.h ./main/php.h.appendix > ./main/php.h.merge
cp ./main/php.h.merge ./main/php.h

#sed -i "21i #define FD_ZERO_SIMUL(set) memset(set, 0, sizeof(*(set)))" main/php.h

files=$(grep -rl "FD_ZERO(" --include=*.c) || true
for file in $files; do
    sed -i 's/FD_ZERO(/FD_ZERO_SIMUL(/g' $file
done

files=$(grep -rl "(char \*)gnu_get_libc_version()" --include=*.c) || true
for file in $files; do
    sed -i 's/(char \*)gnu_get_libc_version()/\"2.27\"/g' $file
done

files=$(grep -rl "# define XPFPA_HAVE_CW 1" --include=*.h) || true
for file in $files; do
    sed -i 's/# define XPFPA_HAVE_CW 1//g' $file
done

files=$(grep -rl "#define HAVE_MMAP 1" --include=*.h) || true
for file in $files; do
    sed -i 's/#define HAVE_MMAP 1//g' $file
done

# php_crypt_r.c
sed -i 's/#elif (defined(__GNUC__) \&\& (__GNUC__ >= 4 \&\& __GNUC_MINOR__ >= 2))/#elif defined(AF_KEEP_ORG) \&\& (defined(__GNUC__) \&\& (__GNUC__ >= 4 \&\& __GNUC_MINOR__ >= 2))/g' ./ext/standard/php_crypt_r.c
sed -i 's/#elif (defined(__GNUC__) \&\& (__GNUC__ >= 4 \&\& __GNUC_MINOR__ >= 1))/#elif defined(AF_KEEP_ORG) \&\& (defined(__GNUC__) \&\& (__GNUC__ >= 4 \&\& __GNUC_MINOR__ >= 1))/g' ./ext/standard/php_crypt_r.c
sed -i 's/#elif defined(HAVE_ATOMIC_H)/#elif defined(AF_KEEP_ORG) \&\& defined(HAVE_ATOMIC_H)/g' ./ext/standard/php_crypt_r.c

# zend_alloc.c
sed -i 's/#if defined(__GNUC__) && defined(i386)/#if defined(AF_KEEP_ORG) \&\& defined(__GNUC__) \&\& defined(i386)/g' ./Zend/zend_alloc.c
sed -i 's/#elif defined(__GNUC__) && defined(__x86_64__)/#elif defined(AF_KEEP_ORG) \&\& defined(__GNUC__) \&\& defined(__x86_64__)/g' ./Zend/zend_alloc.c
sed -i 's/#elif defined(_MSC_VER) && defined(_M_IX86)/#elif defined(AF_KEEP_ORG) \&\& defined(_MSC_VER) \&\& defined(_M_IX86)/g' ./Zend/zend_alloc.c

# zend.h
sed -i 's/# define EXPECTED(condition)   __builtin_expect(condition, 1)/# define EXPECTED(condition)   (__builtin_expect(condition, 1))/g' ./Zend/zend.h
sed -i 's/# define UNEXPECTED(condition) __builtin_expect(condition, 0)/# define UNEXPECTED(condition) (__builtin_expect(condition, 0))/g' ./Zend/zend.h


# Makefile
sed -i 's/all_targets = $(OVERALL_TARGET) $(PHP_MODULES) $(PHP_ZEND_EX) $(PHP_BINARIES) pharcmd/all_targets = $(OVERALL_TARGET) $(PHP_MODULES) $(PHP_ZEND_EX) $(PHP_BINARIES)/' ./Makefile
sed -i 's/PHP_BINARIES = cli cgi/PHP_BINARIES = cli/' ./Makefile

touch configured.mark

#
## Remove inline ASM
#sed -i "667d" Zend/zend_alloc.c
#sed -i "667i #if defined(__GNUC__) && defined(i386) && 0" Zend/zend_alloc.c
#sed -i "672d" Zend/zend_alloc.c
#sed -i "672i #elif defined(__GNUC__) && defined(__x86_64__) && 0" Zend/zend_alloc.c
#sed -i "677d" Zend/zend_alloc.c
#sed -i "677i #elif defined(_MSC_VER) && defined(_M_IX86) && 0" Zend/zend_alloc.c
#sed -i "693d" Zend/zend_alloc.c
#sed -i "693i #if defined(__GNUC__) && defined(i386) && 0" Zend/zend_alloc.c
#sed -i "698d" Zend/zend_alloc.c
#sed -i "698i #elif defined(__GNUC__) && defined(__x86_64__) && 0" Zend/zend_alloc.c
#sed -i "703d" Zend/zend_alloc.c
#sed -i "703i #elif defined(_MSC_VER) && defined(_M_IX86) && 0" Zend/zend_alloc.c
#sed -i "2445d" Zend/zend_alloc.c
#sed -i "2445i #if defined(__GNUC__) && defined(i386) && 0" Zend/zend_alloc.c
#sed -i "2485d" Zend/zend_alloc.c
#sed -i "2485i #elif SIZEOF_SIZE_T == 4 && defined(HAVE_ZEND_LONG64) && 0" Zend/zend_alloc.c
#
#
#
#sed -i 's#\_\_asm\_\_ \(.*\)\;#\ #g' Zend/zend_float.h
#sed -i 's#\_\_asm\_\_\(.*\)#\ #g' Zend/zend_execute.c
#sed -i '26,29d' Zend/zend_multiply.h
#sed -i '2472,2476d' Zend/zend_alloc.c
#sed -i '2452,2456d' Zend/zend_alloc.c
#sed -i 's#\_\_asm\_\_\(.*\)\;#\ #g' Zend/zend_alloc.c
#sed -i 's#\_\_asm\_\_ \(.*\))\;#\ #g' sapi/fpm/fpm/fpm_atomic.h
#sed -i "36,37d" sapi/fpm/fpm/fpm_atomic.h
#sed -i "36i add = add + (atomic_int_t)*value;\n" sapi/fpm/fpm/fpm_atomic.h
#sed -i "47,48d" sapi/fpm/fpm/fpm_atomic.h
#sed -i "47i res = __sync_bool_compare_and_swap(lock,old,set);\n" sapi/fpm/fpm/fpm_atomic.h
#sed -i "62,63d" sapi/fpm/fpm/fpm_atomic.h
#sed -i "62i add = add + (atomic_int_t)*value;\n" sapi/fpm/fpm/fpm_atomic.h
#sed -i "73,74d" sapi/fpm/fpm/fpm_atomic.h
#sed -i "73i res = __sync_bool_compare_and_swap(lock,old,set);\n" sapi/fpm/fpm/fpm_atomic.h
#sed -i "60285,60291d" ext/sqlite3/libsqlite/sqlite3.c
#sed -i "29499,29505d" ext/sqlite3/libsqlite/sqlite3.c
#sed -i "23152,23158d" ext/sqlite3/libsqlite/sqlite3.c
#sed -i "21622,21628d" ext/sqlite3/libsqlite/sqlite3.c
#sed -i 's#\_\_asm\_\_ \(.*\)\;#\ #g' ext/sqlite3/libsqlite/sqlite3.c
#sed -i "379d" Zend/zend_float.h
#sed -i "379i  _FPU_SETCW(_xpfpa_fpu_oldcw)" Zend/zend_float.h


CC=wllvm CXX=wllvm++ CFLAGS="-g -O0 -DHAVE_FPU_INLINE_ASM_X86=0" CXXFLAGS="-g -O0 -DHAVE_FPU_INLINE_ASM_X86=0"  make  -j32
cd $dir_name/src/sapi/cli
extract-bc php
llvm-dis php.bc
line=$(grep -n "declare double @llvm.fabs.f64(double)" php.ll | cut -d ":" -f 1)
sed -i "$line d" php.ll
sed -i "$line i }" php.ll
sed -i "$line i ret double %11" php.ll
sed -i "$line i %11 = phi double [ %6, %5 ], [ %9, %7 ]" php.ll
sed -i "$line i ; <label>:10:                                      ; preds = %7, %5" php.ll
sed -i "$line i br label %10" php.ll
sed -i "$line i %9 = fsub double -0.000000e+00, %8"  php.ll
sed -i "$line i %8 = load double, double* %2, align 8" php.ll
sed -i "$line i ; <label>:7:                                      ; preds = %1" php.ll
sed -i "$line i br label %10" php.ll
sed -i "$line i %6 = load double, double* %2, align 8" php.ll
sed -i "$line i ; <label>:5:                                      ; preds = %1" php.ll
sed -i "$line i br i1 %4, label %5, label %7" php.ll
sed -i "$line i %4 = fcmp ogt double %3, 0.000000e+00" php.ll
sed -i "$line i %3 = load double, double* %2, align 8" php.ll
sed -i "$line i store double %0, double* %2, align 8" php.ll
sed -i "$line i %2 = alloca double, align 8" php.ll
sed -i "$line i define double @fabs_f64(double) #0 {" php.ll
sed -i 's#\@llvm.fabs.f64#\@fabs_f64#g' php.ll
llvm-as php.ll


cd $dir_name/src
#Instrument for test-case
sed -i '20i // KLEE' ext/tokenizer/tokenizer.c
sed -i '21i #include <klee/klee.h>' ext/tokenizer/tokenizer.c
sed -i '22i #ifndef TRIDENT_OUTPUT' ext/tokenizer/tokenizer.c
sed -i '23i #define TRIDENT_OUTPUT(id, typestr, value) value' ext/tokenizer/tokenizer.c
sed -i '24i #endif' ext/tokenizer/tokenizer.c
sed -i '159i \\tklee_assert(token_type - T_HALT_COMPILER != 0);' ext/tokenizer/tokenizer.c
sed -i '159i \\tTRIDENT_OUTPUT("obs", "i32", token_type - T_HALT_COMPILER);' ext/tokenizer/tokenizer.c
sed -i '159i \\tif ( __trident_choice("L154", "bool", (int[]){token_type, T_HALT_COMPILER, zendleng}, (char*[]){"x", "y", "z"}, 3, (int*[]){}, (char*[]){}, 0)) break' ext/tokenizer/tokenizer.c


# Compile instrumentation and test driver.
make CXX=$TRIDENT_CXX CC=$TRIDENT_CC  CFLAGS="-L/CPR/lib -ltrident_proxy -L/klee/build/lib  -lkleeRuntest -I/klee/source/include -g -O0" -j32




cat <<EOF > $dir_name/cpr/repair.conf
project_path:/data/$benchmark_name/$project_name/$fix_id
tag_id:$fix_id
src_directory:src
config_command:skip
build_command:skip
binary_path:sapi/cli/php
custom_comp_list:cpr/components/x.smt2,cpr/components/y.smt2,cpr/components/z.smt2,components/constant_a.smt2
general_comp_list:equal.smt2,not-equal.smt2,less-than.smt2,less-or-equal.smt2
depth:3
loc_patch:/data/$benchmark_name/$project_name/$fix_id/src/ext/tokenizer/tokenizer.c:159
loc_bug:/data/$benchmark_name/$project_name/$fix_id/src/ext/tokenizer/tokenizer.c:160
gen_limit:80
stack_size:15000
dist_metric:angelic
spec_path:cpr/spec.smt2
seed_file:cpr/seed-input
test_input_file:cpr/test-input
test_output_list:cpr/expected-output/t1.smt2,cpr/expected-output/t2.smt2
mask_arg:$(seq -s "," -f "%g" 0 52)
EOF


# Create patch components
mkdir $dir_name/cpr/components
declare -a arr_var=("x" "y" "z")
declare -a arr_const=("constant_a")
# Create components for program variables
for i in "${arr_var[@]}"
do
cat <<EOF > $dir_name/cpr/components/$i.smt2
(declare-const rvalue_$i (_ BitVec 32))
(declare-const lvalue_$i (_ BitVec 32))
(declare-const rreturn (_ BitVec 32))
(declare-const lreturn (_ BitVec 32))
(assert (and (= rreturn rvalue_$i) (= lreturn lvalue_$i)))
EOF
done

# Create components for constants
for i in "${arr_const[@]}"
do
cp /CPR/components/$i.smt2 $dir_name/cpr/components
done


# Create seed configuration
touch $dir_name/cpr/seed-input
while IFS= read -r line
do
  sed -i "1i \$POC_$line" $dir_name/cpr/seed-input
done < $dir_name/tests.all.txt.rev

# Create test configuration
touch $dir_name/cpr/test-input
while IFS= read -r line
do
  sed -i "1i \$POC_$line" $dir_name/cpr/test-input
done < $dir_name/failing.tests.txt


# Copy remaining files to run CPR.
cp $script_dir/spec.smt2 $dir_name/cpr
cp -rf $script_dir/test-input-files $dir_name/cpr
cp -rf $script_dir/test-expected-output $dir_name/cpr
cp $script_dir/test-config.json $dir_name/cpr