#!/bin/bash

WORKING_DIR=$PWD
SHOULD_CLEAN=
SRC_FOLDER=$WORKING_DIR/src
INCLUDE_FOLDER=$WORKING_DIR/include
LIBS_FOLDER=$WORKING_DIR/libs
COMPILER=
ARCH=$(uname -m)
INCLUDE_CMD="-I""$INCLUDE_FOLDER"
LIB_CMD="-L""$LIBS_FOLDER"
COMPILER_FLAGS="-Wno-format-extra-args"
LIB_NAME=libimage.a
TEST_BIN=test_libimage
PLATFORM=
AR_PATH=
DEBUG_MODE=

BEGIN=$(date +%s)
while getopts 'C:chd' opt; do
  case "$opt" in
     d)
	     DEBUG_MODE="yes"
	     ;;
     p)
	     PLATFORM="$OPTARG"
	     ;;
     c)
	    SHOULD_CLEAN="yes"
	   ;;
     C)
	     COMPILER="$OPTARG"
	     echo "Gonna use "$(basename $COMPILER) " in path "$(dirname $COMPILER). 
	     ;;
    h)
      echo "Usage: $(basename $0) "
      echo "-p				Set platform manually."
      echo "-c				Remove build folder before compiling."
      echo "-C  <compiler-fullpath>	Uses the compiler provided by the fullpath."
      exit 1
      ;;
  esac
done

searchBinary() {
TARGET="$1"

	if [ -z "$TARGET" ]; then return -1; fi
	FILE_PATH=$(which $TARGET | cut -d " " -f 2)
	if [ ! -f "$FILE_PATH" ]; then return -1; fi

	echo $FILE_PATH
	return 0
}

if [ -z "$PLATFORM" ]; then
	PLATFORM=$(uname -a | cut -d " " -f 1)
fi

echo "Platform: " $PLATFORM
echo Working directory: $WORKING_DIR

if [ -z "$COMPILER" ]; then
	echo -n "Searching for gcc.."
	COMPILER=$(searchBinary "gcc")
	if [ "$?" != 0 ]; then
		echo "Not found."
		echo -n "Searching for clang..."
		COMPILER=$(searchBinary "clang")
		if [ "$?" != 0 ]; then
			echo "Not found."
			echo -n "Searching for mingw..."
			COMPILER=$(searchBinary "mingw")
			if [ "$?" != 0 ]; then
				echo "Not found."
				echo -n "Searching for Visual Studio compiler..."
				PATH=
				for partition in $( $(df -l | cut -d " " -f 1) ); do 
					PATH=$(find $partition -name cl.exe)
					if [ ! -z $PATH ]; then break; fi
				done
				echo "Not implemented $PATH"
				exit
			else 
				echo "mingw found."
			fi
		else 
			echo "clang found."	
		fi
	else 
		echo "gcc found."
	fi	
fi

if [ -z "$AR_PATH" ]; then
	echo -n "Searching for ar..."
	AR_PATH=$(searchBinary "ar")
	if [ "$?" != 0 ]; then
		echo "Not found."
	else 
		echo "ar found."
	fi
fi

if [ ! -z "$SHOULD_CLEAN" ]; then
	echo "[INFO]: Removing build folder"
	rm -rf $WORKING_DIR/build
	rm -rf $WORKING_DIR/libs
fi

echo "[INFO]: Compiling libimage" 
sleep 1

if [ ! -z "$DEBUG_MODE" ]; then
	COMPILER_FLAGS= "$COMPILER_FLAGS"" -DDEBUG=1 -g -ggdb3 -O0"
else
	COMPILER_FLAGS="$COMPILER_FLAGS"" -DRELEASE=1 -O2" 
fi

mkdir -p $WORKING_DIR/build
mkdir -p $WORKING_DIR/libs
pushd $WORKING_DIR/build > /dev/null 2>&1
for file in $(find ../src -name *.c ); do
	$COMPILER $COMPILER_FLAGS $file -o $(basename ${file%.*}).o -c "$INCLUDE_CMD" "$LIB_CMD"
done
OBJ_LIST=$(find $WORKING_DIR/build -name "*.o" -printf "%p ")

$AR_PATH cr $LIB_NAME $OBJ_LIST
mv $LIB_NAME $LIBS_FOLDER/

$COMPILER $COMPILER_FLAGS $WORKING_DIR/tests/$TEST_BIN.c -o $TEST_BIN $INCLUDE_CMD -lm "$LIB_CMD" -limage
popd > /dev/null 2>&1

RUNTIME=$(( $(date +%s) - $BEGIN ))
RUNTIME_MSG=

if [ "$RUNTIME" -gt "1" ]; then
	RUNTIME_MSG="$RUNTIME seconds"
else
	RUNTIME_MSG="$RUNTIME second"
fi

echo "Finished execution in: $RUNTIME_MSG"
