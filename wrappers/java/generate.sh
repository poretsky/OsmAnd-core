#!/bin/bash

if [ -z "$BASH_VERSION" ]; then
	echo "Invalid shell, re-running using bash..."
	exec bash "$0" "$@"
	exit $?
fi
SRCLOC="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Prepare output folders
REAL_SRCLOC=`readlink -f $SRCLOC`
OUTPUTDIR=$1
if [ -z "$OUTPUTDIR" ]; then
	OUTPUTDIR="$(pwd)"
fi
echo "Output path:$OUTPUTDIR"
if [ -d "$OUTPUTDIR/gen" ]; then
	rm -rf "$OUTPUTDIR/gen"
fi
mkdir -p "$OUTPUTDIR/gen/java/net/osmand/core/jni"
echo "$OUTPUTDIR/gen/java/net/osmand/core/jni"
mkdir -p "$OUTPUTDIR/gen/cpp"

if [[ -z "$SWIG" ]]; then
	SWIG=`which swig`
else
	if [[ "$(uname -a)" =~ Cygwin ]]; then
		SWIG=$(cygpath -u "$SWIG")
	fi
fi
echo "Using '$SWIG' as swig"

# Actually perform generation
$SWIG \
	-java \
	-package net.osmand.core.jni \
	-outdir "$OUTPUTDIR/gen/java/net/osmand/core/jni" \
	-o "$OUTPUTDIR/gen/cpp/swig.cpp" \
	-I"$REAL_SRCLOC/../../include" \
	-c++ \
	-v \
	"$REAL_SRCLOC/../../core.swig"

# Collect output files
FIND_CMD="$(which find)"
PRINT_CMD="-print"
if [[ "$(uname -a)" =~ Cygwin ]]; then
	PRINT_CMD="-print0 | xargs -0 cygpath -w"
fi
eval "$FIND_CMD \"${OUTPUTDIR}/gen/cpp\" -type f $PRINT_CMD" > "$OUTPUTDIR/gen/cpp.list"
eval "$FIND_CMD \"${OUTPUTDIR}/gen/java\" -type f $PRINT_CMD" > "$OUTPUTDIR/gen/java.list"
