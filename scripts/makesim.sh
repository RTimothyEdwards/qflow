#!/bin/tcsh -f
#
#  makesim.sh cellname
#
#---------------------------------------------------------------
# Run magic in batch mode to extract and generate a .sim view
# for the argument cellname
#---------------------------------------------------------------

if ($#argv < 1) then
   echo "Usage:  makesim.sh <cellname>"
   exit 1
endif

set magdir=${argv[1]:h}
if ("$magdir" == "$argv[1]") then
   set magdir="."
endif
set cellname=${argv[1]:t:r}

cd $magdir
rm -f ${cellname}.ext
rm -f ${cellname}.sim

/usr/local/bin/magic -dnull -noconsole <<EOF
drc off
box 0 0 0 0
load ${cellname}
extract all
ext2sim
quit -noprompt
EOF

exit 0	;# Return normal exit status
