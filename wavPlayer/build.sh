#!/bin/bash
#:#####################################################:#
#:#  build.sh - source code compile & install tool    #:#
#:#  Written by Wade Ryan                             #:#
#:#  1/5/2020                                         #:#
#:#                                                   #:# 
#:#  Build commands (lifecycle):                      #:# 
#:#                                                   #:# 
#:#  clean    - remove all intermediate files         #:# 
#:#  compile  - build all sources & executable        #:# 
#:#  install  - install libraries to system folders   #:# 
#:#                                                   #:# 
#:#  * install will use sudo to access                #:# 
#:#    the system folders                             #:# 
#:#                                                   #:# 
#:#                                                   #:# 
#:#####################################################:#
set -a


BASE="."
SRC=$BASE/src
BIN=$BASE/bin
LIB=$BASE/lib
OBJ=$BASE/obj

LIBNAME=asound_rpi

DESTDIR=/usr
PREFIX=/local
TMP=/tmp/build.$$

C="gcc"
CC="g++"
CFLAGS="-c -O2 -std=c11 -Wall"
CCFLAGS="-c -O2 -std=gnu++11 -Wall"
LIBRARIES="-lwiringPi -lm -lasound -lpthread"
#;wiringPiDev;wiringPiPca9685;;wiringPiADS1115rpi;wiringPiPCA9635rpi;
LDFLAGS="-Wl,--no-undefined -Wl,-z,now"
INCLUDES=`find $SRC -type d | awk '{printf("-I%s ",$0);}'`


#:###################:#
#:#    Die          #:#
#:###################:#
die() { 
  exit 2
}

#:###################:#
#:#    Clean        #:#
#:###################:#
clean() { 
  if [ "$1" = "objects" ];then
    rm -rf $OBJ || die
  else 
    rm -rf $BIN $OBJ $LIB || die
  fi
}

#:###################:#
#:#    Compile      #:#
#:###################:#
compile() {
  LAST=$(ls -1tr "$OBJECT" "$SOURCE" "$HEADER" 2>/dev/null | tail -1)

  if [ "$LAST" = "$OBJECT" ];then
    return
  fi

  RELINK=1
  echo compiling "$SOURCE"
  eval \${$1} \${$2} $DYNAMIC $INCLUDES "$SOURCE" -o "$OBJECT"
  RET=$?

  if [ $RET != 0 ];then
    die
  fi
}

#:#####################:#
#:#  Examine Objects  #:#
#:#####################:#
examineObjects() {
  for SOURCE in `find $SRC -type f`
  do
    DIRNAME=`dirname ${SOURCE}`
    FILNAME=`basename ${SOURCE}`
    SRCNAME="${FILNAME%.*}"
    EXT="${FILNAME##*.}"
    OBJECT="$OBJ/$DIRNAME/$SRCNAME" 
    HEADER="$DIRNAME/$SRCNAME.h"

    mkdir -p $OBJ/$DIRNAME

    if [ $EXT = "c" ];then
      compile C CFLAGS
    elif [ $EXT = "cpp" ];then
      compile CC CCFLAGS
    fi
  done
}


#:###################:#
#:#  build objects  #:#
#:###################:#
build() {

  mkdir -p $BIN
  mkdir -p $OBJ

  examineObjects
}
#:##########################:#
#:#  echo what I'm doing   #:#
#:##########################:#
userEcho() {
  echo $*
  eval $* || die
}

#:######################:#
#:#  build executable  #:#
#:######################:#
executable() {
  [ "$INSTALL" != 1 ] && install

  echo linking exe

  userEcho $CC $INCLUDES $LIBRARIES $LDFLAGS wavPlayer.cpp  -o $BIN/wavPlayer lib/lib${LIBNAME}.a
}

#:###################:#
#:#  package        #:#
#:###################:#
package() {
  mkdir -p $LIB 


  echo Buliding source for static library for sake of runtime speed
  DYNAMIC=""
  build

  echo Packagiang lib$LIBNAME.a
  OBJECTS=$(find $OBJ -type f)
  ar rcs $LIB/lib$LIBNAME.a $OBJECTS || die
  ranlib $LIB/lib$LIBNAME.a  || die
}

#:###################:#
#:#  install        #:#
#:###################:#
install() {
  echo "[install]"
  echo install is not implemented yet
}

#:###################:#
#:#  un-install     #:#
#:###################:#
remove() {
  echo "[Un-install]"
  echo install is not implemented yet
}


#:###################:#
#:#    Main         #:#
#:###################:#

CLEAN=0
BUILD=0
INSTALL=0
PACKAGE=0
REMOVE=0
RELINK=0
EXECUTABLE=0

if [ "$*" = "" ];then
  BUILD=1
fi

for COMMAND in $* 
do
  typeset -l $COMMAND
  case $COMMAND in
    help)      echo "phases:  clean compile package install exe remove"
               exit 0
               ;;
    clean)     CLEAN=1
               ;;
    build)     BUILD=1
               ;;
    package)   PACKAGE=1
               ;;
    install)   INSTALL=1
               PACKAGE=1
               EXECUTABLE=1
               ;;
    exe)       PACKAGE=1
               EXECUTABLE=1
               ;;
    remove)    REMOVE=1
               ;;
    *)         BUILD=1
               ;;
  esac
done

[ $CLEAN = 1 ]       && clean
[ $BUILD = 1 ]       && build
[ $PACKAGE = 1 ]     && package
[ $INSTALL = 1 ]     && install
[ $EXECUTABLE = 1 ]  && executable
[ $REMOVE = 1 ]      && remove

exit 0
