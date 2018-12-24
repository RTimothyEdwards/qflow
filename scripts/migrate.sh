#!/bin/tcsh -f
#----------------------------------------------------------
# Layout and netlist migration script
#----------------------------------------------------------
# Tim Edwards, 8/20/18, for Open Circuit Design
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  migrate.sh [options] <project_path> <source_name>
   exit 1
endif

# Split out options from the main arguments
set argline=(`getopt "x" $argv[1-]`)
set options=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $1}'`
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc >= 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo Usage:  migrate.sh [options] <project_path> <source_name>
   echo   where
   echo       <project_path> is the name of the project directory containing
   echo                 a file called qflow_vars.sh.
   echo       <source_name> is the root name of the verilog file, and
   echo       [options] are:
   echo                 -x      extract only (use existing layout)
   echo
   exit 1
endif

set useexisting=0

foreach option (${options})
   switch (${option})
      case -x:
	 set useexisting=1
	 breaksw
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
source ${techdir}/${techname}.sh
cd ${projectpath}
if (-f project_vars.sh) then
   source project_vars.sh
endif

if (! ${?migrate_options} ) then
   set migrate_options = ""
endif

if ( ! ${?lef_options} ) then
   set lef_options = ""
endif

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set lastlog=${logdir}/post_sta.log
if ( ${useexisting} == 1 ) then
   set synthlog=${logdir}/lvs.log
else
   set synthlog=${logdir}/migrate.log
   rm -f ${logdir}/drc.log >& /dev/null
   rm -f ${logdir}/lvs.log >& /dev/null
endif
rm -f ${logdir}/gdsii.log >& /dev/null
rm -f ${synthlog} >& /dev/null
touch ${synthlog}
set date=`date`
if ( ${useexisting} == 1 ) then
   echo "Qflow LVS logfile appended with re-extraction on $date" > ${synthlog}
else
   echo "Qflow migration logfile created on $date" > ${synthlog}
endif

# Check if last line of post_sta log file says "error condition"
# Not necessary to run post_sta, so check route logfile if post_sta does not exist.
if ( ! -f ${lastlog} ) then
   set lastlog=${logdir}/route.log
endif
if ( ! -f ${lastlog} ) then
   echo "Warning:  No post-route STA or route logfiles found."
else
   set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
   if ( ${errcond} == 1 ) then
      echo "Synthesis flow stopped on error condition.  DRC check "
      echo "will not proceed until error condition is cleared."
      exit 1
   endif
endif

set migratefile="migrate_${rootname}.tcl"

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

# Prepend techdir to techleffile unless techleffile begins with "/"
set abspath=`echo ${techleffile} | cut -c1`
if ( "${abspath}" == "/" ) then
   set techlefpath=${techleffile}
else
   set techlefpath=${techdir}/${techleffile}
endif

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

cat > ${migratefile} << EOF
box 0 0 0 0
drc off
snap int
EOF

if ($techleffile != "") then
   cat >> ${migratefile} << EOF
lef read ${techlefpath}
EOF
endif

foreach lfile ( ${lefpath} )
   cat >> ${migratefile} << EOF
lef read $lfile
EOF
end

# Handle additional files from the hard macro list
if ( ${?hard_macros} ) then
   foreach macro_path ( $hard_macros )
      foreach file ( `ls ${sourcedir}/${macro_path}` )
	 if ( ${file:e} == "lef" ) then
	    cat >> ${migratefile} << EOF
lef read ${sourcedir}/${macro_path}/${file}"
EOF
	 endif
      end
   end
endif

# The following script reads in the DEF file and modifies labels so that
# they are rotated outward from the cell, since DEF files don't indicate
# label geometry.

if ( ${useexisting} == 1 ) then
   cat >> ${migratefile} << EOF
load ${rootname}
EOF
else
   cat >> ${migratefile} << EOF
def read ${rootname}
load ${rootname}
select top cell
select area labels
setlabel font FreeSans
setlabel size 0.3um
box grow s -[box height]
box grow s 100
select area labels
setlabel rotate 90
setlabel just e
select top cell
box height 100
select area labels
setlabel rotate 270
setlabel just w
select top cell
box width 100
select area labels
setlabel just w
select top cell
box grow w -[box width]
box grow w 100
select area labels
setlabel just e
writeall force ${rootname}
EOF
endif

cat >> ${migratefile} << EOF
lef write ${rootname} ${lef_options}
expand
extract all
ext2spice hierarchy on
ext2spice format ngspice
ext2spice scale off
ext2spice renumber off
ext2spice cthresh infinite
ext2spice rthresh infinite
ext2spice blackbox on
ext2spice subcircuit top auto
ext2spice global off
ext2spice
quit -noprompt
EOF

#------------------------------------------------------------------
# Run magic in batch mode.
#------------------------------------------------------------------

echo "Running magic $version"
echo "magic -dnull -noconsole ${migrate_options} ${migratefile}" |& tee -a ${synthlog} 
${bindir}/magic -dnull -noconsole ${migrate_options} ${migratefile} |& tee -a ${synthlog}

#---------------------------------------------------------------------
# Spot check:  Did the script generate .mag, .lef, and .spice files?
#---------------------------------------------------------------------

if ( ${useexisting} == 0 ) then
   if ( !( -f ${rootname}.mag || ( -f ${rootname}.mag && \
		-M ${rootname}.mag < -M ${rootname}.def ))) then
      echo "Migration failure:  No .mag layout file generated." \
		|& tee -a ${synthlog}
      echo "Premature exit." |& tee -a ${synthlog}
      echo "Synthesis flow stopped due to error condition." >> ${synthlog}
      exit 1
   endif
endif

if ( !( -f ${rootname}.lef || ( -f ${rootname}.lef && \
	-M ${rootname}.lef < -M ${rootname}.def ))) then
   echo "Migration failure:  No project .lef macro file generated." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

if ( !( -f ${rootname}.spice || ( -f ${rootname}.spice && \
	-M ${rootname}.spice < -M ${rootname}.def ))) then
   echo "Migration failure:  No project .spice extracted netlist generated." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

# Remove all of the extracted files from the layout directory
rm -f ${layoutdir}/*.ext

#------------------------------------------------------------
# Done!
#------------------------------------------------------------

set endtime = `date`
if ( ${useexisting} == 1 ) then
   echo "Re-extraction script ended on $endtime" >> $synthlog
else
   echo "Migration script ended on $endtime" >> $synthlog
endif

exit 0
