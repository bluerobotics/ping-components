#!/bin/bash

# Variables
bold=$(tput bold)
normal=$(tput sgr0)
scriptpath="$( cd "$(dirname "$0")" ; pwd -P )"
projectpath=${scriptpath}/..
buildfolder=/tmp/build
deployfolder=${buildfolder}/deploy
scriptname=$(basename "$0")
define=""
buildtype="Release"
numberofthreads=1
verboseoutput="/tmp/compile_output.txt"

linuxdeployfiles=(
    "${projectpath}/qml/imgs/pingviewer.png"
    "${projectpath}/deploy/pingviewer.desktop"
    "${buildfolder}/pingviewer"
)

autokill=false
clangbuild=false
deploy=true
debug=false
help=false

# Functions
echob() {
    echo "${bold}${1}${normal}"
}

printfb() {
    printf "${bold}${1}${normal}"
}

error() {
    echo -e "ERROR: $*" >&2
    exit 1
}

usage() {
    echo "USAGE: $scriptname --no-deploy, --wich-clang, --debug, --autokill, --help"
}

checktool() {
    name=""
    errormsg=""
    if (( $# > 1 )); then
        name=$2
        if (( $# >= 3 )); then
            errormsg=$3
        fi
    else
        name=( $1 )
    fi
    printfb "$name: "
    eval "$1" >>$verboseoutput 2>&1

    [ $? == 0 ] || {
        echob "✖"
        cat $verboseoutput
        error "$name is not available." $errormsg
    }
    echob "✔"
}

runstep() {
    name=$1
    okmessage=$2
    errormsg=$3
    printfb "$okmessage: "

    # This prints a '.' while the command is evaluated
    sh -c "while sleep 1; do printf '.'; done;" &
    echopid=$!
    eval "$1" >>$verboseoutput 2>&1
    returncode=$?
    kill $echopid
    [ $returncode == 0 ] || {
        echob "✖"
        cat $verboseoutput
        error "$name failed." $errormsg
    }
    echob "✔"
}

# Check for args
for i in "$@"
do
case $i in
    --autokill)
    autokill=true
    define="-DAUTO_KILL=ON"
    buildtype="Debug"
    shift ;;

    --ping360speedtest)
    define="-DPING360_SPEED_TEST=ON"
    buildtype="Debug"
    shift ;;

    --debug)
    debug=true
    buildtype="Debug"
    shift ;;

    --no-deploy)
    deploy=false
    shift ;;

    --with-clang)
    clangbuild=true
    shift ;;

    --help|-h)
    help=true
    shift ;;

    *)
    error "Unknow argument:" $i
    ;;
esac
done

if $help; then
    usage
    exit 1
fi

# Remove last compile output
rm $verboseoutput

echob "Project will be:"
printf "\t- Compiled in "
$debug && echo "Debug mode." || echo "Release mode."

printf "\t- "
$deploy && echo "Deployed." || echo "Not deployed."

$autokill && printf "\t- " && echo "Auto kill enabled ☠."

echo ""

unameout="$(uname -s)"
case "$unameout" in
    Linux*)     machine="Linux 🐧";;
    Darwin*)    machine="Mac 🖥";;
    CYGWIN*)    machine="Cygwin 💻";;
    MINGW*)     machine="MinGw 💻";;
    *)          machine="UNKNOWN:${unameout}"
esac

if [[ $machine != *"Mac"* ]] && [[ $machine != *"Linux"* ]]; then
    error "Machine not compatible: ${machine}"
fi;

echob "Compiling for ${machine}"
echob "Checking for tools..."

checktool "git --version"
checktool "cmake --version"
checktool "python --version"
checktool 'python -c "import jinja2; print(jinja2.__version__);"' jinja2 "Version 2.10+ is necessary."

if [[ $machine = *"Mac"* ]]; then
    # Clang to C++ version: https://clang.llvm.org/cxx_status.html
    # Xcode versions to llvm: https://en.wikipedia.org/wiki/Xcode
    checktool "clang --version"
    numberofthreads=$(($(sysctl -n hw.physicalcpu)*2))
    deployfolder=${buildfolder}/pingviewer.app
fi

if [[ $machine = *"Linux"* ]]; then
    # GCC to C++ version: https://gcc.gnu.org/projects/cxx-status.html#cxx14
    checktool "gcc --version"
    numberofthreads=$(nproc --all)
fi

