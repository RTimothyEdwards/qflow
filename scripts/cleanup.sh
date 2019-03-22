#!/bin/tcsh -f
#----------------------------------------------------------
# Workspace cleanup script for qflow
#----------------------------------------------------------
# Tim Edwards, April 2013
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  cleanup.sh [options] <project_path> <source_name>
   exit 1
endif

# Split out options from the main arguments (no options---this is a placeholder)
set argline=(`getopt "" $argv[1-]`)
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc == 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo Usage:  cleanup.sh <project_path> <source_name>
   echo   where
   echo       <project_path> is the name of the project directory containing
   echo                 a file called qflow_vars.sh.
   echo       <source_name> is the root name of the verilog file, and
   exit 1
endif

foreach option (${argline})
   switch (${option})
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
rm -f ${origname}_tmp.v

#----------------------------------------------------------
# Clean up files from synthesis.  Leave the final buffered
# .blif netlist and the RTL verilog files
#----------------------------------------------------------

cd ${synthdir}

# rm -f ${origname}.blif
rm -f ${origname}_bak.blif
rm -f ${origname}_tmp.blif
rm -f ${rootname}_orig.blif
rm -f ${rootname}_anno.blif
rm -f ${rootname}_nofanout
rm -f ${rootname}_powerground
rm -f tmp.blif

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
rm -f ${rootname}.mtmp ${rootname}.rc
rm -f ${rootname}.pth ${rootname}.sav ${rootname}.scel
rm -f ${rootname}.txt ${rootname}.info

rm -f ${rootname}.pin ${rootname}.pl1 ${rootname}.pl2
rm -f ${rootname}.cfg
rm -f antenna.out fillcells.txt fail.out

rm -f run_drc_map9v3.tcl
rm -f generate_gds_map9v3.tcl
rm -f migrate_map9v3.tcl

# rm -f ${origname}_unroute.def

rm -f cn
rm -f failed

#------------------------------------------------------------
# Done!
#------------------------------------------------------------
