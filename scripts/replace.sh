#!/bin/tcsh -f
#----------------------------------------------------------
# Placement script using abk-openroad RePlAce
#
#----------------------------------------------------------
# Tim Edwards, 12/26/18, for Open Circuit Design
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  replace.sh <project_path> <source_name>
   exit 1
endif

# Split out options from the main arguments
set argline=(`getopt "kdf" $argv[1-]`)
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc == 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo Usage:  replace.sh [options] <project_path> <source_name>
   echo   where
   echo       <project_path> is the name of the project directory containing
   echo                 a file called qflow_vars.sh.
   echo       <source_name> is the root name of the verilog file, and
   echo       [options] are:
   echo			-k	keep working files
   echo			-d	generate DEF file for routing
   echo			-f	final placement.  Don't run if routing succeeded
   echo
   echo	  Options to specific tools can be specified with the following
   echo	  variables in project_vars.sh:
   echo
   echo
   exit 1
endif

set keep=0
set makedef=0
set final = 0

foreach option (${argline})
   switch (${option})
      case -k:
         set keep=1
         breaksw
      case -d:
         set makedef=1
         breaksw
      case -f:
         set final=1
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

if (! ${?qrouter_nocleanup} ) then
   set qrouter_nocleanup = ""
else
   if (${qrouter_nocleanup} == 0) then
      set qrouter_nocleanup = ""
   else
      set qrouter_nocleanup = "true"
   endif
endif

if (!($?logdir)) then
   set logdir=${projectpath}/log
endif
mkdir -p ${logdir}
set lastlog=${logdir}/synth.log
set synthlog=${logdir}/place.log
rm -f ${synthlog} >& /dev/null
rm -f ${logdir}/sta.log >& /dev/null
rm -f ${logdir}/route.log >& /dev/null
rm -f ${logdir}/post_sta.log >& /dev/null
rm -f ${logdir}/migrate.log >& /dev/null
rm -f ${logdir}/drc.log >& /dev/null
rm -f ${logdir}/lvs.log >& /dev/null
rm -f ${logdir}/gdsii.log >& /dev/null
touch ${synthlog}
set date=`date`
echo "Qflow placement logfile created on $date" > ${synthlog}

#----------------------------------------------------------
# Done with initialization
#----------------------------------------------------------

# Check if last line of log file says "error condition"
set errcond = `tail -1 ${lastlog} | grep "error condition" | wc -l`
if ( ${errcond} == 1 ) then
   echo "Synthesis flow stopped on error condition.  Placement will not proceed"
   echo "until error condition is cleared."
   exit 1
endif

#---------------------------------------------------
# Create .info file from qrouter
#---------------------------------------------------

cd ${layoutdir}

# First prepare a simple .cfg file that can be used to point qrouter
# to the LEF files when generating layer information using the "-i" option.
# This also contains the scaling units used in the .cel and .def files.

#------------------------------------------------------------------
# Determine the version number and availability of scripting
# in qrouter.
#------------------------------------------------------------------

set version=`${bindir}/qrouter -v 0 -h | tail -1`
set major=`echo $version | cut -d. -f1`
set minor=`echo $version | cut -d. -f2`
set subv=`echo $version | cut -d. -f3`
set scripting=`echo $version | cut -d. -f4`

# Create the initial (bootstrap) configuration file

if ( $scripting == "T" ) then
   if ( "$techleffile" == "" ) then
      echo "read_lef ${lefpath}" > ${rootname}.cfg
   else
      echo "read_lef ${techlefpath}" > ${rootname}.cfg
   endif
else
   if ( "$techleffile" == "" ) then
      echo "lef ${lefpath}" > ${rootname}.cfg
   else
      echo "lef ${techlefpath}" > ${rootname}.cfg
   endif
endif

${bindir}/qrouter -i ${rootname}.info -c ${rootname}.cfg

#---------------------------------------------------------------------
# Spot check:  Did qrouter produce file ${rootname}.info?
#---------------------------------------------------------------------

if ( ! -f ${rootname}.info || \
	( -M ${rootname}.info < -M ${rootname}.pin )) then
   echo "qrouter (-i) failure:  No file ${rootname}.info." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

