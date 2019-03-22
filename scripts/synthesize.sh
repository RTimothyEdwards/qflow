#!/bin/tcsh -f
#
# synthesize.sh:
#-------------------------------------------------------------------------
#
# This script synthesizes verilog files for qflow using yosys
#
#-------------------------------------------------------------------------
# November 2006
# Steve Beccue and Tim Edwards
# MultiGiG, Inc.
# Scotts Valley, CA
# Updated 2013 Tim Edwards
# Open Circuit Design
#-------------------------------------------------------------------------

if ($#argv == 2 || $#argv == 3) then
   set projectpath=$argv[1]
   set modulename=$argv[2]
   if ($#argv == 3) then
      set sourcename=$argv[3]
   else 
      set sourcename=""
   endif
else
   echo Usage:  synthesize.sh <project_path> <module_name> [<source_file>]
   echo
   echo   where
   echo
   echo	      <project_path> is the name of the project directory containing
   echo			a file called qflow_vars.sh.
   echo
   echo	      <module_name> is the name of the verilog top-level module, and
   echo
   echo	      <source_file> is the name of the verilog file, if the module
   echo			name and file root name are not the same.
   echo
   echo	      Options are set from project_vars.sh.  Use the following
   echo	      variable names:
   echo
   echo			$yosys_options	for yosys
   echo			$yosys_script	for yosys
   echo			$nobuffers	to ignore output buffering
   echo			$inbuffers	to force input buffering
   echo			$fanout_options	for blifFanout
   exit 1
endif

#---------------------------------------------------------------------
# This script is called with the first argument <project_path>, which should
# have file "qflow_vars.sh".  Get all of our standard variable definitions
# from the qflow_vars.sh file.
#---------------------------------------------------------------------

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

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set synthlog=${logdir}/synth.log

# Reset all the logfiles
rm -f ${synthlog} >& /dev/null
rm -f ${logdir}/place.log >& /dev/null
rm -f ${logdir}/sta.log >& /dev/null
rm -f ${logdir}/route.log >& /dev/null
rm -f ${logdir}/post_sta.log >& /dev/null
rm -f ${logdir}/migrate.log >& /dev/null
rm -f ${logdir}/drc.log >& /dev/null
rm -f ${logdir}/lvs.log >& /dev/null
rm -f ${logdir}/gdsii.log >& /dev/null
touch ${synthlog}
set date=`date`
echo "Qflow synthesis logfile created on $date" > ${synthlog}

# Prepend techdir to libertyfile unless libertyfile begins with "/"
set abspath=`echo ${libertyfile} | cut -c1`
if ( "${abspath}" == "/" ) then
   set libertypath=${libertyfile}
else
   set libertypath=${techdir}/${libertyfile}
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

# Prepend techdir to leffile unless leffile begins with "/"
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

# Determine version of yosys
set versionstring = `${bindir}/yosys -V | cut -d' ' -f2`
#set major = `echo $versionstring | cut -d. -f1`
set major = 0
set minor = `echo $versionstring | cut -d. -f2`

# Sigh. . .  versioning doesn't follow any fixed standard
set minortest = `echo $minor | cut -d+ -f2`
set minor = `echo $minor | cut -d+ -f1`
if ( ${minortest} == "" ) then

   set revisionstring = `echo $versionstring | cut -d. -f3`
   if ( ${revisionstring} == "" ) set revisionstring = 0
   set revision = `echo $revisionstring | cut -d+ -f1`
   set subrevision = `echo $revisionstring | cut -d+ -f2`
   if ( ${subrevision} == "" ) set subrevision = 0

else
   set revision = 0
   set subrevision = ${minortest}

endif

# Check if "yosys_options" specifies a script to use for yosys.
if ( ! ${?yosys_options} ) then
   set yosys_options = ""
endif
set usescript = `echo ${yosys_options} | grep -- -s | wc -l`


# Only generate yosys script if none is specified in the yosys options
if ( ${usescript} == 0 ) then

cd ${sourcedir}

# Check for filelists variable overriding the default.  If it is set,
# use the value as the filename to get all the verilog source files
# used by the module.  If not, check for a file ${modulename}.fl and
# if it exists, use it (this is the default).  If not, then do an
# automatic search for files containing modules.

if ( ! ($?source_file_list) ) then
   # Check for ${modulename}.fl
   if ( -f ${modulename}.fl ) then
      set source_file_list = ${modulename}.fl
   endif
endif

if ( $?source_file_list ) then

   # Use file list

   set uniquedeplist = ""
   if ( $?is_system_verilog ) then
      if ( "$is_system_verilog" == "1" ) then
	 set svopt = "-sv"
      else
	 set svopt = ""
      endif
   else
      set svopt = ""
   endif

   cat > ${modulename}.ys << EOF
# Synthesis script for yosys created by qflow
read_liberty -lib -ignore_miss_dir -setattr blackbox ${libertypath}
EOF

   if ( ${?hard_macros} ) then
      foreach macro_path ( $hard_macros )
         foreach file ( `ls ${sourcedir}/${macro_path}` )
	    if ( ${file:e} == "lib" ) then
		cat >> ${modulename}.ys << EOF
read_liberty -lib -ignore_miss_dir -setattr blackbox ${sourcedir}/${macro_path}/${file}
EOF
		break
	    endif
	 end
      end
   endif

   set lines=`cat $source_file_list`
   set i=1
   while ( $i <= $#lines )
      echo "read_verilog ${svopt} $lines[$i]" >> ${modulename}.ys
      echo "Adding $lines[$i]"
      set uniquedeplist = "$uniquedeplist $lines[$i]"
      @ i = $i + 1
   end

   cat >> ${modulename}.ys << EOF
# Hierarchy check
hierarchy -check
EOF

   # Note:  Remove backslashes and brackets to avoid problems with tcsh
   set yerrors = `eval ${bindir}/yosys -s ${modulename}.ys |& sed -e "/\\/s#\\#/#g" \
		-e "/\[/s/\[//g" -e "/\]/s/\]//g" | grep ERROR`
   set yerrcnt = `echo $yerrors | wc -c`

   if ($yerrcnt > 1) then
      ${bindir}/yosys -s ${modulename}.ys >& ${synthlog}
      echo "Errors detected in verilog source, need to be corrected." \
		|& tee -a ${synthlog}
      echo "See file ${synthlog} for error output."
      echo "Synthesis flow stopped due to error condition." >> ${synthlog}
      exit 1
   endif
else

   #---------------------------------------------------------------------
   # Determine hierarchy by running yosys with a simple script to check
   # hierarchy.  Add files until yosys no longer reports an error.
   # Any error not related to a missing source file causes the script
   # to rerun yosys and dump error information into the log file, and
   # exit.
   #---------------------------------------------------------------------

   set uniquedeplist = ""
   set yerrcnt = 2

   while ($yerrcnt > 1)

   # Use is_system_verilog to declare that files with ".v" extension
   # are system verilog or system verilog compatible.

   if ( $?is_system_verilog ) then
      if ( "$is_system_verilog" == "1" ) then
         set svopt = "-sv"
      else
         set svopt = ""
      endif
   else
      set svopt = ""
   endif
 
   # Note:  While the use of read_liberty to allow structural verilog only
   # works in yosys 0.3.1 and newer, the following line works for the
   # purpose of querying the hierarchy in all versions.

   if ($sourcename == "") then
      set vext = "v"

      if ( !( -f ${modulename}.${vext} )) then
         set vext = "sv"
         set svopt = "-sv"
         if ( !( -f ${modulename}.${vext} )) then
            echo "Error:  Verilog source file ${modulename}.v (or .sv) cannot be found!" \
		|& tee -a ${synthlog}
            set svopt = ""
         else
            set sourcename = ${modulename}.${vext}
         endif
      else
         set sourcename = ${modulename}.${vext}
      endif
   else
      if ( !( -f ${sourcename} )) then
         echo "Error:  Verilog source file ${sourcename} cannot be found!" \
		|& tee -a ${synthlog}
      else
         set vext = ${sourcename:e}
         if ( ${vext} == "sv" ) then
	    set svopt = "-sv"
         endif
      endif
   endif

   if (${sourcename} == "") then
      echo "Synthesis flow stopped due to error condition." >> ${synthlog}
      exit 1
   endif

   cat > ${modulename}.ys << EOF
# Synthesis script for yosys created by qflow
read_liberty -lib -ignore_miss_dir -setattr blackbox ${libertypath}
EOF

   # Add all entries in variable "hard_macros".  "hard_macros" should be a
   # list of directories found in ${sourcedir}, each directory the module
   # name of a single module.  The directory contains several files for
   # each hard macro, including .lib, .lef, .spc, .v, and possibly others.

   if ( ${?hard_macros} ) then
       foreach macro_path ( $hard_macros )
           foreach file ( `ls ${sourcedir}/${macro_path}` )
	       if ( ${file:e} == "lib" ) then
		   cat >> ${modulename}.ys << EOF
read_liberty -lib -ignore_miss_dir -setattr blackbox ${sourcedir}/${macro_path}/${file}
EOF
		   break
	       endif
	   end
       end
   endif

   cat >> ${modulename}.ys << EOF
read_verilog ${svopt} ${sourcename}
EOF

   if ( ! ( $?vext ) ) then
      if ( "$svopt" == "" ) then
	 set vext = "v"
      else
	 set vext = "sv"
      endif
   endif

   foreach subname ( $uniquedeplist )
       if ( ${subname:e} == "" ) then
           if ( !( -f ${subname}.${vext} )) then
	       echo "Error:  Verilog source file ${subname}.${vext} cannot be found!" \
			|& tee -a ${synthlog}
	   endif
           echo "read_verilog ${svopt} ${subname}.${vext}" >> ${modulename}.ys
       else
           if ( !( -f ${subname} )) then
	       echo "Error:  Verilog source file ${subname} cannot be found!" \
			|& tee -a ${synthlog}
	   endif
           echo "read_verilog ${svopt} ${subname}" >> ${modulename}.ys
       endif
   end

   cat >> ${modulename}.ys << EOF
# Hierarchy check
hierarchy -check
EOF

   # Note:  Remove backslashes and brackets to avoid problems with tcsh
   set yerrors = `eval ${bindir}/yosys -s ${modulename}.ys |& sed -e "/\\/s#\\#/#g" \
		-e "/\[/s/\[//g" -e "/\]/s/\]//g" | grep ERROR`
   set yerrcnt = `echo $yerrors | wc -c`

   if ($yerrcnt > 1) then
      set yvalid = `echo $yerrors | grep "referenced in module" | wc -c`
      # Check error message specific to a missing source file.
      set ymissing = `echo $yerrors | grep "is not part of" | wc -c`
      if (($ymissing > 1) && ($yvalid > 1)) then
         set newdep = `echo $yerrors | cut -d " " -f 3 | cut -c3- | cut -d "'" -f 1`
         set uniquedeplist = "${uniquedeplist} ${newdep}"
      else
         ${bindir}/yosys -s ${modulename}.ys >& ${synthlog}
         echo "Errors detected in verilog source, need to be corrected." \
		|& tee -a ${synthlog}
         echo "See file ${synthlog} for error output."
         echo "Synthesis flow stopped due to error condition." >> ${synthlog}
         exit 1
      endif
   endif

   # end while ($yerrcnt > 1)
   end

# end if (! ($?source_file_list) )
endif

#---------------------------------------------------------------------
# Generate the main yosys script
#---------------------------------------------------------------------

set blif_opts = ""

# Set option for generating buffers
set blif_opts = "${blif_opts} -buf ${bufcell} ${bufpin_in} ${bufpin_out}"

# Set option for generating only the flattened top-level cell
# set blif_opts = "${blif_opts} ${modulename}"

cat > ${modulename}.ys << EOF
# Synthesis script for yosys created by qflow
EOF

# Support for structural verilog---any cell can be called as long as
# it has a liberty file entry to go along with it.  Standard cells
# are supported automatically.  All other cells should be put in the
# "hard_macro" variable.

cat >> ${modulename}.ys << EOF
read_liberty -lib -ignore_miss_dir -setattr blackbox ${libertypath}
EOF

if ( ${?hard_macros} ) then
    foreach macro_path ( $hard_macros )
        foreach file ( `ls ${sourcedir}/${macro_path}` )
	    if ( ${file:e} == "lib" ) then
		cat >> ${modulename}.ys << EOF
read_liberty -lib -ignore_miss_dir -setattr blackbox ${sourcedir}/${macro_path}/${file}
EOF
		break
	    endif
	end
    end
endif

# NOTE:  Using source_file_list, all files go into $uniquedeplist;  there
# is no additional ${sourcename} file.

if ( ! ( $?source_file_list )) then
   if ( ${sourcename} != "" ) then
      cat >> ${modulename}.ys << EOF
read_verilog ${svopt} ${sourcename}
EOF
   endif
endif

if ( ! ( $?vext ) ) then
   if ( "$svopt" == "" ) then
      set vext = "v"
   else
      set vext = "sv"
   endif
endif

foreach subname ( $uniquedeplist )
    if ( ${subname:e} == "" ) then
       echo "read_verilog ${svopt} ${subname}.${vext}" >> ${modulename}.ys
    else
       echo "read_verilog ${svopt} ${subname}" >> ${modulename}.ys
    endif
end

# Will not support yosys 0.0.x syntax; flag a warning instead

if ( ${major} == 0 && ${minor} == 0 ) then
   echo "Warning: yosys 0.0.x unsupported.  Please update!"
   echo "Output is likely to be incompatible with qflow."
endif

if ( ${major} == 0 && ${minor} < 5 ) then

cat >> ${modulename}.ys << EOF
# High-level synthesis
hierarchy -top ${modulename}
EOF

endif

if ( ${?yosys_script} ) then
   if ( -f ${yosys_script} ) then
      cat ${yosys_script} >> ${modulename}.ys
   else
      echo "Error: yosys script ${yosys_script} specified but not found"
   endif
else if ( ${major} != 0 || ${minor} >= 5 ) then

   cat >> ${modulename}.ys << EOF

# High-level synthesis
synth -top ${modulename}
EOF

else

   cat >> ${modulename}.ys << EOF

# High-level synthesis
proc; memory; opt; fsm; opt

# Map to internal cell library
techmap; opt
EOF

endif

cat >> ${modulename}.ys << EOF
# Map register flops
dfflibmap -liberty ${libertypath}
opt

EOF

if ( ${?abc_script} ) then
   if ( ${abc_script} != "" ) then
      cat >> ${modulename}.ys << EOF
abc -exe ${bindir}/yosys-abc -liberty ${libertypath} -script ${abc_script}
flatten

EOF
   else
      echo "Warning: no abc script ${abc_script}, using default, no script" \
		|& tee -a ${synthlog}
      cat >> ${modulename}.ys << EOF
abc -exe ${bindir}/yosys-abc -liberty ${libertypath}
flatten

EOF
   endif
else
   cat >> ${modulename}.ys << EOF
# Map combinatorial cells, standard script
abc -exe ${bindir}/yosys-abc -liberty ${libertypath} -script +strash;scorr;ifraig;retime,{D};strash;dch,-f;map,-M,1,{D}
flatten

EOF
endif

# Purge buffering of internal net name aliases.  Option "debug"
# retains all internal names by buffering them, resulting in a
# larger layout (especially for layouts derived from hierarchical
# source), but one in which all signal names from the source can
# be probed.

if ( ! ${?yosys_debug} ) then
   cat >> ${modulename}.ys << EOF
clean -purge
EOF
endif

# Map tiehi and tielo, if they are defined

if ( ${?tiehi} && ${?tiehipin_out} ) then
   if ( "${tiehi}" != "" ) then
      echo "hilomap -hicell $tiehi $tiehipin_out" >> ${modulename}.ys  
   endif
endif

if ( ${?tielo} && ${?tielopin_out} ) then
   if ( "${tielo}" != "" ) then
      echo "hilomap -locell $tielo $tielopin_out" >> ${modulename}.ys  
   endif
endif

# Output buffering, if not specifically prevented
if ( ${major} > 0 || ${minor} > 1 ) then
   if (!($?nobuffers)) then
       cat >> ${modulename}.ys << EOF
# Output buffering
iopadmap -outpad ${bufcell} ${bufpin_in}:${bufpin_out} -bits
EOF
   endif
   if ($?inbuffers) then
       cat >> ${modulename}.ys << EOF
# Input buffering
iopadmap -inpad ${bufcell} ${bufpin_out}:${bufpin_in} -bits
EOF
   endif
endif

cat >> ${modulename}.ys << EOF
# Cleanup
opt
clean
rename -enumerate
write_blif ${blif_opts} ${modulename}_mapped.blif
stat
EOF

endif # Generation of yosys script if ${usescript} == 0


#---------------------------------------------------------------------
# Yosys synthesis
#---------------------------------------------------------------------

# If there is a file ${modulename}_mapped.blif, move it to a temporary
# place so we can see if yosys generates a new one or not.

if ( -f ${modulename}_mapped.blif ) then
   mv ${modulename}_mapped.blif ${modulename}_mapped_orig.blif
endif

echo "Running yosys for verilog parsing and synthesis" |& tee -a ${synthlog}
# If provided own script yosys call that, otherwise call yosys with generated script.
if ( ${usescript} == 1 ) then
   echo "yosys ${yosys_options}" |& tee -a ${synthlog}
   eval ${bindir}/yosys ${yosys_options} |& tee -a ${synthlog}
else
   echo "yosys ${yosys_options} -s ${modulename}.ys" |& tee -a ${synthlog}
   eval ${bindir}/yosys ${yosys_options} -s ${modulename}.ys |& tee -a ${synthlog}
endif

#---------------------------------------------------------------------
# Spot check:  Did yosys produce file ${modulename}_mapped.blif?
#---------------------------------------------------------------------

if ( !( -f ${modulename}_mapped.blif )) then
   echo "outputprep failure:  No file ${modulename}_mapped.blif." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   # Replace the old blif file, if we had moved it
   if ( -f ${modulename}_mapped_orig.blif ) then
      mv ${modulename}_mapped_orig.blif ${modulename}_mapped.blif
   endif
   exit 1
else
   # Remove the old blif file, if we had moved it
   if ( -f ${modulename}_mapped_orig.blif ) then
      rm ${modulename}_mapped_orig.blif
   endif
endif

#---------------------------------------------------------------------
# Get power and ground nets, and save them in a file in synthdir
#---------------------------------------------------------------------

# First set defaults so vddnet and gndnet are defined to something
# if they are not set in the tech variables file.
if !($?vddnet) then
    set vddnet = "VDD"
endif
if !($?gndnet) then
    set gndnet = "GND"
endif

echo "Running getpowerground to determine power and ground net names." |& tee -a ${synthlog}
echo "getpowerground.tcl ${lefpath}" |& tee -a ${synthlog}
set powerground = `${scriptdir}/getpowerground.tcl ${lefpath} | grep =`
set testnet = `echo $powerground | cut -d' ' -f1`
set testnettype = `echo $testnet | cut -d= -f1`
set testnetname = `echo $testnet | cut -d= -f2`
if ( "$testnetname" != "" ) then
   set $testnettype = $testnetname
endif
set testnet = `echo $powerground | cut -d' ' -f2`
set testnettype = `echo $testnet | cut -d= -f1`
set testnetname = `echo $testnet | cut -d= -f2`
if ( "$testnetname" != "" ) then
   set $testnettype = $testnetname
endif

echo set vddnet=\"${vddnet}\" > ${synthdir}/${modulename}_powerground
echo set gndnet=\"${gndnet}\" >> ${synthdir}/${modulename}_powerground

#----------------------------------------------------------------------
# Run ypostproc syntax post-processor
#----------------------------------------------------------------------

if (! $?postproc_options) then
   set postproc_options=""
else
   if ( $postproc_options == "-anchors" ) then
      if ( $?antennacell ) then
          echo "Running getantennacell to determine cell to use for anchoring." |& tee -a ${synthlog}
          echo "getantennacell.tcl $modulename ${lefpath} $antennacell" |& tee -a ${synthlog}
          set anchorinfo=`${scriptdir}/getantennacell.tcl $modulename ${lefpath} $antennacell  | grep antenna= | cut -d= -f2`
          set postproc_options = "-anchor=$anchorinfo"
      else
          echo "Antenna anchoring requested but no antenna cell defined in tech."
          set postproc_options = ""
      endif
   endif
endif

echo "Cleaning up output syntax" |& tee -a ${synthlog}
echo "ypostproc.tcl ${modulename}_mapped.blif ${modulename} ${techdir}/${techname}.sh ${vddnet} ${gndnet} ${postproc_options}" \
	|& tee -a ${synthlog}
${scriptdir}/ypostproc.tcl ${modulename}_mapped.blif ${modulename} \
		${techdir}/${techname}.sh ${vddnet} ${gndnet} ${postproc_options}
set errcond = ${status}
if ( $errcond != 0 ) then
   echo "ypostproc failure.  See file ${synthlog} for error messages." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#----------------------------------------------------------------------
# Add buffers in front of all outputs (for yosys versions before 0.2.0)
#----------------------------------------------------------------------

if ( ${major} == 0 && ${minor} < 2 ) then
   if ($?nobuffers) then
      set final_blif = "${modulename}_mapped_tmp.blif"
   else
      echo "Adding output buffers"
      ${scriptdir}/ybuffer.tcl ${modulename}_mapped_tmp.blif \
		${modulename}_mapped_buf.blif ${techdir}/${techname}.sh
      set final_blif = "${modulename}_mapped_buf.blif"
   endif
else
   # Buffers already handled within yosys
   set final_blif = "${modulename}_mapped_tmp.blif"
endif

#---------------------------------------------------------------------
# The following definitions will replace "LOGIC0" and "LOGIC1"
# with buffers from gnd and vdd, respectively.  This takes care
# of technologies where tie-low and tie-high cells are not
# defined.
#---------------------------------------------------------------------

echo "Cleaning up blif file syntax" |& tee -a ${synthlog}

if ( "$tielo" == "") then
   set subs0a="/LOGIC0/s/O=/${bufpin_in}=gnd ${bufpin_out}=/"
   set subs0b="/LOGIC0/s/LOGIC0/${bufcell}/"
else
   set subs0a=""
   set subs0b=""
endif

if ( "$tiehi" == "") then
   set subs1a="/LOGIC1/s/O=/${bufpin_in}=vdd ${bufpin_out}=/"
   set subs1b="/LOGIC1/s/LOGIC1/${bufcell}/"
else
   set subs1a=""
   set subs1b=""
endif

#---------------------------------------------------------------------
# Remove backslashes, references to "$techmap", and
# make local input nodes of the form $0node<a:b><c> into the
# form node<c>_FF_INPUT
#---------------------------------------------------------------------

cat ${final_blif} | sed \
	-e "$subs0a" -e "$subs0b" -e "$subs1a" -e "$subs1b" \
	-e 's/\\\([^$]\)/\1/g' \
	-e 's/$techmap//g' \
	-e 's/$0\([^ \t<]*\)<[0-9]*:[0-9]*>\([^ \t]*\)/\1\2_FF_INPUT/g' \
	> ${synthdir}/${modulename}.blif

# Switch to synthdir for processing of the BDNET netlist
cd ${synthdir}

#---------------------------------------------------------------------
# If "nofanout" is set, then don't run blifFanout.
#---------------------------------------------------------------------

if ($?nofanout) then
   set nchanged=0
else

#---------------------------------------------------------------------
# Make a copy of the original blif file, as this will be overwritten
# by the fanout handling process
#---------------------------------------------------------------------

   cp ${modulename}.blif ${modulename}_bak.blif

#---------------------------------------------------------------------
# Check all gates for fanout load, and adjust gate strengths as
# necessary.  Iterate this step until all gates satisfy drive
# requirements.
#
# Use option "-c value" in fanout_options to force a value for the
# (maximum expected) output load, in fF (default is 30fF)
# Use option "-l value" in fanout_options to force a value for the
# maximum latency, in ps (default is 1000ps)
#---------------------------------------------------------------------

   rm -f ${modulename}_nofanout
   echo $gndnet > ${modulename}_nofanout
   echo $vddnet >> ${modulename}_nofanout

   if (! $?fanout_options) then
      set fanout_options=""
   endif

   if (-f ${libertypath} && -f ${bindir}/blifFanout ) then

      if ("x${separator}" == "x") then
	 set sepoption=""
      else
	 set sepoption="-s ${separator}"
      endif
      if ($?clkbufcell) then
	 if ("x${clkbufcell}" == "x") then
	    if ("x${bufcell}" == "x") then
	       set bufoption=""
	    else
	       set bufoption="-b ${bufcell} -i ${bufpin_in} -o ${bufpin_out}"
	    endif
	 else
	    if ("x${bufcell}" == "x") then
	       set bufoption="-b ${clkbufcell} -i ${bufpin_in} -o ${bufpin_out}"
	    else
	       set bufoption="-b ${bufcell},${clkbufcell} -i ${bufpin_in},${clkbufpin_in} -o ${bufpin_out},${clkbufpin_out}"
	    endif
	 endif
      else
	 if ("x${bufcell}" == "x") then
	    set bufoption=""
	 else
	    set bufoption="-b ${bufcell} -i ${bufpin_in} -o ${bufpin_out}"
	 endif
      endif

      # Add paths to liberty files for any hard macros
      set libertyoption="-p ${libertypath}"
      if ( ${?hard_macros} ) then
         # Do not use the standard cell separator on hard macros!
	 # Option "-S" sets the separator to null.
	 set libertyoption="${libertyoption} -S"
	 foreach macro_path ( $hard_macros )
	    foreach file ( `ls ${sourcedir}/${macro_path}` )
	       if ( ${file:e} == "lib" ) then
	           set libertyoption="${libertyoption} -p ${sourcedir}/${macro_path}/${file}"
	       endif
	    end
         end
      endif

      echo "Running blifFanout (iterative)" |& tee -a ${synthlog}
      echo "Each iteration calls:" |& tee -a ${synthlog}
      echo "blifFanout ${fanout_options} -I ${modulename}_nofanout ${sepoption} ${libertyoption} ${bufoption} tmp.blif ${modulename}.blif" |& tee -a ${synthlog}
      echo "" >> ${synthlog}

      set nchanged=1000
      while ($nchanged > 0)
         mv ${modulename}.blif tmp.blif
         set nchanged = `${bindir}/blifFanout ${fanout_options} \
		-I ${modulename}_nofanout ${libertyoption} ${sepoption} \
		${bufoption} tmp.blif ${modulename}.blif |& tee -a ${synthlog} | \
		grep "changed:" | cut -f5 -d' '`

         set errcond=$status
	 if ( $errcond > 0 ) then
	    echo "blifFanout failed with error condition ${errcond}." \
			|& tee -a ${synthlog}
	    echo "Premature exit." |& tee -a ${synthlog}
	    echo "Synthesis flow stopped due to error condition." >> ${synthlog}
	    exit 1
	 else
            echo "gates resized: $nchanged" |& tee -a ${synthlog}
	 endif
      end
   else
      echo "blifFanout not run:  No cell size optimization." |& tee -a ${synthlog}
      set nchanged=0
   endif
endif

#---------------------------------------------------------------------
# Spot check:  Did blifFanout produce an error?
#---------------------------------------------------------------------

if ( $nchanged < 0 ) then
   echo "blifFanout failure.  See file ${synthlog} for error messages." \
	|& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

echo "" >> ${synthlog}
echo "Generating RTL verilog and SPICE netlist file in directory" \
		|& tee -a ${synthlog}
echo "	 ${synthdir}" |& tee -a ${synthlog}
echo "Files:" |& tee -a ${synthlog}
echo "   Verilog: ${synthdir}/${modulename}.rtl.v" |& tee -a ${synthlog}
echo "   Verilog: ${synthdir}/${modulename}.rtlnopwr.v" |& tee -a ${synthlog}
echo "   Verilog: ${synthdir}/${modulename}.rtlbb.v" |& tee -a ${synthlog}
echo "   Spice:   ${synthdir}/${modulename}.spc" |& tee -a ${synthlog}
echo "" >> ${synthlog}

echo "Running blif2Verilog." |& tee -a ${synthlog}
${bindir}/blif2Verilog -c -v ${vddnet} -g ${gndnet} ${modulename}.blif \
	> ${modulename}.rtl.v

# Version without explicit power and ground
${bindir}/blif2Verilog -c -p -v ${vddnet} -g ${gndnet} ${modulename}.blif \
	> ${modulename}.rtlnopwr.v

# Version without vectors
${bindir}/blif2Verilog -c -p -b -n -v ${vddnet} -g ${gndnet} ${modulename}.blif \
	> ${modulename}.rtlbb.v

#---------------------------------------------------------------------
# Spot check:  Did blif2Verilog exit with an error?
# Note that these files are not critical to the main synthesis flow,
# so if they are missing, we flag a warning but do not exit.
#---------------------------------------------------------------------

if ( !( -f ${modulename}.rtl.v || \
        ( -M ${modulename}.rtl.v < -M ${modulename}.blif ))) then
   echo "blif2Verilog failure:  No file ${modulename}.rtl.v created." \
                |& tee -a ${synthlog}
endif

if ( !( -f ${modulename}.rtlnopwr.v || \
        ( -M ${modulename}.rtlnopwr.v < -M ${modulename}.blif ))) then
   echo "blif2Verilog failure:  No file ${modulename}.rtlnopwr.v created." \
                |& tee -a ${synthlog}
endif

if ( !( -f ${modulename}.rtlbb.v || \
        ( -M ${modulename}.rtlbb.v < -M ${modulename}.blif ))) then
   echo "blif2Verilog failure:  No file ${modulename}.rtlbb.v created." \
                |& tee -a ${synthlog}
endif

#---------------------------------------------------------------------

echo "Running blif2BSpice." |& tee -a ${synthlog}
if ("x${spicefile}" == "x") then
    set spiceopt=""
else
    set spiceopt="-l ${spicepath}"
endif
${bindir}/blif2BSpice -i -p ${vddnet} -g ${gndnet} ${spiceopt} \
	${modulename}.blif > ${modulename}.spc

#---------------------------------------------------------------------
# Spot check:  Did blif2BSpice exit with an error?
# Note that these files are not critical to the main synthesis flow,
# so if they are missing, we flag a warning but do not exit.
#---------------------------------------------------------------------

if ( !( -f ${modulename}.spc || \
        ( -M ${modulename}.spc < -M ${modulename}.blif ))) then
   echo "blif2BSpice failure:  No file ${modulename}.spc created." \
                |& tee -a ${synthlog}
else

   set test=`which python3`
   if ( $status == 0 ) then
       echo "Running spi2xspice.py" |& tee -a ${synthlog}
       if (!( ${?xspice_options} )) then
           set xspice_options=""
       endif

       # Add paths to liberty files for any hard macros
       set libertyoption="${libertypath}"
       if ( ${?hard_macros} ) then
	  foreach macro_path ( $hard_macros )
	     foreach file ( `ls ${sourcedir}/${macro_path}` )
	        if ( ${file:e} == "lib" ) then
	           set libertyoption="${libertyoption} ${sourcedir}/${macro_path}/${file}"
		endif
	     end
	  end
       endif

       echo spi2xspice.py '"'${libertyoption}'"' ${xspice_options} ${modulename}.spc ${modulename}.xspice |& tee -a ${synthlog}
       echo "" >> ${synthlog}

       ${scriptdir}/spi2xspice.py "${libertyoption}" ${xspice_options} \
		${modulename}.spc ${modulename}.xspice

       if ( !( -f ${modulename}.xspice || \
		( -M ${modulename}.xspice < -M ${modulename}.spc ))) then
          echo "spi2xspice.py failure:  No file ${modulename}.xspice created." \
		|& tee -a ${synthlog}
       endif
   else
       echo "No python3 on system, not running spi2xspice.py"
   endif

endif

#---------------------------------------------------------------------

cd ${projectpath}
set endtime = `date`
echo "Synthesis script ended on $endtime" >> $synthlog
