#!/bin/bash

if [ $# -ne 1 ]; then
  echo "[usage] $0 <target_device>"
  echo "    target_device          : device file. e.g. /dev/sg3"
  exit 1
fi

for _DIR in ./*; do

    if [ -d ${_DIR} ]; then

        cd ${_DIR}
        for file in *.sh; do
            ./${file} ${1}
        done
        cd ../
    fi

done

