#!/bin/bash
set -euo pipefail
version=1a085b1446-118a107f2d #this is the angelix version
gold_file=gzip.c-118a107f2d
echo " --test-timeout 50  " > /tmp/ANGELIX_ARGS
test_array=( "helin-segv" "help-version" "hufts" "mixed" "null-suffix-clobber" "stdin" "trailing-nul" )
clean-source () {
    local directory="$1"
    pushd "$directory" &> /dev/null
    find . -name .svn -exec rm -rf {} \; &> /dev/null || true
    find . -name .git -exec rm -rf {} \; &> /dev/null || true
    find . -name .hg -exec rm -rf {} \; &> /dev/null || true
    ./configure &> /dev/null || true
    make clean &> /dev/null || true
    make distclean &> /dev/null || true
    popd &> /dev/null
}
# Common functions -------------------------------------------------------------

# Common functions -------------------------------------------------------------

# replaces within "$3" lines starting from the only occurrence of symbol "$2" in file "$1"
replace-in-range () {
    local file="$1"
    local symbol="$2"
    local length="$3"
    local original="$4"
    local replacement="$5"

    local begin=$(grep -n "$symbol" "$file" | cut -d : -f 1)
    local end=$(( begin + length ))

    sed -i "$begin,$end{s/$original/$replacement/;}" "$file"
}

add-header () {
    local file="$1"
    sed -i '1s/^/#ifndef ANGELIX_OUTPUT\n/' "$file"
    sed -i '2s/^/#define ANGELIX_OUTPUT(type, expr, id) expr\n/' "$file"
    sed -i '3s/^/#define ANGELIX_REACHABLE(id)\n/' "$file"
    sed -i '4s/^/#endif\n/' "$file"
}

restore_original () {
    local src="$1"
    if [ -e $src.org ]; then
        # restore the original
        cp $src.org $src
    else
        # prepare the org file
        cp $src $src.org
    fi
}

# Gzip ----------------------------------------------------------------------

add-angelix-runner-simple () {
    local script="$1"
    local call="$2"

    local line=$(grep -n -v "^\s*[#;]" "$script" | grep -v "echo" | grep "$call" | cut -d: -f 1)
    #echo "line:$line::script: $script"
    sed -i "$line"'s/\.\.\/gzip/${ANGELIX_RUN:-eval} \.\.\/gzip/' "$script"
}

add-angelix-runner () {
    local script="$1"
    local call="$2"

    local line=$(grep -n -v "^\s*[#;]" "$script" | grep -v "echo" | grep "$call" | grep -e "-d" | cut -d: -f 1)
    #echo "line:$line::script: $script"
    sed -i "$line"'s/\.\.\/gzip/exe() { "$@"; }; ${ANGELIX_RUN:-exe} \.\.\/gzip/' "$script"
    #sed -i "s/-S ''/-S \\\'\\\'/" "$script"
}

prepare-angelix-runner () {
    local script="$1"

    #    local line=$(grep -n "init" "$script")
    sed -i '/init/a rm -rf err && rm -rf out;' "$script"
    sed -i 's/^\./#\./' "$script"
    sed -i 's/gzip/\.\.\/gzip/' "$script"
    sed -i 's/\.\.\/gzip:/gzip:/' "$script"
    sed -i 's/^Exit/exit/' "$script"
    sed -i 's/framework_failure_/exit 99/' "$script"

    sed -i 's/compare/diff/' "$script"

    if [[ $script == *"trailing-nul" ]]; then
      sed -i '23i rm -f 0 00 1' $script
      sed -i "38i if [ \$fail -ne 0 ]; then" $script
      sed -i "39i echo 'FAIL:'" $script
      sed -i "40i fi" $script
    fi

    local kline=$(grep -n "gzip" "$script")
    #echo "kline:$kline"
}

preinstrument_test() {
    directory="$1"
    for t in "${test_array[@]}"
    do
        local test_script="$directory/tests/$t"
        prepare-angelix-runner "$test_script"
    done
}


instrument_test() {
    directory="$1"
    for t in "${test_array[@]}"
    do
        local test_script="$directory/tests/$t"
        if [[ "$t" == *"version"* ]]
        then
            add-angelix-runner-simple "$test_script" gzip
        else
            add-angelix-runner "$test_script" gzip

        fi
    done
}

