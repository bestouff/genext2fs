#!/bin/sh

die() {
	echo "*** $0 failed :("
	exit 1
}

./clean.sh

automake_flags="-c -a"
for p in aclocal autoconf automake ; do
	flags=${p}_flags
	if ! ${p} ${!flags} ; then
		echo "*** ${p} failed :("
		exit 1
	fi
done

echo
echo "Now just run:"
echo "./configure"
echo "make"
