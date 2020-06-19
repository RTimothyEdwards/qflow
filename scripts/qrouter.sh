#!/bin/tcsh -f
#----------------------------------------------------------
# Route script using qrouter
#----------------------------------------------------------
# Tim Edwards, 5/16/11, for Open Circuit Design
# Modified April 2013 for use with qflow
#----------------------------------------------------------

if ($#argv < 2) then
   echo "Usage:  qrouter.sh [options] <project_path> <source_name>"
   exit 1
endif

# Split out options from the main arguments
set argline=(`getopt "nr" $argv[1-]`)

set options=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $1}'`
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc >= 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
   if ($argc == 3) then
      set statusin = `echo $cmdargs | cut -d' ' -f3`
      if ($statusin == 2) then
	 echo "Qrouter completed on first iteration, no need to run again."
         exit 0
      endif
   endif
else
   echo "Usage:  qrouter.sh [options] <project_path> <source_name>"
   echo "  where"
   echo "      <project_path> is the name of the project directory containing"
   echo "                a file called qflow_vars.sh."
   echo "      <source_name> is the root name of the verilog file"
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

# Get power and ground bus names
if (-f ${synthdir}/${rootname}_powerground) then
   source ${synthdir}/${rootname}_powerground
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

# Prepend techdir to techleffile unless techleffile begins with "/"
set abspath=`echo ${techleffile} | cut -c1`
if ( "${abspath}" == "/" ) then
   set techlefpath=${techleffile}
else
   set techlefpath=${techdir}/${techleffile}
endif

# Prepend techdir to each spicefile unless spicefile begins with "/"
set spicepath=""
foreach f (${spicefile})
   set abspath=`echo ${f} | cut -c1`
   if ( "${abspath}" == "/" ) then
      set p=${spicefile}
   else
      set p=${techdir}/${spicefile}
   endif
   set spicepath="${spicepath} $p"
end

# Add hard macros to spice path

if ( ${?hard_macros} ) then
   foreach macro_path ( $hard_macros )
      foreach file ( `ls ${sourcedir}/${macro_path}` )
	 # Too bad SPICE doesn't have an agreed-upon extension.  Common ones are:
	 if ( ${file:e} == "sp" || ${file:e} == "spc" || \
			${file:e} == "spice" || ${file:e} == "cdl" || \
			${file:e} == "ckt" || ${file:e} == "net") then
	    set spicepath="${spicepath} -l ${sourcedir}/${macro_path}/${file}"
	    break
	 endif
      end
   end
endif

# Prepare LEF file options to pass to DEF2Verilog (post-route)

if ( "$techleffile" == "" ) then
    set lefoptions=""
else
    set lefoptions="-l ${techlefpath}"
endif
set lefoptions="${lefoptions} -l ${lefpath}"

# Pass additional .lef files to DEF2Verilog from the hard macros list

if ( ${?hard_macros} ) then
    foreach macro_path ( $hard_macros )
        foreach file ( `ls ${sourcedir}/${macro_path}` )
            if ( ${file:e} == "lef" ) then
                set lefoptions="${lefoptions} -l ${sourcedir}/${macro_path}/${file}"
            endif
        end
    end
endif

# "-nog" (no graphics) has no graphics or console.  "-noc"
# (no console) has graphics but no console.  Console must
# always be disabled or else the script cannot capture the
# qrouter output.

if (! ${?qrouter_options} ) then
   set qrouter_options = "${options}"
endif

if (! ${?route_show} ) then
   set qrouter_options = "-nog ${qrouter_options}"
else
   if (${route_show} == 1) then
      set qrouter_options = "-noc ${qrouter_options}"
   else
      set qrouter_options = "-nog ${qrouter_options}"
   endif
endif

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set lastlog=${logdir}/place.log
set synthlog=${logdir}/route.log
rm -f ${synthlog} >& /dev/null
rm -f ${logdir}/post_sta.log >& /dev/null
touch ${synthlog}
set date=`date`
echo "Qflow route logfile created on $date" > ${synthlog}

