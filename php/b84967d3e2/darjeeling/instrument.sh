#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
benchmark_name=$(echo $script_dir | rev | cut -d "/" -f 4 | rev)
project_name=$(echo $script_dir | rev | cut -d "/" -f 3 | rev)
bug_id=$(echo $script_dir | rev | cut -d "/" -f 2 | rev)
dir_name=$1/$benchmark_name/$project_name/$bug_id
exp_dir_path=/experiment/$benchmark_name/$project_name/$bug_id
setup_dir_path=/setup/$benchmark_name/$project_name/$bug_id
fix_file=$2

POS_N=7592
NEG_N=1



cat <<EOF > darjeeling-driver
#!/bin/bash
$setup_dir_path/test.sh /experiment \$1
EOF
chmod +x darjeeling-driver

cat <<EOF > Dockerfile
FROM rshariffdeen/cerberus:darjeeling
USER root
RUN mkdir -p /setup/$benchmark_name/$project_name/$bug_id
COPY . $setup_dir_path
COPY darjeeling/darjeeling-driver $setup_dir_path/darjeeling/test.sh
RUN /setup/$benchmark_name/$project_name/$bug_id/setup.sh /experiment
WORKDIR /experiment
EOF

cd $script_dir/..
tag_id=$(echo "$bug_id" | awk '{print tolower($0)}')
docker build -t $tag_id -f darjeeling/Dockerfile .

cat <<EOF > $dir_name/src/repair.yml
algorithm:
  type: exhaustive
coverage:
  method:
    type: gcov
localization:
  type: spectrum-based
  metric: tarantula
  restrict-to-files:
  - $fix_file
optimizations:
  ignore-dead-code: true
  ignore-equivalent-insertions: true
  ignore-string-equivalent-snippets: true
program:
  build-instructions:
    steps:
    - command: ./config.sh /experiment
      directory: $setup_dir_path
    - command: ./build.sh /experiment
      directory: $setup_dir_path
    steps-for-coverage:
    - command: CFLAGS="--coverage " CXXFLAGS="--coverage "  LDFLAGS="--coverage " ./config.sh /experiment
      directory: $setup_dir_path
    - command: CFLAGS="--coverage " CXXFLAGS="--coverage " LDFLAGS="--coverage " ./build.sh /experiment
      directory: $setup_dir_path
    time-limit: 30
  image: $tag_id
  language: c
  source-directory: $exp_dir_path/src
  tests:
    tests:
    type: genprog
    number-of-failing-tests: $POS_N
    number-of-passing-tests: $NEG_N
    time-limit: 10
    workdir: $setup_dir_path/darjeeling
resource-limits:
  candidates: 100000
seed: 0
threads: 1
transformations:
  schemas:
  - type: delete-statement
  - type: append-statement
  - type: prepend-statement
  - type: replace-statement
version: 1.0
EOF