# Pull scale units from the .info file.  Default units are centimicrons.
set units=`cat ${rootname}.info | grep units | cut -d' ' -f3`
if ( "${units}" == "" ) then
    set units=100
endif

#-------------------------------------------------------------------------
# Create the pre-placement .def file for RePlAce
#-------------------------------------------------------------------------

cd ${projectpath}

if ( "$techleffile" == "" ) then
    set lefoptions=""
    set reploptions=""
else
    set lefoptions="-l ${techlefpath}"
    set reploptions="-lef ${techlefpath}"
endif
set lefoptions="${lefoptions} -l ${lefpath}"
set reploptions="${reploptions} -lef ${lefpath}"

# Pass additional .lef files to vlog2Def and DEF2Verilog from the hard macros list

if ( ${?hard_macros} ) then
    foreach macro_path ( $hard_macros )
	foreach file ( `ls ${sourcedir}/${macro_path}` )
	    if ( ${file:e} == "lef" ) then
		set lefoptions="${lefoptions} -l ${sourcedir}/${macro_path}/${file}"
	    endif
	end
    end
endif

if ( ${?initial_density} ) then
   set vlog2defopts = "-d ${initial_density}"
else
   set vlog2defopts = ""
endif

echo "Running vlog2Def to generate input files for graywolf" |& tee -a ${synthlog}
echo "vlog2Def ${lefoptions} -u $units ${vlog2defopts} -o ${layoutdir}/${rootname}_preplace.def ${synthdir}/${rootname}.rtlnopwr.v" |& tee -a ${synthlog}

${bindir}/vlog2Def ${lefoptions} -u $units ${vlog2defopts} -o ${layoutdir}/${rootname}_preplace.def \
	${synthdir}/${rootname}.rtlnopwr.v >>& ${synthlog}

set errcond = $status
if ( ${errcond} != 0 ) then
   echo "vlog2Def failed with exit status ${errcond}" |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped on error condition." >>& ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Spot check:  Did vlog2Def produce file ${rootname}_preplace.def?
#---------------------------------------------------------------------

if ( ! -f ${layoutdir}/${rootname}_preplace.def || \
	( -M ${layoutdir}/${rootname}_preplace.def \
	< -M ${rootname}.rtlnopwr.v )) then
   echo "vlog2Def failure:  No file ${rootname}_preplace.def." |& tee -a ${synthlog}
   echo "vlog2Def was called with arguments: ${lefpath} "
   echo "	-u $units -o ${layoutdir}/${rootname}_preplace.def"
   echo "	${synthdir}/${rootname}.rtlnopwr.v"
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Move to the layout directory
#---------------------------------------------------------------------

cd ${layoutdir}

#---------------------------------------------------------------------
# To do:  Add behavior to manually define blockages.
#---------------------------------------------------------------------

# Set value ${fillers} to be equal either to a single cell name if
# only one of (decapcell, antennacell, fillcell, bodytiecell) is
# defined, or a comma-separated quadruplet.  If only one is defined,
# then "fillcell" is set to that name for those scripts that only
# handle one kind of fill cell.

# Make sure all cell types are defined, but an empty string if not used.

if ( ! ${?fillcell} ) then
   set fillcell = ""
endif
if ( ! ${?decapcell} ) then
   set decapcell = ""
endif
if ( ! ${?antennacell} ) then
   set antennacell = ""
endif
if ( ! ${?bodytiecell} ) then
   set bodytiecell = ""
endif

set fillers = "${fillcell},${decapcell},${antennacell},${bodytiecell}"

# For tools that only require one fill cell as option, make sure that
# there is a valid fill cell type
if ("x$fillcell" == "x") then
   set fillcell = $decapcell
endif
if ("x$fillcell" == "x") then
   set fillcell = $antennacell
endif
if ("x$fillcell" == "x") then
   set fillcell = $bodytiecell
endif
if ("x$fillcell" == "x") then
      # There is no fill cell, which is likely to produce poor results.
      echo "Warning:  No fill cell types are defined in the tech setup script."
      echo "This is likely to produce poor layout and/or poor routing results."
endif

#---------------------------------------------------------------------
# To do:  Define a way to pass pin placement hints to RePlAce
#---------------------------------------------------------------------

#-----------------------------------------------
# 1) Run RePlAce
#-----------------------------------------------