# Check if last line of placement log file says "error condition"
if ( ! -f ${lastlog} ) then
   set lastlog=${logdir}/synth.log
endif
if ( ! -f ${lastlog} ) then
   echo "Warning:  No placement or static timing analysis logfiles found."
else
   set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
   if ( ${errcond} == 1 ) then
      echo "Synthesis flow stopped on error condition.  Detail routing"
      echo "will not proceed until error condition is cleared."
      exit 1
   endif
endif

#----------------------------------------------------------
# Done with initialization
#----------------------------------------------------------

cd ${layoutdir}

#------------------------------------------------------------------
# Determine the version number and availability of scripting
#------------------------------------------------------------------

set version=`${bindir}/qrouter -v 0 -h | tail -1`
set major=`echo $version | cut -d. -f1`
set minor=`echo $version | cut -d. -f2`
set subv=`echo $version | cut -d. -f3`
set scripting=`echo $version | cut -d. -f4`

# If there is a file called (project)_unroute.def, copy it
# to the primary .def file to be used by the router.  This
# overwrites any previously generated route solution.

if ( -f ${rootname}_unroute.def ) then
   cp ${rootname}_unroute.def ${rootname}.def
endif

if ( -f antenna.out ) then
   rm -f antenna.out
endif

if ( -f failed.out ) then
   rm -f failed.out
endif

if (${scripting} == "T") then

#------------------------------------------------------------------
# Scripted qrouter.  Given qrouter with Tcl/Tk scripting capability,
# create a script to perform the routing.  The script will allow
# the graphics to display, keep the output to the console at a
# minimum, and generate a file with congestion information in the
# case of route failure.
#------------------------------------------------------------------

   echo "Running qrouter $version"
   echo "qrouter ${qrouter_options} -s ${rootname}.cfg" |& tee -a ${synthlog} 
   ${bindir}/qrouter ${qrouter_options} -s ${rootname}.cfg \
		|& tee -a ${synthlog} | \
		grep - -e Failed\ net -e fail -e Progress -e remaining.\*00\$ \
		-e remaining:\ \[1-9\]0\\\?\$ -e \\\*\\\*\\\*
else

#------------------------------------------------------------------
# Create the detailed route.  Monitor the output and print errors
# to the output, as well as writing the "commit" line for every
# 100th route, so the end-user can track the progress.
#------------------------------------------------------------------

   echo "Running qrouter $version"
   echo "qrouter -c ${rootname}.cfg -p ${vddnet} -g ${gndnet} -d '${rootname}_route.rc' ${qrouter_options} ${rootname}" \
		 |& tee -a ${synthlog}
   ${bindir}/qrouter -c ${rootname}.cfg -p ${vddnet} -g ${gndnet} \
		-d "${rootname}_route.rc" ${qrouter_options} ${rootname} \
		|& tee -a ${synthlog} | \
		grep - -e Failed\ net -e fail -e Progress -e remaining.\*00\$ \
		-e remaining:\ \[1-9\]0\\\?\$ -e \\\*\\\*\\\*
endif

set errcond = $status
if ( ${errcond} != 0 ) then
   echo "qrouter failed with exit status ${errcond}" |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped on error condition." >>& ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Spot check:  Did qrouter produce file ${rootname}_route.def?
#---------------------------------------------------------------------

if ( ! -f ${rootname}_route.def || ( -M ${rootname}_route.def \
		< -M ${rootname}.def )) then
   echo "qrouter failure:  No output file ${rootname}_route.def." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

if ( ! -f ${rootname}_route.rc || ( -M ${rootname}_route.rc \
		< -M ${rootname}.def )) then
   echo "qrouter failure:  No delay file ${rootname}_route.rc." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Spot check:  Did qrouter produce file failed.out?
#---------------------------------------------------------------------

if ( -f failed.out && ( -M failed.out > -M ${rootname}.def )) then
   echo "qrouter failure:  Not all nets were routed." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Back-annotate everything including fill cells and added antenna cell
