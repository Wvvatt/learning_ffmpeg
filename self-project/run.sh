#!/bin/sh

start()
{
    LD_LIBRARY_PATH=../deps/lib $*
}

clear()
{
    rm *.flv *.mp4 *.pgm -f
}

PARAM=$($@)
case "$1" in
	start)
	echo ${PARAM:2}
	;;
	clear)
	clear
	;;
    *)
	echo "Usage: run.sh {start|clear} "
	exit 1
esac

exit $?