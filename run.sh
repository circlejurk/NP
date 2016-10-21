#!/bin/sh

cp -r ras ${HOME}
cp bin/ras-server ${HOME}
cd ${HOME}/ras
${HOME}/ras-server
cd -
rm -rf ${HOME}/ras ${HOME}/ras-server
