#!/bin/tcsh
#---------------------------------------------------------------
# Shell script setting up all variables used by the qflow scripts
# for this project
#---------------------------------------------------------------

# The LEF file containing standard cell macros

set leffile=gscl45nm.lef

# The SPICE netlist containing subcell definitions for all the standard cells

set spicefile=gscl45nm.sp

# The liberty format file containing standard cell timing and function information

set libertyfile=gscl45nm.lib

# If there is another LEF file containing technology information
# that is separate from the file containing standard cell macros,
# set this.  Otherwise, leave it defined as an empty string.

set techleffile=""

# All cells below should be the lowest output drive strength value,
# if the standard cell set has multiple cells with different drive
# strengths.  Comment out any cells that do not exist.

set flopcell=DFFPOSX1	;# Standard positive-clocked DFF, no set or reset
# set flopset=DFFS	;# DFF with preset, if available
# set flopreset=DFFSR	;# DFF with clear, if available
set flopsetreset=DFFSR	;# DFF with both set and clear
set setpin=S		;# The name of the set pin on DFFs
set resetpin=R		;# The name of the clear/reset pin on DFFs
set setpininvert=1	;# Set this to 1 if the set pin is inverted (!set)
set resetpininvert=1	;# Set this to 1 if the reset pin is inverted (!reset)
set floppinout=Q	;# Name of the output pin on DFFs
set floppinin=D		;# Name of the output pin on DFFs
set floppinclk=CLK	;# Name of the clock pin on DFFs

set bufcell=BUFX2	;# Minimum drive strength buffer cell
set bufpin_in=A		;# Name of input port to buffer cell
set bufpin_out=Y	;# Name of output port to buffer cell
set clkbufcell=CLKBUF1	;# Minimum drive strength clock buffer cell
set clkbufpin_in=A	;# Name of input port to clock buffer cell
set clkbufpin_out=Y	;# Name of output port to clock buffer cell
set inverter=INVX1	;# Minimum drive strength inverter cell
set invertpin_in=A	;# Name of input port to inverter cell
set invertpin_out=Y	;# Name of output port to inverter cell
set norgate=NOR2X1	;# 2-input NOR gate, minimum drive strength
set norpin_in1=A	;# Name of first input pin to NOR gate
set norpin_in2=B	;# Name of second input pin to NOR gate
set norpin_out=Y	;# Name of output pin from OR gate
set nandgate=NAND2X1	;# 2-input NAND gate, minimum drive strength
set nandpin_in1=A	;# Name of first input pin to NAND gate
set nandpin_in2=B	;# Name of second input pin to NAND gate
set nandpin_out=Y	;# Name of output pin from NAND gate
set fillcell=FILL	;# Spacer (filler) cell (may use regexp)
set decapcell=""	;# Decap (filler) cell (may use regexp)
set antennacell=""	;# Antenna (filler) cell (may use regexp)
set antennapin_in=""	;# Name of input pin to antenna cell, if it exists

set tiehi=""		;# Cell to connect to power, if one exists
set tiehipin_out=""	;# Output pin name of tiehi cell, if it exists
set tielo=""		;# Cell to connect to ground, if one exists
set tielopin_out=""	;# Output pin name of tielo cell, if it exists

set gndnet=gnd		;# Name used for ground pins in standard cells
set vddnet=vdd		;# Name used for power pins in standard cells

set separator=""		;# Separator between gate names and drive strengths
set techfile=gscl45nm		;# magic techfile
set magicrc=gscl45nm.magicrc	;# magic startup script
set gdsfile=gscl45nm.gds	;# GDS database of standard cells

set fanout_options="-l 200 -c 50"	;# blifFanout target maximum latency
					;# per gate 200ps, output load set to 50fF

set base_units="1000"		;# Use nanometer base units instead of centimicrons
set vesta_options="--summary reports --long"
set num_layers=6		;# Normally restrict routing to 6 layers
set addspacers_options="-stripe 1.7 50.0 PG"
set xspice_options="-io_time=50p -time=10p -idelay=10p -odelay=30p -cload=50f"
