#!/bin/bash

# Variables
bold=$(tput bold)
normal=$(tput sgr0)
scriptpath="$( cd "$(dirname "$0")" ; pwd -P )"
projectpath=${scriptpath}/..

# Functions
echob() {
    echo "${bold}${1}${normal}"
}

echob "Running style script:"
${scriptpath}/clang-format.sh
qmlformat -i ${projectpath}/qml/*.qml
if ! git diff --quiet --ignore-submodules HEAD 2>/dev/null; then
    echob "Style error ! Run clang-format in tools. ($ ./tools/clang-format.sh)"
    git diff | cat
    exit 1
fi

echob "Running doxygen script:"
${scriptpath}/testdoxygen.sh || exit 1

echob "Compile code in test mode:"
build_test="$projectpath/build_test"
rm -rf $build_test
mkdir -p ${build_test}
cmake -B ${build_test} -DCMAKE_BUILD_TYPE=Debug && cmake --build ${build_test} --parallel --config Debug
xvfb-run --server-args="-screen 0 1024x768x24" $build_test/test || exit 1

echob "Do runtime test:"
${scriptpath}/compile.sh --autokill --no-deploy --debug || exit 1
export DISPLAY=:99.0
build_folder="/tmp/build"
xvfb-run --server-args="-screen 0 1024x768x24" ${build_folder}/pingviewer || exit 1
echob "Checking for warning messages.."
last_log=$(ls -dt ~/Documents/PingViewer/Gui_Log/* | head -n1)
if grep "\[Warning\]" $last_log; then
    echob "Please fix the warning messages!"
    # Qt 5.15 has some internal warnings, disabling this test for now
    # exit 1
fi

echob "Do ping360 speed test:"
${scriptpath}/compile.sh --ping360speedtest --no-deploy --debug || exit 1
export DISPLAY=:99.0
build_folder="$projectpath/build"
xvfb-run --server-args="-screen 0 1024x768x24" ${build_folder}/pingviewer || exit 1
