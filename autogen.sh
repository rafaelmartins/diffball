#!/bin/sh

touch ChangeLog
aclocal-1.11 || { echo "failed aclocal"; exit 1; };
libtoolize --automake -c -f || { echo "failed libtoolize"; exit 1; }
autoconf || { echo "failed autoconf"; exit 1; }
autoheader || { echo "failed autoheader"; exit ; }
automake-1.11 -a -c || { "echo failed automake"; exit 1; }

if [ -x ./test.sh ] ; then
	exec ./test.sh "$@"
fi
echo "finished"
