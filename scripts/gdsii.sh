#!/bin/tcsh -f
#----------------------------------------------------------
# GDSII output generating script using magic
#----------------------------------------------------------
# Tim Edwards, 4/23/18, for Open Circuit Design
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  gdsii.sh [options] <project_path> <source_name>
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
   echo Usage:  gdsii.sh [options] <project_path> <source_name>
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

if (! ${?gdsii_options} ) then
   set gdsii_options = ${options}
endif

set gengdsfile="${layoutdir}/generate_gds_${rootname}.tcl"

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set lastlog=${logdir}/drc.log
set synthlog=${logdir}/gdsii.log
rm -f ${synthlog} >& /dev/null
touch ${synthlog}
set date=`date`
echo "Qflow gdsii logfile created on $date" > ${synthlog}

# Check if last line of drc log file says "error condition"
if ( ! -f ${lastlog} ) then
   set lastlog=${logdir}/lvs.log
endif
if ( ! -f ${lastlog} ) then
   echo "Warning:  No DRC or LVS logfiles found."
else
   set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
   if ( ${errcond} == 1 ) then
      echo "Synthesis flow stopped on error condition.  GDSII generation"
      echo "will not proceed until error condition is cleared."
      exit 1
   endif
endif

# Does variable "gdsfile" exist?

if (! ${?gdsfile} || ( ${?gdsfile} && ( ${gdsfile} == "" ) ) ) then
   echo "GDS generation failure:  No gdsfile variable set in technology setup script." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

# Prepend techdir to each gdsfile unless gdsfile begins with "/"
set gdspath=""
foreach f (${gdsfile})
   set abspath=`echo ${f} | cut -c1`
   if ( "${abspath}" == "/" ) then
      set p=${gdsfile}
   else
      set p=${techdir}/${gdsfile}
   endif
   set gdspath="${gdspath} $p"
end

#----------------------------------------------------------
# Done with initialization
#----------------------------------------------------------

cd ${layoutdir}

#------------------------------------------------------------------
# Determine the version number and availability of scripting
#------------------------------------------------------------------

set version=`${bindir}/magic --version`
set major=`echo $version | cut -d. -f1`
set minor=`echo $version | cut -d. -f2`
set subv=`echo $version | cut -d. -f3`

#------------------------------------------------------------------
# Generate script for input to magic.
#------------------------------------------------------------------


cat > ${gengdsfile} << EOF
drc off
box 0 0 0 0
gds readonly true
gds rescale false
EOF

# Usually "gdsfile" is set to one GDS file for the standard cell set
# but it can be a space-separated list of GDS files to read.  This
# is set by reading the .sh file.

foreach gfile ( ${gdspath} )
cat >> ${gengdsfile} << EOF
gds read $gfile
EOF
end

cat >> ${gengdsfile} << EOF
load $rootname
select top cell
expand
gds write $rootname
quit -noprompt
EOF

#------------------------------------------------------------------
# Run magic in batch mode.
#------------------------------------------------------------------

echo "Running magic $version"
echo "magic -dnull -noconsole ${gdsii_options} ${gengdsfile}" |& tee -a ${synthlog} 
${bindir}/magic -dnull -noconsole ${gdsii_options} ${gengdsfile} |& tee -a ${synthlog}

#---------------------------------------------------------------------
# Spot check:  Did magic produce file ${rootname}.gds?
#---------------------------------------------------------------------

if ( !( -f ${rootname}.gds || ( -f ${rootname}.gds && -M ${rootname}.def \
		< -M ${rootname}.gds ))) then
   echo "magic failure:  No file ${rootname}.gds." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------

set endtime = `date`
echo "GDS generating script ended on $endtime" >> $synthlog

exit 0
