#!/bin/bash

inputdir="database"
outputdir="html"
packs="--packages=/usr/share/kicad/footprints/ --packages=/media/ext/dev/pcb/userlib_kicad.pretty/"

mkdir -p $outputdir

for i in `ls database/*.pintable` ; do
  j=`basename -s .pintable $i`
  echo $i "  " $outputdir/$j.html
  ./res/prog64 $packs $i  $outputdir/$j.html
done