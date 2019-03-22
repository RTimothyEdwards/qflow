#!/bin/tcsh -f
#----------------------------------------------------------
# Qflow layout display script using magic-8.0
#----------------------------------------------------------
# Tim Edwards, April 2013
#----------------------------------------------------------

if ($#argv < 2) then
   echo Usage:  display.sh [options] <project_path> <source_name>
   echo	Options:
   echo		-g	Use GDS view of standard cells (default auto-detect)
   echo		-l	Use LEF view of standard cells
   echo		-d	Use DEF view of layout (default auto-detect)
   echo		-m	Use magic database view of layout
   exit 1
endif

# Split out options from the main arguments
set argline=(`getopt "gldm" $argv[1-]`)
set cmdargs=`echo "$argline" | awk 'BEGIN {FS = "-- "} END {print $2}'`
set argc=`echo $cmdargs | wc -w`

if ($argc == 2) then
   set argv1=`echo $cmdargs | cut -d' ' -f1`
   set argv2=`echo $cmdargs | cut -d' ' -f2`
else
   echo Usage:  display.sh [options] <project_path> <source_name>
   echo   where
   echo       <project_path> is the name of the project directory containing
   echo                 a file called qflow_vars.sh.
   echo       <source_name> is the root name of the verilog file, and
   exit 1
endif

set dogds=-1
set domag=-1

foreach option (${argline})
   switch (${option})
      case -g:
	 set dogds=1
	 shift
	 breaksw
      case -l:
	 set dogds=0
	 shift
	 breaksw
      case -d:
	 set domag=0
	 shift
	 breaksw
      case -m:
	 set domag=1
	 shift
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

# Prepend techdir to magicrc unless magicrc begins with "/"
set abspath=`echo ${magicrc} | cut -c1`
if ( "${abspath}" == "/" ) then
   set magicrcpath=${magicrc}
else
   set magicrcpath=${techdir}/${magicrc}
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
# Copy the .magicrc file from the tech directory to the
# layout directory, if it does not have one.  This file
# automatically loads the correct technology file.
#----------------------------------------------------------

if (! -f ${layoutdir}/.magicrc ) then
   if ( -f ${magicrcpath} ) then
      cp ${magicrcpath} ${layoutdir}/.magicrc
   endif
endif

#----------------------------------------------------------
# Done with initialization
#----------------------------------------------------------

cd ${layoutdir}

#---------------------------------------------------
# Create magic layout (.mag file) using the
# technology LEF file to determine route widths
# and other parameters.
#---------------------------------------------------

set gdscmd="gds vendor true ; gds rescale false"
foreach gfile ( ${gdspath} )
set gdscmd="${gdscmd} ; gds read ${gdspath}"
end

set lefcmd=""
foreach lfile ( ${lefpath} )
set lefcmd="${lefcmd} ; lef read ${lefpath}"
end

if ($techleffile != "") then
   set techlefcmd="lef read ${techlefpath}"
else
   set techlefcmd=""
endif

# Handle additional .lef files from the hard macros list

set hardlefcmd=""
set hardgdscmd=""
if ( ${?hard_macros} ) then
    foreach macro_path ( $hard_macros )
        foreach file ( `ls ${sourcedir}/${macro_path}` )
            if ( ${file:e} == "lef" ) then
                set hardlefcmd="${hardlefcmd} ; lef read ${sourcedir}/${macro_path}/${file}"
            endif
            if ( ${file:e} == "gds" ) then
                set hardgdscmd="${hardgdscmd} ; gds read ${sourcedir}/${macro_path}/${file}"
            endif
        end
    end
endif

# Auto-detect which view to use based on log files.  If migration has not
# been done, then use the DEF view of the layout, otherwise use the (migrated)
# magic database view.  If GDS was generated then use the full GDS view of
# the standard cells;  otherwise use the LEF view of the standard cells.
# If either option has been forced by option switches, then the option switch
# overrides the auto-detection.

if ($domag == -1) then
   if (-f ${logdir}/migrate.log) then
      set domag = 1
   else
      set domag = 0
   endif
endif

if ($dogds == -1) then
   if (-f ${logdir}/gdsii.log) then
      set dogds = 1
   else
      set dogds = 0
   endif
endif

set dispfile="${layoutdir}/load_${rootname}.tcl"

# Create a script file for loading and displaying the layout

if ($domag == 1 && $dogds == 0) then
   cat > ${dispfile} << EOF
${techlefcmd}
${lefcmd}
${hardlefcmd}
load ${sourcename}
select top cell
expand
EOF
else if ($domag == 1 && $dogds == 1) then
   cat > ${dispfile} << EOF
${gdscmd}
${hardgdscmd}
${techlefcmd}
${lefcmd}
${hardlefcmd}
load ${sourcename}
select top cell
expand
EOF
else if ($domag == 0 && $dogds == 0) then
   cat > ${dispfile} << EOF
${techlefcmd}
${lefcmd}
${hardlefcmd}
def read ${sourcename}
select top cell
expand
EOF
else if ($domag == 0 && $dogds == 1) then
   cat > ${dispfile} << EOF
${gdscmd}
${hardgdscmd}
${techlefcmd}
${lefcmd}
${hardlefcmd}
def read ${sourcename}
select top cell
expand
EOF
endif

# don't die ungracefully if no X display:
if ( ! $?DISPLAY ) then
  echo "No DISPLAY var, not running graphical magic."
  exit
endif

# Run magic and query what graphics device types are
# available.  Use Cairo if available, fall back on
# X11, or else exit with a message

# Support option to hardwire the graphics interface.

set magicxr=0
set magicx11=0

if ( ! $?magic_display ) then
  ${bindir}/magic -noconsole -d <<EOF >& .magic_displays
exit
EOF

  set magicxr=`cat .magic_displays | grep XR | wc -l`
  set magicx11=`cat .magic_displays | grep X11 | wc -l`

  rm -f .magic_displays
endif

# Get the version of magic

${bindir}/magic -noconsole --version <<EOF >& .magic_version
exit
EOF

set magic_major=`cat .magic_version | cut -d. -f1`
set magic_minor=`cat .magic_version | cut -d. -f2`
set magic_rev=`cat .magic_version | cut -d. -f3`

rm -f .magic_version

# For magic versions less than 8.1.102, only the .mag file can
# be loaded from the command line.  Otherwise, run the script.

if ( ${magic_major} < 8 || ( ${magic_major} == 8 && ${magic_minor} < 1 ) || ( ${magic_major} == 8 && ${magic_minor} == 1 && ${magic_rev} < 102 ) ) then
   set dispfile = ${sourcename}
endif

# Run magic again, this time interactively.  The script
# exits when the user exits magic.

if ( $?magic_display ) then
   ${bindir}/magic -d ${magic_display} ${dispfile}
else if ( ${magicxr} >= 1 ) then
   ${bindir}/magic -d XR ${dispfile}
else if ( ${magicx11} >= 1) then
   ${bindir}/magic -d X11 ${dispfile}
else
   echo "Magic does not support Cairo or X11 graphics on this host."
endif

#------------------------------------------------------------
# Done!
#------------------------------------------------------------