if ( !( ${?replace_options} )) then
   # Some defaults (to be refined)
   set replace_options = "-bmflag ispd"
   set replace_options = "${replace_options} ${reploptions}"
   set replace_options = "${replace_options} -def ${layoutdir}/${rootname}_preplace.def"
   set replace_options = "${replace_options} -output outputs"
   set replace_options = "${replace_options} -dpflag NTU3"
   set replace_options = "${replace_options} -dploc ${bindir}/ntuplace3"
endif

#----------------------------------------------------------------------------
# Set -den option to RePlAce if initial_density is defined in project_vars.sh

if ( ${?initial_density} ) then
   set replace_options = "${replace_options} -den ${initial_density}"
endif

echo "Running RePlAce placement" |& tee -a ${synthlog}
echo "RePlAce ${replace_options}" |& tee -a ${synthlog}
${bindir}/RePlAce ${replace_options} >>& ${synthlog}

set errcond = $status
if ( ${errcond} != 0 ) then
   echo "RePlAce failed with exit status ${errcond}" |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped on error condition." >>& ${synthlog}
   exit 1
endif

#---------------------------------------------------------------------
# Spot check:  Did RePlAce produce file ${rootname}_preplace_final.def?
#---------------------------------------------------------------------

set outfile=`ls outputs/ispd/${rootname}_preplace/experiment*/${rootname}_preplace_final.def --sort=time | head -1`

if ( ! -f ${outfile} || \
	( -M ${outfile} < -M ${rootname}_preplace.def )) then
   echo "RePlAce failure:  No file ${rootname}_preplace_final.def." |& tee -a ${synthlog}
   echo "Premature exit." |& tee -a ${synthlog}
   echo "Synthesis flow stopped due to error condition." >> ${synthlog}
   exit 1
endif

echo "Copying RePlAce result up to layout directory:" |& tee -a ${synthlog}
echo "cp ${outfile} ${rootname}.def" |& tee -a ${synthlog}
cp ${outfile} ${rootname}.def

#---------------------------------------------------
# Remove RePlAce working files
#---------------------------------------------------

if ($keep == 0) then
   rm -rf outputs
endif

#---------------------------------------------------------------
# 2) Run clock tree synthesis and back-annotate netlists
#---------------------------------------------------------------

#---------------------------------------------------
# NOTE:  This should be in the qrouter script. . .
# 3) Prepare .cfg file for qrouter
#---------------------------------------------------

