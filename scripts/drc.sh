#!/bin/tcsh -f
#----------------------------------------------------------
# DRC error checking script using magic
#----------------------------------------------------------
# Tim Edwards, 8/20/18, for Open Circuit Design
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  drc.sh [options] <project_path> <source_name>
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
   echo Usage:  drc.sh [options] <project_path> <source_name>
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

if (! ${?drc_options} ) then
   set drc_options = ${options}
endif

set rundrcfile="${layoutdir}/run_drc_${rootname}.tcl"

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set lastlog=${logdir}/lvs.log
set synthlog=${logdir}/drc.log
rm -f ${synthlog} >& /dev/null
rm -f ${logdir}/lvs.log >& /dev/null
rm -f ${logdir}/gdsii.log >& /dev/null
touch ${synthlog}
set date=`date`
echo "Qflow DRC logfile created on $date" > ${synthlog}

# Check if last line of drc log file says "error condition"
if ( ! -f ${lastlog} ) then
   set lastlog=${logdir}/post_sta.log
endif
if ( ! -f ${lastlog} ) then
   echo "Warning:  No LVS or post-route STA logfiles found."
else
   set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
   if ( ${errcond} == 1 ) then
      echo "Synthesis flow stopped on error condition.  DRC check "
      echo "will not proceed until error condition is cleared."
      exit 1
   endif
endif

# Does variable "gdsfile" exist?  If not, then use the LEF views for
# DRC checks.

set use_gds=1
if (! ${?gdsfile} || ( ${?gdsfile} && ( ${gdsfile} == "" ) ) ) then
   set use_gds=0
endif

# Prepend techdir to each leffile unless leffile begins with "/"
set lefpath=""
foreach f (${leffile})
   set abspath=`echo ${f} | cut -c1`
   if ( "${abspath}" == "/" ) then
      set p=${leffile}
   else
      set p=${techdir}/${leffile}
   endif
   set lefpath="${lefpath} $p"
end

# Ditto for gdsfile
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

# Usually "gdsfile" is set to one GDS file for the standard cell set
# but it can be a space-separated list of GDS files to read.  This
# is set by reading the .sh file.  If no gdsfile variable exists, or
# is blank, then GDS generation cannot proceed.

rm -f ${rundrcfile}
touch ${rundrcfile}

if (! ($?gdsview)) then
   set gdsview=0
endif

if ( $gdsview == 1 ) then
cat >> ${rundrcfile} << EOF
gds readonly true
gds rescale false
EOF
foreach gfile ( ${gdspath} )
cat >> ${rundrcfile} << EOF
gds read $gfile
EOF
end
else
foreach lfile ( ${lefpath} )
cat >> ${rundrcfile} << EOF
lef read $lfile
EOF
end
endif

cat >> ${rundrcfile} << EOF
load $rootname
drc on
select top cell
expand
drc check
drc catchup
set dcount [drc list count total]
puts stdout "drc = \$dcount"
quit -noprompt
EOF

#------------------------------------------------------------------
# Run magic in batch mode.
#------------------------------------------------------------------

echo "Running magic $version"
echo "magic -dnull -noconsole ${drc_options} ${rundrcfile}" |& tee -a ${synthlog} 
${bindir}/magic -dnull -noconsole ${drc_options} ${rundrcfile} |& tee -a ${synthlog}

#---------------------------------------------------------------------
# Spot check:  Does the last line of the synthlog have "drc = 0"?
#---------------------------------------------------------------------

set errors=`tail -10 ${synthlog} | grep "drc =" | cut -d' ' -f3`
if ( $errors > 0 ) then
   echo "DRC failure:  There are ${errors} DRC errors in the layout." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------

set endtime = `date`
echo "DRC checking script ended on $endtime" >> $synthlog

exit 0
