#!/bin/bash

usage()
{
	echo "$0 <input-file-name>"
}

MAX=false
STDO=-i

while getopts "ms" OPT ; do
	case $OPT in
		m)	MAX=true
			;;
		s)	STDO=
			;;
		*)	echo "unknown option"
			usage
			exit 1
			;;
	esac
done

shift $(($OPTIND - 1))

SFILE="$1"

if [ \( "$SFILE" \) ] && [ -s "$SFILE" ] ; then
	: fine
else
	echo "garbled arguments: $*"
	echo $MAX $STDO
	usage
	exit 1
fi

sed -e '/^$/d' -e 's/^watts: .//' $SFILE | sort -n `$MAX && echo -r` >sort-tmp
if [ "$?" -eq 0 ] ; then
	sed $STDO 's/^/watts: /' sort-tmp # &&
	#	echo "file $SFILE sorted successfully" >/dev/stderr
	if [ -n "$STDO" ] ; then
		mv sort-tmp $SFILE
	else
		rm sort-tmp
	fi
else
	echo "something failed"
fi