if ($makedef == 1) then

   echo "Running getantennacell to determine cell to use for antenna anchors." \
	|& tee -a ${synthlog}
   echo "getantennacell.tcl $rootname ${lefpath} $antennacell" |& tee -a ${synthlog}
   set useantennacell=`${scriptdir}/getantennacell.tcl $rootname \
	${lefpath} $antennacell  | grep antenna= | cut -d= -f2 | cut -d/ -f1`

   if ( "${useantennacell}" != "" ) then
      echo "Using cell ${useantennacell} for antenna anchors" |& tee -a ${synthlog}
   endif

   #---------------------------------------------------------------------
   # Add spacer cells to create a straight border on the right side
   # Add power stripes, stretching the cell if specified
   #---------------------------------------------------------------------

   if ( !(${?nospacers}) && (-f ${bindir}/addspacers) ) then

      # Fill will use just the fillcell for padding under power buses
      # and on the edges (to do:  refine this to use other spacer types
      # if the width options are more flexible).

      if ( !( ${?addspacers_options} )) then
         set addspacers_options = ""
      endif
      set addspacers_options = "${addspacers_options} -p ${vddnet} -g ${gndnet} -f ${fillcell} -O"

      echo "Running addspacers to generate power stripes and align cell right edge" \
		|& tee -a ${synthlog}
      echo "addspacers ${addspacers_options} ${lefoptions} -o ${rootname}_filled.def ${rootname}" \
		|& tee -a ${synthlog}

      rm -f ${rootname}_filled.def
      ${bindir}/addspacers ${addspacers_options} ${lefoptions} \
		-o ${rootname}_filled.def ${rootname} >>& ${synthlog}
      set errcond = $status
      if ( ${errcond} != 0 ) then
	 echo "addspacers failed with exit status ${errcond}" |& tee -a ${synthlog}
	 echo "Premature exit." |& tee -a ${synthlog}
	 echo "Synthesis flow stopped on error condition." >>& ${synthlog}
	 exit 1
      endif

      if ( -f ${rootname}_filled.def ) then
	 mv ${rootname}_filled.def ${rootname}.def
      endif

      if ( -f ${rootname}.obsx ) then
         # If addspacers annotated the .obs (obstruction) file, then
         # overwrite the original.
	 mv ${rootname}.obsx ${rootname}.obs
      endif
   endif

   # Copy the .def file to a backup called "unroute"
   cp ${rootname}.def ${rootname}_unroute.def

   # If the user didn't specify a number of layers for routing as part of
   # the project variables, then the info file created by qrouter will have
   # as many lines as there are route layers defined in the technology LEF
   # file.

   if ( !( ${?route_layers} )) then
      set route_layers = `cat ${rootname}.info | grep -e horizontal -e vertical | wc -l`
   endif

   # Create the main configuration file

   # Variables "via_pattern" (none, normal, invert), "via_stacks",
   # and "via_use" can be specified in the tech script, and are
   # appended to the qrouter configuration file.  via_stacks defaults
   # to 2 if not specified.  It can be overridden from the user's .cfg2
   # file.

   if (${scripting} == "T") then
      echo "# qrouter runtime script for project ${rootname}" > ${rootname}.cfg
      echo "" >> ${rootname}.cfg
      echo "verbose 1" >> ${rootname}.cfg
      if ( "$techleffile" != "" ) then
         echo "read_lef ${techlefpath}" >> ${rootname}.cfg
      endif
      echo "read_lef ${lefpath}" >> ${rootname}.cfg

      if ( ${?hard_macros} ) then
          foreach macro_path ( $hard_macros )
	      foreach file ( `ls ${sourcedir}/${macro_path}` )
		  if ( ${file:e} == "lef" ) then
		      echo "read_lef ${sourcedir}/${macro_path}/${file}" >> ${rootname}.cfg
	          endif
	      end
	  end
      endif

      echo "catch {layers ${route_layers}}" >> ${rootname}.cfg
      if ( ${?via_use} ) then
         echo "" >> ${rootname}.cfg
         echo "via use ${via_use}" >> ${rootname}.cfg
      endif
      if ( ${?via_pattern} ) then
         echo "" >> ${rootname}.cfg
         echo "via pattern ${via_pattern}" >> ${rootname}.cfg
      endif
      if (! ${?via_stacks} ) then
         set via_stacks="all"
      endif
      echo "via stack ${via_stacks}" >> ${rootname}.cfg
      set vddonly = `echo $vddnet | cut -d"," -f1`
      set gndonly = `echo $gndnet | cut -d"," -f1`
      echo "vdd $vddonly" >> ${rootname}.cfg
      echo "gnd $gndonly" >> ${rootname}.cfg

   else
      echo "# qrouter configuration for project ${rootname}" > ${rootname}.cfg
      echo "" >> ${rootname}.cfg
      if ( "$techleffile" != "" ) then
         echo "lef ${techlefpath}" >> ${rootname}.cfg
      endif
      echo "lef ${lefpath}" >> ${rootname}.cfg
      if ( ${?hard_macros} ) then
          foreach macro_path ( $hard_macros )
	      foreach file ( `ls ${sourcedir}/${macro_path}` )
		  if ( ${file:e} == "lef" ) then
		      echo "lef ${sourcedir}/${macro_path}/${file}" >> ${rootname}.cfg
	          endif
	      end
	  end
      endif
      echo "num_layers ${route_layers}" >> ${rootname}.cfg
      if ( ${?via_pattern} ) then
         echo "" >> ${rootname}.cfg
         echo "via pattern ${via_pattern}" >> ${rootname}.cfg
      endif
      if ( ${?via_stacks} ) then
         if (${via_stacks} == "none") then
            echo "no stack" >> ${rootname}.cfg
	 else
            if (${via_stacks} == "all") then
               echo "stack ${route_layers}" >> ${rootname}.cfg
	    else
               echo "stack ${via_stacks}" >> ${rootname}.cfg
	    endif
	 endif
      endif
   endif

   # Add obstruction fence around design, created by place2def.tcl
   # and modified by addspacers

   if ( -f ${rootname}.obs ) then
      cat ${rootname}.obs >> ${rootname}.cfg
   endif

   # Scripted version continues with the read-in of the DEF file

   if (${scripting} == "T") then
      if ("x$useantennacell" != "x") then
	 echo "catch {qrouter::antenna init ${useantennacell}}" >> ${rootname}.cfg
      endif
      echo "read_def ${rootname}.def" >> ${rootname}.cfg
   endif

   # If there is a file called ${rootname}.cfg2, then append it to the
   # ${rootname}.cfg file.  It will be used to define all routing behavior.
   # Otherwise, if using scripting, then append the appropriate routing
   # command or procedure based on whether this is a pre-congestion
   # estimate of routing or the final routing pass.

   if ( -f ${rootname}.cfg2 ) then
      cat ${rootname}.cfg2 >> ${rootname}.cfg
   else
      if (${scripting} == "T") then
	 echo "qrouter::standard_route ${rootname}_route.def false ${qrouter_nocleanup}" >> ${rootname}.cfg
	 # write_delays folded into standard_route in qrouter version 1.4.21.
	 if (${major} == 1 && ${minor} == 4 && ${subv} < 21) then
	    echo "qrouter::write_delays ${rootname}_route.rc" >> ${rootname}.cfg
	 endif
	 # Qrouter will drop into the interpreter on failure, so force a
	 # quit command to make sure that qrouter actually exits.
	 echo "quit" >> ${rootname}.cfg
      endif
   endif

   #------------------------------------------------------------------
   # Automatic optimization of buffer tree placement causes the
   # original BLIF netlist, with tentative buffer assignments, to
   # be invalid.  Use the DEF2Verilog tool to back-annotate the
   # correct assignments into the original BLIF netlist, then
   # use that BLIF netlist to regenerate the SPICE and RTL verilog
   # netlists.
   #------------------------------------------------------------------

   echo "DEF2Verilog -v ${synthdir}/${rootname}.rtlnopwr.v -o ${synthdir}/${rootname}_anno.v" \
	|& tee -a ${synthlog}
   echo "-p ${vddnet} -g ${gndnet} ${lefoptions} ${rootname}.def" |& tee -a ${synthlog}
   ${bindir}/DEF2Verilog -v ${synthdir}/${rootname}.rtlnopwr.v \
		-o ${synthdir}/${rootname}_anno.v \
		-p ${vddnet} -g ${gndnet} \
		${lefoptions} ${rootname}.def >>& ${synthlog}
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

   if ( !( -f ${synthdir}/${rootname}_anno.v )) then
      echo "DEF2Verilog failure:  No file ${rootname}_anno.v." \
		|& tee -a ${synthlog}
      echo "RTL verilog and SPICE netlists may be invalid if there" \
		|& tee -a ${synthlog}
      echo "were buffer trees optimized by placement." |& tee -a ${synthlog}
      echo "Synthesis flow continuing, condition not fatal." >> ${synthlog}
    else
      echo "" >> ${synthlog}
      echo "Generating RTL verilog and SPICE netlist file in directory" \
		|& tee -a ${synthlog}
      echo "   ${synthdir}" |& tee -a ${synthlog}
      echo "Files:" |& tee -a ${synthlog}
      echo "   Verilog: ${synthdir}/${rootname}.rtl.v" |& tee -a ${synthlog}
      echo "   Verilog: ${synthdir}/${rootname}.rtlnopwr.v" |& tee -a ${synthlog}
      echo "   Verilog: ${synthdir}/${rootname}.rtlbb.v" |& tee -a ${synthlog}
      echo "   Spice:   ${synthdir}/${rootname}.spc" |& tee -a ${synthlog}
      echo "" >> ${synthlog}

      cd ${synthdir}

      #------------------------------------------------------------------
      # Copy the original rtl.v and rtlnopwr.v for use in comparison of
      # pre- and post-placement netlists.
      #------------------------------------------------------------------

      echo "Copying ${rootname}.rtl.v, ${rootname}.rtlnopwr.v, and ${rootname}.rtlbb.v to backups"
      cp ${rootname}.rtl.v ${rootname}_synth.rtl.v
      cp ${rootname}.rtlnopwr.v ${rootname}_synth.rtlnopwr.v
      cp ${rootname}.rtlbb.v ${rootname}_synth.rtlbb.v

      echo "Running vlog2Verilog." |& tee -a ${synthlog}
      echo "vlog2Verilog -c -v ${vddnet} -g ${gndnet} -o ${rootname}.rtl.v ${rootname}_anno.v" |& tee -a ${synthlog}
      ${bindir}/vlog2Verilog -c -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtl.v ${rootname}_anno.v >>& ${synthlog}

      set errcond = $status
      if ( ${errcond} != 0 ) then
         echo "vlog2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
         echo "Premature exit." |& tee -a ${synthlog}
         echo "Synthesis flow stopped on error condition." >>& ${synthlog}
         exit 1
      endif

      echo "vlog2Verilog -c -p -v ${vddnet} -g ${gndnet} -o ${rootname}.rtlnopwr.v ${rootname}_anno.v" |& tee -a ${synthlog}
      ${bindir}/vlog2Verilog -c -p -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtlnopwr.v ${rootname}_anno.v >>& ${synthlog}

      set errcond = $status
      if ( ${errcond} != 0 ) then
         echo "vlog2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
         echo "Premature exit." |& tee -a ${synthlog}
         echo "Synthesis flow stopped on error condition." >>& ${synthlog}
         exit 1
      endif

      echo "vlog2Verilog -c -b -p -n -v ${vddnet} -g ${gndnet} -o ${rootname}.rtlbb.v ${rootname}_anno.v" |& tee -a ${synthlog}
      ${bindir}/vlog2Verilog -c -b -p -n -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtlbb.v ${rootname}_anno.v >>& ${synthlog}

      set errcond = $status
      if ( ${errcond} != 0 ) then
         echo "vlog2Verilog failed with exit status ${errcond}" |& tee -a ${synthlog}
         echo "Premature exit." |& tee -a ${synthlog}
         echo "Synthesis flow stopped on error condition." >>& ${synthlog}
         exit 1
      endif

      echo "Running vlog2Spice." |& tee -a ${synthlog}
      echo "vlog2Spice -i -l ${spicepath} -o ${rootname}.spc ${rootname}.rtl.v" \
		|& tee -a ${synthlog}
      ${bindir}/vlog2Spice -i -l ${spicepath} -o ${rootname}.spc ${rootname}.rtl.v >>& ${synthlog}

      set errcond = $status
      if ( ${errcond} != 0 ) then
         echo "vlog2Spice failed with exit status ${errcond}" |& tee -a ${synthlog}
         echo "Premature exit." |& tee -a ${synthlog}
         echo "Synthesis flow stopped on error condition." >>& ${synthlog}
         exit 1
      endif

      #------------------------------------------------------------------
      # Spot check:  Did vlog2Verilog or vlog2Spice exit with an error?
      #------------------------------------------------------------------

      if ( ! -f ${rootname}.rtl.v || ( -M ${rootname}.rtl.v \
		    < -M ${rootname}_anno.v )) then
	 echo "vlog2Verilog failure:  No file ${rootname}.rtl.v created." \
		|& tee -a ${synthlog}
      endif

      if ( ! -f ${rootname}.rtlnopwr.v || ( -M ${rootname}.rtlnopwr.v \
		    < -M ${rootname}_anno.v )) then
	 echo "vlog2Verilog failure:  No file ${rootname}.rtlnopwr.v created." \
		|& tee -a ${synthlog}
      endif

      if ( ! -f ${rootname}.rtlbb.v || ( -M ${rootname}.rtlbb.v \
		    < -M ${rootname}_anno.v )) then
	 echo "vlog2Verilog failure:  No file ${rootname}.rtlbb.v created." \
		|& tee -a ${synthlog}
      endif

      if ( ! -f ${rootname}.spc || ( -M ${rootname}.spc \
		    < -M ${rootname}_anno.v )) then
	 echo "vlog2Spice failure:  No file ${rootname}.spc created." \
		|& tee -a ${synthlog}
      endif

      # Return to the layout directory
      cd ${layoutdir}

    endif
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------

set endtime = `date`
echo "Placement script ended on $endtime" >> $synthlog

exit 0
