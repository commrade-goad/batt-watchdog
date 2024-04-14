#!/bin/sh
COMPILER=$1
DESTINATION=$2
LINK="-lSDL2 -lSDL2_mixer -lmpg123"

print_help(){
    echo 'USAGE: (1) ./build.sh [gcc/clang]'
    echo '       (2) ./build.sh [gcc/clang] [/path/and/name]'
    exit 1
}

if [ -z $DESTINATION ] 
then
    DESTINATION="./batt-watchdog"
fi

case $COMPILER in
    "gcc") g++ src/main.cxx $LINK -o $DESTINATION ;;
    "clang") clang++ -std=c++17 src/main.cxx -O2 $LINK -o $DESTINATION;;
    * ) print_help;;
esac
if [ $? == 0 ]
    then
        echo saved at \`$DESTINATION\`
fi
