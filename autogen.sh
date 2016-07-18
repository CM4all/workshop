#!/bin/sh

set -e

rm -rf config.cache build
mkdir build
aclocal
automake --add-missing --foreign
autoconf

./configure \
	--prefix=/usr/local/stow/cm4all-workshop \
	--enable-debug \
	--enable-silent-rules \
	CFLAGS="-O0 -ggdb" \
	CXXFLAGS="-O0 -ggdb" \
	"$@"
