#!/bin/bash

shell_dir=$(cd $(dirname $0); pwd)
build_mode="release"
build_dir=""
cmake_build_type="RelWithDebInfo"
build_args="build"
output_dir="$shell_dir"

function show_usage ()
{
  echo "Usage: $0 [OPTIONS] [TARGET]"
  echo " OPTIONS"
  echo "  -m      Set build mode(release, debug(RelWithDebInfo)); [default: release]"
  echo "  -b path Set build path; [default: <source path>/build_<mode>_<target>]"
  echo "  -? | -h Show this help message and exit"
  echo ""
  echo " TARGET"
  echo "  all     Build and create packages (default)"
  echo "  build   Build only"
  echo ""
  echo " EXAMPLES"
  echo "  $0                    # build only"
  echo "  $0 all                # Build and pack all packages (release mode)"
  echo "  $0 -m debug build     # debug mode build only"
  echo ""
}

function get_options ()
{
  while getopts ":m:b:?h" opt; do
    case $opt in
      m ) build_mode="$OPTARG" ;;
      b ) build_dir="$OPTARG" ;;
      ?|h ) show_usage; exit 1;;
    esac
  done
  shift $(($OPTIND - 1))

  case $build_mode in
    release|debug);;
    *) show_usage; echo "Mode [$build_mode] is not a valid mode" ;;
  esac

  if [ "x$build_dir" = "x" ]; then
    build_dir="$shell_dir/build_${build_mode}"
  fi

  if [ $# -gt 0 ]; then
    build_args="$@"
    echo "[`date +'%F %T'`] Build target [$build_args]"
  fi
}

get_options $@
if [ "$build_mode" = "debug" ]; then
  cmake_build_type="Debug"
else
  cmake_build_type="RelWithDebInfo"
fi

echo "[`date +'%F %T'`] Build mode [$build_mode]"

if [ -d "$build_dir" ]; then
  rm -rf $build_dir
fi

if [ ! -d "$build_dir" ]; then
  mkdir -p $build_dir
fi

cd $build_dir
cmake -DCMAKE_BUILD_TYPE=$cmake_build_type ..
make

if [ "$build_args" = "all" ]; then
  make package
fi

echo "[`date +'%F %T'`] Build type [$cmake_build_type]"
echo "[`date +'%F %T'`] Build dir [$build_dir]"
echo "[`date +'%F %T'`] Build done"
cd $shell_dir