echo ""
echob "Start to build project.."
echob "Number of threads: ""${numberofthreads}"
runstep "git submodule init" "Init submodule" "Failed to init submodule"
runstep "git submodule update" "Update submodule" "Failed to update submodule"
runstep "rm -rf ${buildfolder}" "Check for old build folder" "Failed to delete old build folder"
runstep "mkdir -p ${buildfolder}" "Build folder created" "Failed to create build folder in ${buildfolder}"

# Build type is necessary for both configuratio and compilation step
# - For the first when using a makefile based system (unix makefiles, nmake makefiles, mingw makefiles)
# - For the second when using multi-configuration build system tools (vscode, xcode)
# Parallel configuration will use the available cpus
runstep "cmake -S ${projectpath} -B ${buildfolder} -DCMAKE_BUILD_TYPE=${buildtype} ${define}" "Configure project" "Project configuration failed"
runstep "cmake --build ${buildfolder} --parallel --config ${buildtype}" "Build project" "Failed to build project"

if [ "$deploy" == "false" ]; then
    echob "Done!"
    exit 0
fi

echo ""
echob "Start to deploy project.."

if [[ $machine = *"Linux"* ]]; then
    runstep "mkdir -p ${deployfolder}" "Deploy folder created" "Failed to create deploy folder in ${buildfolder}"
    for i in "${linuxdeployfiles[@]}"; do
        runstep "cp ${i} ${deployfolder}" "Move file to deploy folder ""${i}" "Failed to deploy file: ""${i}"
    done

    runstep "wget https://github.com/bluerobotics/stm32flash-code/releases/download/continuous/stm32flash_linux.zip -O /tmp/stm32flash_linux.zip" "Download stm32flash_linux" "Faile to download stm32flash_linux"
    runstep "unzip /tmp/stm32flash_linux.zip -d /tmp" "Unzip stm32flash" "Fail to unzip stm32flash"
    runstep "chmod +x /tmp/stm32flash" "Convert stm32flash to executable" "Failed to turn stm32flash in executable"
    runstep "mv /tmp/stm32flash ${deployfolder}" "Move stm32flash to deploy" "Failed to move stm32flash into deploy folder"
    runstep "wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -O /tmp/linuxdeploy.AppImage" "Download linuxdeploy" "Failed to download linuxdeploy"
    runstep "wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -O /tmp/linuxdeploy-plugin-qt.AppImage" "Download linuxdeploy Qt plugin" "Failed to download linuxdeploy Qt plugin"
    runstep "chmod a+x /tmp/linuxdeploy.AppImage" "Convert linuxdeploy to executable" "Failed to turn linuxdeploy in executable"
    runstep "chmod a+x /tmp/linuxdeploy-plugin-qt.AppImage" "Convert linuxdeploy Qt plugin to executable" "Failed to turn linuxdeploy Qt plugin in executable"
    runstep "unset QTDIR; unset QT_PLUGIN_PATH ; unset LD_LIBRARY_PATH;" "Unset some Qt variables" "Failed to unsed Qt variables"
    runstep "export QML_SOURCES_PATHS=${projectpath}/qml" "Set QML_SOURCES_PATHS" "Failed to set QML_SOURCES_PATHS"
    runstep "/tmp/linuxdeploy.AppImage --icon-file=$PWD/qml/imgs/pingviewer.png --desktop-file=${deployfolder}/pingviewer.desktop --executable=${deployfolder}/pingviewer --appdir=${deployfolder} --plugin qt --output appimage --verbosity=3" "Run linuxdeploy" "Failed to run linuxdeploy"
    runstep "mv pingviewer*.AppImage /tmp/pingviewer-x86_64.AppImage" "Move .AppImage folder to /tmp/" "Faile to move .AppImage file"
else
    runstep "wget https://github.com/bluerobotics/stm32flash-code/releases/download/continuous/stm32flash_osx.zip -O /tmp/stm32flash_osx.zip" "Download stm32flash_osx" "Faile to download stm32flash_osx"
    runstep "unzip /tmp/stm32flash_osx.zip  -d /tmp" "Unzip stm32flash" "Fail to unzip stm32flash"
    runstep "chmod +x /tmp/stm32flash" "Convert stm32flash to executable" "Failed to turn stm32flash in executable"
    runstep "mv /tmp/stm32flash ${deployfolder}" "Move stm32flash to deploy" "Failed to move stm32flash into deploy folder"
    runstep "macdeployqt ${deployfolder} -qmldir=${projectpath}/qml -dmg" "Use macdeployqt" "Fail to use macdeployqt"
    runstep "mv ${buildfolder}/pingviewer.dmg /tmp/pingviewer-${buildtype}.dmg" "Move .dmg folder to /tmp/" "Faile to move .dmg file"
fi