instrument_source () {
    local directory="$1"
    if [[ "$version" == *"a1d3d4019ddd22"* ]]
    then
        instrument_test "$directory"
        #  local test7_script="$directory/tests/stdin"
        # add-angelix-runner "$test7_script" gzip
        local gzip_write="$directory/bits.c"
        local gzip_main="$directory/gzip.c"

        #    if ! grep -q ANGELIX "$gzip_write"; then
        add-header "$gzip_write"
        sed -i 's/put_byte(bi_buf)/put_byte(ANGELIX_OUTPUT(int, bi_buf, "stdout"))/g' "$gzip_write"
        sed -i '653iifd =ifd;' "$gzip_main"
        #   fi
    else
        #  local test6_script="$directory/tests/null-suffix-clobber"
        #  add-angelix-runner "$test6_script" gzip
        instrument_test "$directory"
        local gzip_error="$directory/gzip.c"

        add-header "$gzip_error"
        # sed -i '/incorrect suffix/i ANGELIX_REACHABLE("stderr");' "$gzip_error"
        sed -i '/incorrect suffix/i ANGELIX_REACHABLE("incorrect_suffix");' "$gzip_error"
        # sed -i '/invalid suffix/i ANGELIX_REACHABLE("stderr");' "$gzip_error"
        sed -i '/invalid suffix/i ANGELIX_REACHABLE("incorrect_suffix");' "$gzip_error"
        #sed -i '/work = lzw/i ANGELIX_REACHABLE("stderr2");' "$gzip_error"
        sed -i "11,15d"  "$gzip_error"

    fi
}

gzip_test_suite=$(seq 1 12)

preinstrument(){
    local directory="$1"
    local is_golden="$2"
    preinstrument_test "$directory"

    case $version in
        3eb6091d69a-884ef6d16c6 )
            if [[ "$is_golden" ==  *"notgolden"* ]]; then
                local test6_script="$directory/tests/null-suffix-clobber"
                sed -i "s/invalid suffix/incorrect suffix/" "$test6_script"
            fi
            ;;
    esac

    # if [[ "$version" == *"3eb6091d69a"* ]]
    # then
    #    if [[ "$is_golden" ==  *"notgolden"* ]]
    #     then
    #        #local test6_script="$directory/tests/null-suffix-clobber"
    #        #sed -i "s/invalid suffix/incorrect suffix/" "$test6_script"
    #        #else
    #        #   cd "$directory/"
    #        #   ./configure &>/dev/null && make clean &>/dev/null
    #        #     cd "$origDIR"
    #     fi
    # fi
    #sed -i 's/5.5/-/g' $directory/bootstrap.conf
    #echo "before boostrap"
    #cd "$directory/"
    #bash "./bootstrap"

    sed -i '/gets is/d' "$directory/lib/stdio.in.h"
    sed -i 's/: O_BINARY/: 0/' "$directory/gzip.c"
    sed -i 's/S_IRWXUGO/00777/' "$directory/gzip.c"
    sed -i 3324's/signbit (arg)/arg < 0.0L/' "$directory/lib/vasnprintf.c"


    #cd "$directory/"
    #./configure &>/dev/null && make clean &>/dev/null
    #cd "$origDIR"
    #echo "after boostrap"
    #echo "after sed"
    #sed -i "s/root\/mountpoint-genprog\/genprog-many-bugs\//$(echo "$origDIR/" | sed -e 's/\\/\\\\/g' -e 's/\//\\\//g' -e 's/&/\\\&/g')/g" "$directory/tests/Makefile"


    instrument_source "$directory"
    touch "$directory/INSTRUMENTED_ANGELIX"


}

# check locale
perl -v |& grep --silent "warning" && echo "Type perl -v and check locale errors" && exit 1

root_directory=$1
buggy_directory="$root_directory/src"
golden_directory="$root_directory/src-gold"

if [ ! -d golden_directory ]; then
  cp -rf $buggy_directory $golden_directory
  cp "$root_directory/diffs/$gold_file" "$golden_directory/$(echo $gold_file| cut -d'-' -f 1)"
fi

if [ ! -d "$root_directory/angelix" ]; then
  mkdir $root_directory/angelix
fi

clean-source $buggy_directory
clean-source $golden_directory

if [ ! -f "$buggy_directory/INSTRUMENTED_ANGELIX" ]; then
   preinstrument $buggy_directory "notgolden"
fi

if [ ! -f "$golden_directory/INSTRUMENTED_ANGELIX" ]; then
    preinstrument $golden_directory "golden"
fi

sed -i 's/make /.\//' "$root_directory/gzip-run-tests.pl"
sed -i 's/\.log//g' "$root_directory/gzip-run-tests.pl"
sed -i '/rm/d' "$root_directory/gzip-run-tests.pl"
sed -i '/kill/d' "$root_directory/gzip-run-tests.pl"
sed -i '/\^PASS/i print "FAIL:$line";' "$root_directory/gzip-run-tests.pl"


run_tests_script=$(readlink -f "$root_directory/gzip-run-tests.pl")

cat <<EOF > $root_directory/angelix/oracle
#!/bin/bash
FILE=/tmp/testo
perl "$run_tests_script" "\$1" &> "\$FILE"
res="\$(cat \$FILE | grep FAIL | wc -l)"
if [[ res -ne 0 ]] ; then
  pwd >> "\$FILE"
  exit 1;
else
  exit 0;
fi
EOF
chmod u+x $root_directory/angelix/oracle

cat <<EOF > $root_directory/angelix/config
#!/bin/bash
rm -f tests/Makefile && echo "\$CC" > /tmp/gzip-cc && ./configure
EOF
chmod +x $root_directory/angelix/config


cat <<EOF > $root_directory/angelix/build
#!/bin/bash
make -e -j`nproc`
EOF
chmod u+x $root_directory/angelix/build
