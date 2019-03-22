#!/bin/tcsh -f
#----------------------------------------------------------
# Static timing analysis script using vesta
#----------------------------------------------------------
# Tim Edwards, 10/29/13, for Open Circuit Design
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  vesta.sh [options] <project_path> <source_name>
   exit 1
endif

# Split out options from the main arguments
set argline=(`getopt "ad" $argv[1-]`)

set options=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $1}'`
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc == 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo Usage:  vesta.sh [options] <project_path> <source_name>
   echo   where
   echo       <project_path> is the name of the project directory containing
   echo                 a file called qflow_vars.sh.
   echo       <source_name> is the root name of the verilog file
   echo	      [options] are:
   echo			-d	use delay file to back-annotate wire delays
   echo			-a	append to log file (do not overwrite)
   echo
   exit 1
endif

set dodelays=0
set append=0

foreach option (${argline})
   switch (${option})
      case -d:
         set dodelays=1
         breaksw
      case -a:
	 set append=1
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
source ${techdir}/${techname}.sh
cd ${projectpath}
if (-f project_vars.sh) then
   source project_vars.sh
endif

if (! ${?vesta_options} ) then
   set vesta_options = ""
endif

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
if ($dodelays == 1) then
   set lastlog=${logdir}/route.log
   set synthlog=${logdir}/post_sta.log
else
   set lastlog=${logdir}/place.log
   set synthlog=${logdir}/sta.log
   rm -f ${logdir}/route.log >& /dev/null
endif
rm -f ${logdir}/post_sta.log >& /dev/null
rm -f ${logdir}/migrate.log >& /dev/null
rm -f ${logdir}/drc.log >& /dev/null
rm -f ${logdir}/lvs.log >& /dev/null
rm -f ${logdir}/gdsii.log >& /dev/null
set date=`date`
if ( $append == 0 ) then
   rm -f ${synthlog} >& /dev/null
   touch ${synthlog}
   echo "Qflow static timing analysis logfile created on $date" > ${synthlog}
else
   touch ${synthlog}
   echo "\nQflow static timing analysis logfile appended on $date" >> ${synthlog}
endif


# Check if last line of log file says "error condition"
set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
if ( ${errcond} == 1 ) then
   echo "Synthesis flow stopped on error condition.  Static timing analysis"
   echo "will not proceed until error condition is cleared."
   exit 1
endif

# Prepend techdir to libertyfile unless libertyfile begins with "/"
set abspath=`echo ${libertyfile} | cut -c1`
if ( "${abspath}" == "/" ) then
   set libertypath=${libertyfile}
   set libertylist=${libertyfile}
else
   set libertypath=${techdir}/${libertyfile}
   set libertylist=${techdir}/${libertyfile}
endif

# Add hard macros

if ( ${?hard_macros} ) then
   foreach macro_path ( $hard_macros )
      foreach file ( `ls ${sourcedir}/${macro_path}` )
	 if ( ${file:e} == "lib" ) then
	    set libertypath="${libertypath} -l ${sourcedir}/${macro_path}/${file}"
	    set libertylist="${libertylist} ${sourcedir}/${macro_path}/${file}"
	    break
         endif
      end
   end
endif

#----------------------------------------------------------
# Done with initialization
#----------------------------------------------------------

# Check if last line of log file says "error condition"
set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
if ( ${errcond} == 1 ) then
   echo "Synthesis flow stopped on error condition.  Timing analysis will not"
   echo "proceed until error condition is cleared."
   exit 1
endif

cd ${layoutdir}

#------------------------------------------------------------------
# Generate the static timing analysis results
#------------------------------------------------------------------

