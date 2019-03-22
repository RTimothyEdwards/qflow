#!/bin/tcsh -f
#----------------------------------------------------------
# LVS comparison script using magic
#----------------------------------------------------------
# Tim Edwards, 8/20/18, for Open Circuit Design
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  lvs.sh [options] <project_path> <source_name>
   exit 1
endif

# Split out options from the main arguments
set argline=(`getopt "" $argv[1-]`)

set options=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $1}'`
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc >= 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo Usage:  lvs.sh [options] <project_path> <source_name>
   echo   where
   echo       <project_path> is the name of the project directory containing
   echo                 a file called qflow_vars.sh.
   echo       <source_name> is the root name of the verilog file
   exit 1
endif

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
source ${techdir}/${techname}.sh
cd ${projectpath}
if (-f project_vars.sh) then
   source project_vars.sh
endif

if (! ${?lvs_options} ) then
   set lvs_options = ${options}
endif

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set lastlog=${logdir}/migrate.log
set synthlog=${logdir}/lvs.log
rm -f ${synthlog} >& /dev/null
rm -f ${logdir}/gdsii.log >& /dev/null
touch ${synthlog}
set date=`date`
echo "Qflow LVS logfile created on $date" > ${synthlog}

# Check if last line of migrate log file says "error condition"
if ( ! -f ${lastlog} ) then
   echo "Warning:  No migration logfile found."
else
   set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
   if ( ${errcond} == 1 ) then
      echo "Synthesis flow stopped on error condition.  LVS comparison"
      echo "will not proceed until error condition is cleared."
      exit 1
   endif
endif

# Check if migration was run.  Must have synthesis and layout extracted
# netlists.  All netlists must be more recent than the project ".def" file.

if ( ! ( -f ${synthdir}/${rootname}.spc || ( -f ${synthdir}/${rootname}.spc && \
	-M ${synthdir}/${rootname}.spc < -M ${synthdir}/${rootname}.blif ))) then
    echo "LVS failure: No schematic netlist found." |& tee -a ${synthlog}
    echo "Premature exit." |& tee -a ${synthlog}
    echo "Synthesis flow stopped due to error condition." >> ${synthlog}
    exit 1
endif

if ( ! ( -f ${layoutdir}/${rootname}.spice  || ( -f ${layoutdir}/${rootname}.spice && \
	-M ${layoutdir}/${rootname}.spice < -M ${layoutdir}/${rootname}.def ))) then
    echo "LVS failure: No layout extracted netlist found;  migration was not run." \
	|& tee -a ${synthlog}
    echo "Premature exit." |& tee -a ${synthlog}
    echo "Synthesis flow stopped due to error condition." >> ${synthlog}
    exit 1
endif

# If the layout (.mag) file is more recent than the netlist (.spice) file, then
# the layout needs to be re-extracted.

if ( -M ${layoutdir}/${rootname}.spice < -M ${layoutdir}/${rootname}.mag ) then
    echo "Layout post-dates extracted netlist;  re-extraction required." \
	|& tee -a ${synthlog}
    # Re-extract the netlist.
    source ${scriptdir}/migrate.sh -x ${projectpath} ${sourcename}
endif

# Check for technology setup script.  If it is in the qflow technology script
# as variable "netgen_setup", then use that.  Otherwise, assume it is in the
# technology directory path.

if ( ${?netgen_setup} ) then
   set setup_script=${netgen_setup}
else
   set setup_script=${techdir}/${techname}_setup.tcl
endif

# Check for existence of the netgen setup script in the techfile, and
# alternative setup scripts that may exist in the layout directory.

if ( ! ( -f ${setup_script} )) then
   if ( -f ${layoutdir}/setup.tcl ) then
      set setup_script=${layoutdir}/setup.tcl
   else if ( -f ${layoutdir}/${rootname}_setup.tcl ) then
      set setup_script=${layoutdir}/${rootname}_setup.tcl
   else if ( -f ${layoutdir}/${rootname}.tcl ) then
      set setup_script=${layoutdir}/${rootname}.tcl
   else
      echo "LVS failure: No technology setup script for netgen found." \
	|& tee -a ${synthlog}
      echo "Premature exit." |& tee -a ${synthlog}
      echo "Synthesis flow stopped due to error condition." >> ${synthlog}
      exit 1
   endif
endif


#----------------------------------------------------------
# Done with initialization
#----------------------------------------------------------

cd ${layoutdir}

#------------------------------------------------------------------
# Run netgen in batch mode.
#------------------------------------------------------------------

set outfile=comp.out

echo "Running netgen"
echo 'netgen ${lvs_options} -batch lvs "${rootname}.spice ${rootname}" \
	"${synthdir}/${rootname}.spc ${rootname}" ${setup_script} ${outfile} \
	-json -blackbox' |& tee -a ${synthlog} 

${bindir}/netgen ${lvs_options} -batch lvs "${rootname}.spice ${rootname}" \
	"${synthdir}/${rootname}.spc ${rootname}" ${setup_script} ${outfile} \
	-json -blackbox |& tee -a ${synthlog}

#---------------------------------------------------------------------
# Spot check:  Did netgen produce file comp.out?
#---------------------------------------------------------------------

if ( !( -f comp.out || ( -f comp.out && -M comp.out < -M ${rootname}.spice ))) then
   echo "netgen failure:  No file comp.out." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Check for LVS errors
#---------------------------------------------------------------------

echo "Running count_lvs.py"
echo "${scriptdir}/count_lvs.py" |& tee -a ${synthlog}
${scriptdir}/count_lvs.py |& tee -a ${synthlog}

set err_total = `tail -1 ${synthlog} | cut -d' ' -f4`
if ( ${err_total} > 0 ) then
   echo "Design fails LVS with ${err_total} errors." |& tee -a ${synthlog}
   echo "See lvs.log and comp.out for error details." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------

set endtime = `date`
echo "LVS checking script ended on $endtime" >> $synthlog

exit 0