# connections by regenerating the structural verilog netlists from the
# post-layout DEF file.
#---------------------------------------------------------------------

echo "DEF2Verilog -v ${synthdir}/${rootname}.rtlnopwr.v -o ${synthdir}/${rootname}_postroute.v" \
	|& tee -a ${synthlog}
echo "-p ${vddnet} -g ${gndnet} ${lefoptions} ${rootname}_route.def" |& tee -a ${synthlog}
${bindir}/DEF2Verilog -v ${synthdir}/${rootname}.rtlnopwr.v \
		-o ${synthdir}/${rootname}_postroute.v \
		-p ${vddnet} -g ${gndnet} \
		${lefoptions} ${rootname}_route.def >>& ${synthlog}
set errcond = $status
if ( ${errcond} != 0 ) then
   echo "DEF2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped on error condition." >>& ${synthlog}
   exit 1
endif

#------------------------------------------------------------------
# Spot check:  Did DEF2Verilog produce an output file?
#------------------------------------------------------------------

if ( !( -f ${synthdir}/${rootname}_postroute.v )) then
   echo "DEF2Verilog failure:  No file ${rootname}_postroute.v." \
		|& tee -a ${synthlog}
   echo "RTL verilog and SPICE netlists may be invalid if there" \
		|& tee -a ${synthlog}
   echo "were antenna connections made by the router." |& tee -a ${synthlog}
   echo "Synthesis flow continuing, condition not fatal." >> ${synthlog}