if ($dodelays == 1) then
    # Check if a .rc file exists.  This file is produced by qrouter
    # and contains delay information in nested RC pairs
    if ( -f ${rootname}.rc ) then

       # Run rc2dly
       echo "Converting qrouter output to vesta delay format" |& tee -a ${synthlog}
       echo "Running rc2dly -r ${rootname}.rc -l ${libertypath} -d ${rootname}.dly" \
		|& tee -a ${synthlog}
       ${bindir}/rc2dly -r ${rootname}.rc -l ${libertypath} \
		-d ${synthdir}/${rootname}.dly

       # Run rc2dly again to get SPEF format file
       echo "Converting qrouter output to SPEF delay format" |& tee -a ${synthlog}
       echo "Running rc2dly -D : -r ${rootname}.rc -l ${libertypath} -d ${rootname}.spef" \
		|& tee -a ${synthlog}
       ${bindir}/rc2dly -D : -r ${rootname}.rc -l ${libertypath} \
		-d ${synthdir}/${rootname}.spef

       # Translate <, >, and $ in file to _ to match the verilog.  Make translations
       # ONLY in the name mapping section.
       if ( -f ${synthdir}/${rootname}.spef ) then
	  cat ${synthdir}/${rootname}.spef | sed \
		-e '/^\*[0-9]/s/\$/_/g' \
		-e '/^\*[0-9]/s/</_/g' \
		-e '/^\*[0-9]/s/>/_/g' \
		-e '/^\*[0-9]/s/\./_/g' \
		> ${synthdir}/${rootname}.spefx
	  mv ${synthdir}/${rootname}.spefx ${synthdir}/${rootname}.spef
       endif

       # Run rc2dly again to get SDF format file
       echo "Converting qrouter output to SDF delay format" |& tee -a ${synthlog}
       echo "Running rc2dly -r ${rootname}.rc -l ${libertypath} -d ${rootname}.sdf" \
		|& tee -a ${synthlog}
       ${bindir}/rc2dly -r ${rootname}.rc -l ${libertypath} \
		-d ${synthdir}/${rootname}.sdf

       # Translate <, >, in file to [, ] to match the verilog (rtl.nopwr.v version).
       # (Because SDF format does not recognize alternative array delimiters)
       if ( -f ${synthdir}/${rootname}.sdf ) then
	  cat ${synthdir}/${rootname}.sdf | sed \
		-e 's/</\[/g' -e 's/>/\]/g' \
		> ${synthdir}/${rootname}.sdfx
	  mv ${synthdir}/${rootname}.sdfx ${synthdir}/${rootname}.sdf
       endif

       cd ${synthdir}

       # Spot check for output file (NOTE:  Currently not checking if SPEF
       # or SDF format files were created)
       if ( !( -f ${rootname}.dly || \
		( -M ${rootname}.dly < -M ${layoutdir}/${rootname}.rc ))) then
	  echo "rc2dly failure:  No file ${rootname}.dly created." \
		|& tee -a ${synthlog}
          echo "Premature exit." |& tee -a ${synthlog}
          echo "Synthesis flow stopped due to error condition." >> ${synthlog}
          exit 1
       endif

       # Add delay file to vesta options, assuming it exists.
       set vesta_options = "-c -d ${rootname}.dly ${vesta_options}"
    else
       echo "Error:  No file ${rootname}.rc, cannot back-annotate delays!" \
		|& tee -a ${synthlog}
       echo "Premature exit." |& tee -a ${synthlog}
       echo "Synthesis flow stopped due to error condition." >> ${synthlog}
       exit 1
    endif
endif

cd ${synthdir}

echo ""
if ($dodelays == 1) then
   echo "Running vesta static timing analysis with back-annotated extracted wire delays" \
		|& tee -a ${synthlog}
else
   echo "Running vesta static timing analysis" |& tee -a ${synthlog}
endif
echo "vesta ${vesta_options} ${rootname}.rtlnopwr.v ${libertylist}" \
		|& tee -a ${synthlog}
echo ""
${bindir}/vesta ${vesta_options} ${rootname}.rtlnopwr.v \
		${libertylist} |& tee -a ${synthlog}
echo ""

#------------------------------------------------------------
# Done!
#------------------------------------------------------------
