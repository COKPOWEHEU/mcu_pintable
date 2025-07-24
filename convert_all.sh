#!/bin/bash

inputdir="database"
outputdir="html"
packs="/usr/share/kicad/footprints/"

mkdir -p $outputdir

for i in `ls database/*.pintable` ; do
  j=`basename -s .pintable $i`
  echo $i "  " $outputdir/$j.html
  ./res/prog64 --packages=$packs $i  $outputdir/$j.html
done