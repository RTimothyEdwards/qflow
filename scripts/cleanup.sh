#!/bin/tcsh -f
#----------------------------------------------------------
# Workspace cleanup script for qflow
#----------------------------------------------------------
# Tim Edwards, April 2013
#----------------------------------------------------------

# Split out options from the main arguments (no options---this is a placeholder)
set argline=(`getopt "p" $argv[1-]`)
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc == 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo "Usage:  cleanup.sh [options] <project_path> <source_name>"
   echo "  where"
   echo "      <project_path> is the name of the project directory containing"
   echo "                a file called qflow_vars.sh."
   echo "      <source_name> is the root name of the verilog file, and"
   echo "      [options] are:"
   echo "                -p      purge (keep sources only)"
   exit 1
endif

set purge=0

foreach option (${argline})
   switch (${option})
      case -p:
         set purge=1
         breaksw
      case --:
	 break
   endsw
end

set projectpath=$argv1
set sourcename=$argv2
set rootname=${sourcename:h}

# This script is called with the first argument <project_path>, which should
# have file "qflow_vars.sh".  Get all of our standard variable definitions
# from the qflow_vars.sh file.

if (! -f ${projectpath}/qflow_vars.sh ) then
   echo "Error:  Cannot find file qflow_vars.sh in path ${projectpath}"
   exit 1
endif

source ${projectpath}/qflow_vars.sh
cd ${projectpath}
if (-f project_vars.sh) then
   source project_vars.sh
endif

#----------------------------------------------------------
# Cleanup verilog parsing files.  Leave the original source!
#----------------------------------------------------------

cd ${layoutdir}

# Check if rootname needs a "_buf" suffix, which we use
# when AddIO2blif is told to double-buffer the outputs.

set origname=${rootname}
if ( ! -f ${rootname}.cel && -f ${rootname}_buf.cel ) then
   set rootname=${rootname}_buf
endif

cd ${sourcedir}

rm -f ${origname}.blif
rm -f ${origname}.xml
rm -f ${origname}_tmp.blif
rm -f ${origname}_mapped.blif
rm -f ${origname}_mapped_tmp.blif
rm -f ${origname}.clk
rm -f ${origname}.enc
rm -f ${origname}.init

rm -f ${origname}_mapped.v.orig
rm -f ${origname}_mapped.v
rm -f ${origname}_tmp.v

if ( $purge == 1 ) then
    rm -f ${origname}.ys
endif

#----------------------------------------------------------
# Clean up files from synthesis.  Leave the final buffered
# .blif netlist and the RTL verilog files
#----------------------------------------------------------

cd ${synthdir}

rm -f ${origname}_bak.v
rm -f ${origname}_tmp.v
rm -f ${rootname}_orig.v
rm -f ${rootname}_sized.v
rm -f ${rootname}_mapped.v
rm -f ${rootname}_anno.v
rm -f ${rootname}_postroute.v
rm -f ${rootname}_mapped.v
rm -f ${rootname}.anno.v
rm -f ${rootname}_nofanout
rm -f tmp.blif
rm -f tmp.v

if ( $purge == 1 ) then
    rm -f ${origname}.v
    rm -f ${rootname}.spc
    rm -f ${rootname}.xspice
    rm -f ${rootname}.spef
    rm -f ${rootname}.sdf
    rm -f ${rootname}.dly
    rm -f ${rootname}.rtl.v
    rm -f ${rootname}.rtlbb.v
    rm -f ${rootname}.rtlnopwr.v
    rm -f ${rootname}_synth.rtl.v
    rm -f ${rootname}_synth.rtlbb.v
    rm -f ${rootname}_synth.rtlnopwr.v
    rm -f ${rootname}_powerground
endif

#----------------------------------------------------------
# Clean up the (excessively numerous) GrayWolf files
# Keep the input .cel and .par files, and the input
# _unroute.def file and the final output .def file.  
#----------------------------------------------------------

cd ${layoutdir}

rm -f ${rootname}.blk ${rootname}.gen ${rootname}.gsav ${rootname}.history
rm -f ${rootname}.log ${rootname}.mcel ${rootname}.mdat ${rootname}.mgeo
rm -f ${rootname}.mout ${rootname}.mpin ${rootname}.mpth ${rootname}.msav
rm -f ${rootname}.mver ${rootname}.mvio ${rootname}.stat ${rootname}.out
rm -f ${rootname}.mtmp ${rootname}.pth ${rootname}.sav ${rootname}.scel
rm -f ${rootname}.txt ${rootname}.info

rm -f *.ext

rm -f ${rootname}.pin ${rootname}.pl1 ${rootname}.pl2
rm -f antenna.out fillcells.txt fail.out

rm -f run_drc_map9v3.tcl
rm -f generate_gds_map9v3.tcl
rm -f migrate_map9v3.tcl

rm -f cn
rm -f failed

if ( $purge == 1 ) then
    rm -f comp.out
    rm -f comp.json
    rm -f qflow.magicrc
    rm -f ${rootname}.spice
    rm -f ${rootname}.cel
    rm -f ${rootname}.cfg
    rm -f ${rootname}.rc
    rm -f ${rootname}.cel.bak
    rm -f ${rootname}.lef
    rm -f ${rootname}.def
    rm -f ${rootname}_unroute.def
    rm -f ${rootname}.mag
    rm -f ${rootname}.obs
    rm -f ${rootname}.gds
    rm -f ${rootname}.par.orig
endif

cd ${logdir}

if ( $purge == 1 ) then
    rm -f drc.log
    rm -f lvs.log
    rm -f gdsii.log
    rm -f migrate.log
    rm -f place.log
    rm -f post_sta.log
    rm -f prep.log
    rm -f route.log
    rm -f sta.log
    rm -f synth.log
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------