else
   echo "" >> ${synthlog}
   echo "Generating RTL verilog and SPICE netlist file in directory" \
		|& tee -a ${synthlog}
   echo "   ${synthdir}" |& tee -a ${synthlog}

   cd ${synthdir}

   # Run vlog2Verilog and vlog2Spice on annotated netlist to get annotated
   # versions of the different verilog styles and the SPICE netlist.

   echo "Running vlog2Verilog." |& tee -a ${synthlog}
   echo "vlog2Verilog -c -v ${vddnet} -g ${gndnet} -o ${rootname}.rtl.anno.v ${rootname}_postroute.v" |& tee -a ${synthlog}
   ${bindir}/vlog2Verilog -c -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtl.anno.v ${rootname}_postroute.v >>& ${synthlog}

   set errcond = $status
   if ( ${errcond} != 0 ) then
      echo "vlog2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
      echo "Premature exit." |& tee -a ${synthlog}
      echo "Synthesis flow stopped on error condition." >>& ${synthlog}
      exit 1
   endif

   echo "vlog2Verilog -c -p -v ${vddnet} -g ${gndnet} -o ${rootname}.rtlnopwr.anno.v ${rootname}_postroute.v" |& tee -a ${synthlog}
   ${bindir}/vlog2Verilog -c -p -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtlnopwr.anno.v ${rootname}_postroute.v >>& ${synthlog}

   set errcond = $status
   if ( ${errcond} != 0 ) then
      echo "vlog2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
      echo "Premature exit." |& tee -a ${synthlog}
      echo "Synthesis flow stopped on error condition." >>& ${synthlog}
      exit 1
   endif

   echo "${bindir}/vlog2Verilog -c -b -p -n -v ${vddnet} -g ${gndnet} -o ${rootname}.rtlbb.anno.v ${rootname}_postroute.v" |& tee -a ${synthlog}
   ${bindir}/vlog2Verilog -c -b -p -n -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtlbb.anno.v ${rootname}_postroute.v >>& ${synthlog}

   set errcond = $status
   if ( ${errcond} != 0 ) then
      echo "vlog2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
      echo "Premature exit." |& tee -a ${synthlog}
      echo "Synthesis flow stopped on error condition." >>& ${synthlog}
      exit 1
   endif

   echo "Running vlog2Spice." |& tee -a ${synthlog}
   echo "vlog2Spice -i -l ${spicepath} -o ${rootname}.anno.spc ${rootname}.rtl.anno.v" |& tee -a ${synthlog}
   ${bindir}/vlog2Spice -i -l ${spicepath} -o ${rootname}.anno.spc \
		${rootname}.rtl.anno.v >>& ${synthlog}

   set errcond = ${status}
   if ( ${errcond} != 0 ) then
      echo "vlog2Spice failed with exit status ${errcond}" |& tee -a ${synthlog}
      echo "Premature exit." |& tee -a ${synthlog}
      echo "Synthesis flow stopped on error condition." >>& ${synthlog}
      exit 1
   endif

   #------------------------------------------------------------------
   # Spot check:  Did vlog2Verilog or vlog2Spice exit with an error?
   #------------------------------------------------------------------

   if ( ! -f ${rootname}.rtl.anno.v || ( -M ${rootname}.rtl.anno.v \
		< -M ${rootname}_postroute.v )) then
      echo "vlog2Verilog failure:  No file ${rootname}.rtl.anno.v created." \
		|& tee -a ${synthlog}
   endif

   if ( ! -f ${rootname}.rtlnopwr.anno.v || ( -M ${rootname}.rtlnopwr.anno.v \
		< -M ${rootname}_postroute.v )) then
      echo "vlog2Verilog failure:  No file ${rootname}.rtlnopwr.anno.v created." \
		|& tee -a ${synthlog}
   endif

   if ( ! -f ${rootname}.rtlbb.anno.v || ( -M ${rootname}.rtlbb.anno.v \
		< -M ${rootname}_postroute.v )) then
      echo "vlog2Verilog failure:  No file ${rootname}.rtlbb.anno.v created." \
		|& tee -a ${synthlog}
   endif

   if ( ! -f ${rootname}.anno.spc || ( -M ${rootname}.anno.spc \
		< -M ${rootname}_postroute.v )) then
      echo "vlog2Spice failure:  No file ${rootname}.anno.spc created." \
		|& tee -a ${synthlog}
   endif

   # Back to the layout directory
   cd ${layoutdir}

   # If the antenna.out file contained only unfixed errors, then
   # the annotated output files may not exist, so check.
   if ( -f ${synthdir}/${rootname}.rtlnopwr.anno.v ) then
      mv ${synthdir}/${rootname}.rtlnopwr.anno.v ${synthdir}/${rootname}.rtlnopwr.v 
   endif
   if ( -f ${synthdir}/${rootname}.rtl.anno.v ) then
      mv ${synthdir}/${rootname}.rtl.anno.v ${synthdir}/${rootname}.rtl.v 
   endif
   if ( -f ${synthdir}/${rootname}.rtlbb.anno.v ) then
      mv ${synthdir}/${rootname}.rtlbb.anno.v ${synthdir}/${rootname}.rtlbb.v 
   endif
   if ( -f ${synthdir}/${rootname}.anno.spc ) then
      mv ${synthdir}/${rootname}.anno.spc ${synthdir}/${rootname}.spc
   endif
endif

#---------------------------------------------------------------------
# If qrouter generated a ".cinfo" file, then annotate the ".cel"
# file, re-run placement, and re-run routing.  Note that this feature
# is not well refined, and currently not handled (qrouter standard
# route script does not generate congestion information on failure).
#---------------------------------------------------------------------

if (${scripting} == "T") then
   if ( -f ${rootname}.cinfo && ( -M ${rootname}.cinfo \
		> -M ${rootname}.def )) then
      ${scriptdir}/decongest.tcl ${rootname} ${lefpath} \
		${fillcell} |& tee -a ${synthlog}
   endif
endif

if ( -f ${rootname}_route.def ) then
   rm -f ${rootname}.def
   mv ${rootname}_route.def ${rootname}.def
endif

if ( -f ${rootname}_route.rc ) then
   rm -f ${rootname}.rc
   mv ${rootname}_route.rc ${rootname}.rc
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------

set endtime = `date`
echo "Router script ended on $endtime" >> $synthlog

exit 0
