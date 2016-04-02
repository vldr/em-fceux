#!/bin/bash
# Build/optimize shaders.
glslopt=../glsl-optimizer/glslopt
srcdir=src/drivers/em/assets/shaders
dstdir=src/drivers/em/assets/data
commonf=$srcdir/common
tempf=$srcdir/$RANDOM.tmp
if [ ! -f $glslopt ]
then
echo "GLSL shader optimizer not found at: " $glslopt
echo "Please configure buildshaders.sh script to point to the glslopt binary."
exit 1
fi

echo "Building vertex shaders:"
for f in $srcdir/*.vert
do
dstf=$dstdir/${f##*/}
echo $f " -> " $dstf
cat $commonf $f > $tempf
$glslopt -v -2 $tempf $dstf
done

echo "Building fragment shaders:"
for f in $srcdir/*.frag
do
dstf=$dstdir/${f##*/}
echo $f " -> " $dstf
cat $commonf $f > $tempf
$glslopt -f -2 $tempf $dstf
done

rm $tempf
