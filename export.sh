#!/bin/bash

dst="/media/ext/dev/prog/1-risc/KarakatitsaRISCV.github.io/"
cp -f database/*.pintable $dst/docs/files/tables_src/
cp -f html/*.html $dst/docs/files/pintable/

changed=0

for i in `ls html` ; do
  if ! grep $i $dst/docs/mcu_table.md > /dev/null ; then
    echo '++ '`basename -s ".html" $i`' ++'
    (( changed++ ))
  fi
done

echo "Files changed: "$changed

if (( changed != 0 )) ; then
  kwrite $dst/docs/mcu_table.md 2> /dev/null
fi

echo "Edit mcu_pintable; git commit; git push"
konsole --workdir $dst