#!ENV_PATH python3
#
# spi2xspice.py
#
# Simple script to convert a standard-cell SPICE subcircuit (e.g., one created
# by qflow) into a version replacing all the standard cells with XSPICE
# primitives.  This works in conjunction with lib2xspice.py, which converts
# a liberty file for a standard cell set into a set of XSPICE models using
# d_lut, d_genlut, d_dff, and d_dlatch.
#
# Written by Tim Edwards
# efabless, inc. 2017
# May 17, 2017
# Updated Dec. 22, 2018 to accomodate in-lined standard cell subcircuits
# (i.e., not from a .include statement) and corrected duplicate .end statement.
# Updated Jan. 12, 2024 to fix port direction when inputs and outputs are not
# buffered, and a subcircuit output signal is also fed back to other gates in
# the subcircuit.
#
# This script is in the public domain
#--------------------------------------------------------------------------

import re
import sys
import os

def read_spice_lib(filein, celldefs, debug):
    # Read a spice library of standard cell definitions

    spicedefs = {}

    with open(filein, 'r') as ifile:
        spitext = ifile.read()

    increx = re.compile('^[^\*]*[ \t]*.include[ \t]+([^ \t]+).*$', re.IGNORECASE)
    subrex  = re.compile('^[^\*]*[ \t]*.subckt[ \t]+([^ \t]+)(.*)$', re.IGNORECASE)
    endsrex = re.compile('^[^\*]*[ \t]*.ends.*$', re.IGNORECASE)
    comrex  = re.compile('^[\*]+.*$', re.IGNORECASE)

    # Merge continuation lines in input
    spilines = spitext.replace('\n+', ' ').splitlines()
    if debug:
        print("Reading spice file, " + str(len(spilines)) + " lines.")

    insub = False
    for line in spilines:

        # All comment lines are ignored
        cmatch = comrex.match(line)
        if cmatch:
            continue

        imatch = increx.match(line)
        if imatch:
            # Check if filename needs same prefix as current file
            if not os.path.exists(imatch.group(1)):
                prefix = os.path.split(filein)[0]
                incfile = prefix + '/' + imatch.group(1)
            else:
                incfile = imatch.group(1)
            read_spice_lib(incfile, celldefs, debug)
            continue

        ematch = endsrex.match(line)
        if ematch:
            if insub:
                insub = False
            else:
                print("Error: .ends outside of a subcircuit")
            continue

        if not insub:
            lmatch = subrex.match(line)
            if lmatch:
                subname = lmatch.group(1)
                if subname in celldefs:
                    if debug:
                        print("Found subcircuit " + subname)
                    # Read information on spice port order into celldefs record
                    # Because line extensions have been handled, all information
                    # is on the first line.
                    cellrec = celldefs[subname]
                    cellrec['spicepins'] = lmatch.group(2).split()
                    if debug:
                        print('spicepins is ' + str(cellrec['spicepins']))

                # Read to the ".ends" statement.
                insub = True

def write_models(cellsused, celldefs, ofile, timing):
    # Write the .model statement for all cells used

    io_time = timing[0]
    time = timing[1]
    idelay = timing[2]
    odelay = timing[3]
    cload = timing[4]

    imprex = re.compile('([10\)])[ \t]+([10\(])')
    primerex = re.compile('([10\)])[ \t]*\'')
    for cellname in cellsused:
        cellrec = celldefs[cellname]
        if len(cellrec['function']) > 0:
            print('* ' + cellname + ' ' + cellrec['function'][0], file=ofile)
        else:
            print('* ' + cellname + ' (no function)', file=ofile)
        if cellrec['type'] == 'flop':
            continue
        elif cellrec['type'] == 'latch':
            continue
        elif cellrec['nin'] == 0:
            continue
        else:
            tabstr = ''
            nout = cellrec['nout']
            nin = cellrec['nin']
            for k in range(0, nout):
                # Handle tristate functions
                # If triidx < nin then triidx, tripin, and tripos are all valid
                if 'tristate' in cellrec:
                    trirec = cellrec['tristate']
                    if trirec[0] == '~':
                        tripos = False
                        tripin = trirec[1:]
                    else:
                        tripos = True
                        tripin = trirec
                    for triidx in range(0, nin):
                        if cellrec['inputs'][triidx] == tripin:
                           break
                    # Index into binstring is the reverse of the pin order
                    triidx = nin - triidx - 1
                else:
                    triidx = nin

                pstring = parse_pin(cellrec['function'][k])
                nvals = 2**nin
                for i in range(0, nvals):
                    # binstring is the binary count of input bits.  Note that
                    # the index into binstring is reverse of the pin order.
                    # (e.g., "000", "001", "010", "011", ...)
                    binstring = '{0:{fill}{width}b}'.format(i, fill='0', width=nin)

                    psubs = pstring
                    for j in range(0, nin):
                        bchar = cellrec['inputs'][nin - j - 1].strip('"')
                        bval = binstring[j]
                        psubs = psubs.replace(bchar, bval)

                    # Handle the awkward syntax where, e.g., (A B) means (A & B)
                    # Must be done in a loop because re.sub does not work on
                    # overlapping matches.

                    while True:
                        psubbed = imprex.sub('\g<1>&\g<2>', psubs)
                        if psubbed == psubs:
                            break
                        psubs = psubbed

                    try:
                        # Note: parse_pin may strip parentheses so add double
                        # parentheses to make sure the final &1 applies to
                        # the whole expression.
                        tval = eval('((' + psubs + '))&1')
                    except (SyntaxError, NameError):
                        tabstr = ''
                        print("Could not evaluate function " + cellrec['function'][k])
                        print("(Evaluated as " + psubs + ")")
                        break

                    if tval:
                        bitstr = '1'
                    else:
                        bitstr = '0'

                    # For tristate gates, the function will not include the
                    # tristate condition, so determine if tristate is active
                    # for this set of inputs and set the output to 'Z' if so.
                    if triidx < nin:
                        if binstring[triidx] == '1' and tripos == True:
                            bitstr = 'Z'
                        elif binstring[triidx] == '0' and tripos == False:
                            bitstr = 'Z'

                    tabstr += bitstr

            if tabstr != '':
                if nout == 1:
                    # General n-input LUT model
                    print('.model d_lut_' + cellname + ' d_lut (rise_delay=' + time + ' fall_delay=' + time + ' input_load=' + cload, file=ofile)
                    print('+ table_values "' + tabstr + '")', file=ofile)
                    if debug:
                        print('Cell ' + cellname + ' has table ' + tabstr)
                else:
                    # Even more general n-input, m-output LUT model
                    idelstr = ((idelay + ' ') * nin).strip() 
                    ilodstr = ((cload + ' ') * nin).strip() 
                    odelstr = ((odelay + ' ') * nout).strip() 
                            
                    print('.model d_genlut_' + cellname + ' d_genlut (', file=ofile)
                    print('+ rise_delay=[' + odelstr + ']', file=ofile)
                    print('+ fall_delay=[' + odelstr + ']', file=ofile)
                    print('+ input_load=[' + ilodstr + ']', file=ofile)
                    print('+ input_delay=[' + idelstr + ']', file=ofile)
                    print('+ table_values "' + tabstr + '")', file=ofile)
            else:
                print('No output for ' + cellname)

def read_spice(filein, fileout, celldefs, debug, modelfile, timing):
    global vdd

    vddname = '{:g}'.format(vdd).replace('.', 'v')
    if not 'v' in vddname:
        vddname += 'v'

    vddthigh = str(2 * vdd / 3)
    vddtlow = str(vdd / 3)
    vddhigh = str(vdd)

    # Read a top-level SPICE file with a digital standard cell subcircuit, and
    # write it back out as a subcircuit of XSPICE models and calls with
    # appropriate A-to-D and D-to-A bridges.

    # If 'modelfile' is given, then do not write out model lines
    # for the digital cells, but just include the modelfile at top.

    with open(filein, 'r') as ifile:
        spitext = ifile.read()

    increx = re.compile('^[^\*]*[ \t]*.include[ \t]+([^ \t]+).*$', re.IGNORECASE)
    subrex = re.compile('^[^\*]*[ \t]*.subckt[ \t]+([^ \t]+)(.*)$', re.IGNORECASE)
    xrex   = re.compile('^[ \t]*X([^ \t]+)(.*)$', re.IGNORECASE)
    comrex = re.compile('^[\*]+.*$', re.IGNORECASE)
    specrex = re.compile('^[\*]+This file may contain array delimiters', re.IGNORECASE)
    endsrex = re.compile('^[^\*]*[ \t]*.ends.*$', re.IGNORECASE)
    endrex = re.compile('^[^\*]*[ \t]*.end.*$', re.IGNORECASE)

    # Merge continuation lines in input
    spilines = spitext.replace('\n+', ' ').splitlines()
    if debug:
        print("Reading spice file, " + str(len(spilines)) + " lines.")

    # Get expected name of subcircuit from filename
    fileroot = os.path.split(filein)[1]
    subname = os.path.splitext(fileroot)[0]

    cellsused = []
    pindefs = {}

    # Make first pass to determine if there is more than one subcircuit in
    # the main file.
    subcount = 0
    subfound = False
    for line in spilines:
        cmatch = comrex.match(line)
        if cmatch:
            continue
        lmatch = subrex.match(line)
        if lmatch:
            subcount += 1
            if lmatch.group(1) == subname:
                subfound = True

    print("Writing xspice file")
    with open(fileout, 'w') as ofile:
        print("* XSpice netlist created from SPICE and liberty sources by spi2xspice.py", file=ofile)
        echoout = False
        lineno = 0
        skipsub = False
        for line in spilines:
            lineno += 1

            # Substitute for all characters "[" and "]", to avoid nesting brackets.
            line = line.replace('[', '_').replace(']', '_')

            # All comment lines go to the output, except catch the line generated
            # by qflow scripts that says "This file may contain array delimiters",
            # since we just scrubbed them, above.

            cmatch = comrex.match(line)
            if cmatch:
                smatch = specrex.match(line)
                if not smatch:
                    print(line, file=ofile)
                continue

            # If there is an include line, parse it and read the
            # library cells from that include file.
            imatch = increx.match(line) if not skipsub else []
            if imatch:
                read_spice_lib(imatch.group(1), celldefs, debug) 
                continue

            xmatch = xrex.match(line) if not skipsub else []
            if xmatch:
                # Replace subcircuit call with xspice call
                instname = xmatch.group(1)
                # NOTE:  Parsing for common CDLisms like '/' before cellname and
		# parameter passing to the instance.
                conns = list(item for item in xmatch.group(2).split() if '=' not in item and item != '/')
                pins = conns[0:-1]
                cellname = conns[-1]

                if cellname not in celldefs:
                    print("Could not find cellname " + cellname + " from subcircuit line " + str(lineno))
                    continue

                if cellname not in cellsused:
                    cellsused.append(cellname)

                cellrec = celldefs[cellname]
                # Tricky!  For each pin, find the corresponding subcircuit
                # pin name.  Then look up that pin name in the cell record
                # and find if it is an input or an output.  Then assign
                # the pin direction to the connecting net.  Then compile
                # the list of input and output nets based on the XSPICE
                # model's port order.

                if not 'spicepins' in cellrec:
                    print('Cell ' + cellname + ' does not have SPICE pin order defined!')
                    if debug:
                        print('Cell record is: ' + str(cellrec))
                    continue

                if not 'type' in cellrec:
                    print('Cell ' + cellname + ' does not have a type defined!')
                    if debug:
                        print('Cell record is: ' + str(cellrec))
                    # Provide defaults for the cell so that it doesn't cause failures
                    cellrec['type'] = 'unknown'
                    cellrec['function'] = []
                    cellrec['inputs'] = []
                    cellrec['outputs'] = []
                    cellrec['nin'] = 0
                    cellrec['nout'] = 0

                # Print the record.  Special handling for flops and latches.
                if cellrec['type'] == 'flop':

                    dclk = 'NULL'
                    ddata = 'NULL'
                    dreset = 'NULL'
                    dset = 'NULL'
                    dq = 'NULL'
                    dqbar = 'NULL'

                    if 'clock' in cellrec:
                        clkpin = cellrec['clock']
                        if clkpin[0] == '~':
                            subpin = clkpin[1:]
                        else:
                            subpin = clkpin
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if clkpin[0] == '~':
                            dclk = '~' + pins[i]
                        else:
                            dclk = pins[i]

                    # Normally 'data' will resolve to a pin name.  BUT,
                    # scan flops can have complicated expressions for
                    # next_state.  These should NOT be used by synthesis,
                    # but check for them anyway, so they do not cause
                    # the program to fault.
                    if 'data' in cellrec:
                        datapin = cellrec['data']
                        if datapin[0] == '~':
                            subpin = datapin[1:]
                        else:
                            subpin = datapin
                        try:
                            i = cellrec['spicepins'].index(subpin)
                        except:
                            ddata = 'ERROR'
                            print('Pin ' + subpin + ' of subckt ' + cellname + ' cannot be parsed.', file=sys.stderr)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if datapin[0] == '~':
                            ddata = '~' + pins[i]
                        else:
                            ddata = pins[i]

                    if 'set' in cellrec:
                        setpin = cellrec['set']
                        if setpin[0] == '~':
                            subpin = setpin[1:]
                        else:
                            subpin = setpin
                        if debug:
                            print("flop setpin = " + setpin + " subpin = " + subpin)
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if setpin[0] == '~':
                            dset = '~' + pins[i]
                        else:
                            dset = pins[i]

                    if 'reset' in cellrec:
                        resetpin = cellrec['reset']
                        if resetpin[0] == '~':
                            subpin = resetpin[1:]
                        else:
                            subpin = resetpin
                        if debug:
                            print("flop resetpin = " + resetpin + " subpin = " + subpin)
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if resetpin[0] == '~':
                            dreset = '~' + pins[i]
                        else:
                            dreset = pins[i]

                    for subpin in cellrec['outputs']:
                        subidx = cellrec['outputs'].index(subpin)
                        function = cellrec['function'][subidx]
                        if subpin in cellrec['spicepins']:
                            i = cellrec['spicepins'].index(subpin)
                            if pins[i] in pindefs:
                                pindefs[pins[i]] = 'output'
                            if 'funcneg' in cellrec:
                                if cellrec['funcneg'] == function:
                                    dqbar = pins[i]
                            if 'funcpos' in cellrec:
                                if cellrec['funcpos'] == function:
                                    dq = pins[i]
                        elif debug:
                            print('Pin ' + subpin + ' not in pin list of ' + instname)

                    print('A' + instname + ' ' + ddata + ' ' + dclk + ' ' + dset + ' ' + dreset + ' ' + dq + ' ' + dqbar + ' ddflop', file=ofile)

                elif cellrec['type'] == 'latch':

                    dena   = 'NULL'
                    ddata  = 'NULL'
                    dreset = 'NULL'
                    dset   = 'NULL'
                    dq     = 'NULL'
                    dqbar  = 'NULL'

                    if 'enable' in cellrec:
                        enapin = cellrec['enable']
                        if enapin[0] == '~':
                            subpin = enapin[1:]
                        else:
                            subpin = enapin
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if enapin[0] == '~':
                            dena = '~' + pins[i]
                        else:
                            dena = pins[i]

                    if 'data' in cellrec:
                        datapin = cellrec['data']
                        if datapin[0] == '~':
                            subpin = datapin[1:]
                        else:
                            subpin = datapin
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if datapin[0] == '~':
                            ddata = '~' + pins[i]
                        else:
                            ddata = pins[i]

                    if 'set' in cellrec:
                        setpin = cellrec['set']
                        if setpin[0] == '~':
                            subpin = setpin[1:]
                        else:
                            subpin = setpin
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if setpin[0] == '~':
                            dset = '~' + pins[i]
                        else:
                            dset = pins[i]

                    if 'reset' in cellrec:
                        resetpin = cellrec['reset']
                        if resetpin[0] == '~':
                            subpin = resetpin[1:]
                        else:
                            subpin = resetpin
                        i = cellrec['spicepins'].index(subpin)
                        if pins[i] in pindefs:
                            pindefs[pins[i]] = 'input'
                        if resetpin[0] == '~':
                            dreset = '~' + pins[i]
                        else:
                            dreset = pins[i]

                    for subpin in cellrec['outputs']:
                        subidx = cellrec['outputs'].index(subpin)
                        function = cellrec['function'][subidx]
                        if subpin in cellrec['spicepins']:
                            i = cellrec['spicepins'].index(subpin)
                            if pins[i] in pindefs:
                                pindefs[pins[i]] = 'output'
                            if 'funcneg' in cellrec:
                                if cellrec['funcneg'] == function:
                                    dqbar = pins[i]
                            if 'funcpos' in cellrec:
                                if cellrec['funcpos'] == function:
                                    dq = pins[i]

                    print('A' + instname + ' ' + ddata + ' ' + dena + ' ' + dset + ' ' + dreset + ' ' + dq + ' ' + dqbar + ' dlatch', file=ofile)

                else:
                    inlist = []
                    outlist = []
                    if 'inputs' in cellrec:
                        for subpin in cellrec['inputs']:
                            subpinname = subpin.strip('"')
                            if subpinname in cellrec['spicepins']:
                                i = cellrec['spicepins'].index(subpinname)
                                # "pins[i]" is the name of the connecting net
                                # on the top level
                                if len(pins) > i:
                                    inlist.append(pins[i])
                                    if pins[i] in pindefs:
                                        if pindefs[pins[i]] != 'output':
                                            pindefs[pins[i]] = 'input'
                                else:
                                    print('Pin ' + subpinname + ' of subckt ' + cellname + ' does not have a connecting net', file=sys.stderr)

                    if 'outputs' in cellrec:
                        for subpin in cellrec['outputs']:
                            subpinname = subpin.strip('"')
                            if subpinname in cellrec['spicepins']:
                                i = cellrec['spicepins'].index(subpinname)
                                # "pins[i]" is the name of the connecting net
                                # on the top level
                                if len(pins) > i:
                                    outlist.append(pins[i])
                                    if pins[i] in pindefs:
                                        pindefs[pins[i]] = 'output'
                                else:
                                    print('Pin ' + subpinname + ' of subckt ' + cellname + ' does not have a connecting net', file=sys.stderr)

                    intext = ' '.join(inlist)
                    outtext = ' '.join(outlist)

                    if debug:
                        print("Instance = " + instname + "; inputs = " + intext + "; outputs = " + outtext)

                    # Note:  Cells with output but no input are tie-high or tie-low,
                    # and do not map to LUTs.  Cells with no output are non-functional
                    # (e.g., antenna tie-downs).
                    if len(inlist) == 0 and len(outlist) == 1:
                        if cellrec['function'][0] == '1':
                            print('A' + instname + ' ' + outtext + ' done', file=ofile)
                        elif cellrec['function'][0] == '0':
                            print('A' + instname + ' ' + outtext + ' dzero', file=ofile)
                        else:
                            print("Cell " + cellname + " has no inputs and no constant function.")
                    elif len(inlist) == 0 and len(outlist) != 0: #FIXME
                    #Multiple outputs with no inputs, it is a cell that is used to tie high or tie low
                        for idx,fcn in enumerate(cellrec['function']):
                            if fcn == '1':
                                print('A' + instname + str(idx) + ' ' + outlist[idx] + ' done', file=ofile)
                            elif fcn == '0':
                                print('A' + instname + str(idx) + ' ' + outlist[idx] + ' dzero', file=ofile)

                    elif len(outlist) == 1:
                        print('A' + instname + ' [' + intext + '] ' + outtext + ' d_lut_' + cellname, file=ofile)
                    elif len(outlist) != 0:
                        print('A' + instname + ' [' + intext + '] [' + outtext + '] d_genlut_' + cellname, file=ofile)

                continue

            lmatch = subrex.match(line)

            if lmatch:
                subcktname = lmatch.group(1)
                if subcktname != subname and subcount != 1:
                    # Get pin names and pin order in the subcircuit                
                    if subcktname in celldefs:
                        cellrec = celldefs[subcktname]
                    else:
                        cellrec = {}
                        celldefs[subcktname] = cellrec 
                    cellrec['spicepins'] = lmatch.group(2).split()
                    if debug:
                        print('subckt ' + subcktname + ' pins = ' + str(cellrec['spicepins']))
                    skipsub = True

                else:
                    subname = subcktname
                    echoout = False
                    if debug:
                        print("Found subcircuit " + subname)
                    # Prepend all signal names with "a_"
                    pins = lmatch.group(2).split()
                    pindefs = {}
                    # Use pindefs dict to track which pins are input and output
                    for pin in pins:
                        pindefs[pin] = 'unknown'

                    # Write modified .subckt line
                    if debug:
                        print("Rewriting subcircuit " + subname)
                    print(".subckt " + subname, end='', file=ofile)
                    for pin in pins:
                        print(' a_' + pin, end='', file=ofile)
                    print('', file=ofile)

            ematch = endsrex.match(line)
            if ematch and skipsub:
                skipsub = False
            elif ematch:
                if not echoout:
                    # Write A-to-D and D-to-A bridge models
                    print("", file=ofile)
                    if modelfile != '':
                        print(".include " + modelfile, file=ofile)
                    else:
                        print(".model todig_" + vddname + " adc_bridge(in_high=" + vddthigh + " in_low=" + vddtlow + " rise_delay=" + io_time + " fall_delay=" + io_time + ")", file=ofile)
                        print(".model toana_" + vddname + " dac_bridge(out_high=" + vddhigh + " out_low=0)", file=ofile)
                        print("", file=ofile)

                        # Write d_dff, d_dlatch, d_pullup, and d_pulldown models
                        print(".model ddflop d_dff(ic=0 rise_delay=" + time + " fall_delay=" + time + ")", file=ofile)
                        print(".model dlatch d_dlatch(ic=0 rise_delay=" + time + " fall_delay=" + time + ")", file=ofile)
                        print(".model dzero d_pulldown(load=" + cload + ")", file=ofile)
                        print(".model done d_pullup(load=" + cload + ")", file=ofile)
                        print("", file=ofile)

                    # Write A-to-D and D-to-A bridges
                    inum = 0
                    onum = 0
                    for pinname in pindefs:
                        if pindefs[pinname] == 'output':
                            onum += 1
                            print("AD2A" + str(onum) + " [" + pinname + "] [a_" + pinname + "] toana_" + vddname, file=ofile)
                        # (Previously 'input': this makes sure that even unused
                        # power and ground nets are not left floating.)
                        else:
                            inum += 1
                            print("AA2D" + str(inum) + " [a_" + pinname + "] [" + pinname + "] todig_" + vddname, file=ofile)
                    print("", file=ofile)

                echoout =  True

            if not ematch:
                endline = endrex.match(line)
            else:
                endline = None

            if echoout and not endline:
                print(line, file=ofile)

        # At the end, write all of the LUT-based digital models.
        if modelfile == '':
            print("", file=ofile)
            write_models(cellsused, celldefs, ofile, timing)
  
        print(".end", file=ofile)

def write_lib(fileout, celldefs, debug, timing):
    global vdd

    vddname = '{:g}'.format(vdd).replace('.', 'v')
    if not 'v' in vddname:
        vddname += 'v'

    vddthigh = str(2 * vdd / 3)
    vddtlow = str(vdd / 3)
    vddhigh = str(vdd)

    # 'timing' is a list of five values:  I/O rise/falltime, cell rise/
    # falltime, cell input delay, cell output delay, and cell load cap.
    # These are all rough averages for the process.

    io_time = timing[0]
    time = timing[1]
    idelay = timing[2]
    odelay = timing[3]
    cload = timing[4]

    # Write a library file containing the models of all of the cells in the
    # liberty file, which can then be included in other xspice simulatable
    # netlists.

    print("Writing xspice model library")
    with open(fileout, 'w') as ofile:
        print("* XSpice library created from liberty sources by spi2xspice.py", file=ofile)
        echoout = False

        print("", file=ofile)
        print(".model todig_" + vddname + " adc_bridge(in_high=" + vddthigh + " in_low=" + vddtlow + " rise_delay=" + io_time + " fall_delay=" + io_time + ")", file=ofile)
        print(".model toana_" + vddname + " dac_bridge(out_high=" + vddhigh + " out_low=0)", file=ofile)
        print("", file=ofile)

        # Write d_dff, d_dlatch, d_pullup, and d_pulldown models
        print(".model ddflop d_dff(ic=0 rise_delay=" + time + " fall_delay=" + time + ")", file=ofile)
        print(".model dzero d_pulldown(load=" + cload + ")", file=ofile)
        print(".model done d_pullup(load=" + cload + ")", file=ofile)

        cellsused = []
        for cell in celldefs:
            cellsused.append(cell)

        # At the end, write all of the LUT-based digital models.
        print("", file=ofile)
        write_models(cellsused, celldefs, ofile, timing)
        print(".end", file=ofile)

def parse_pin(function):
    # Handle n' as way of expressing ~n or !n
    primerex = re.compile('([^ \t]+)[ \t]*\'')
    outparenrex = re.compile('^[ \t]*\([ \t]*(.+)[ \t]*\)[ \t]*$')
    parenrex = re.compile('\([ \t]*([^ \t\)|&~^]+)[ \t]*\)')
    pstring = function.strip().strip('"').strip()
    pstring = pstring.replace('*', '&').replace('+', '|').replace('!', '~')
    pstring = outparenrex.sub('\g<1>', pstring)
    pstring = parenrex.sub('\g<1>', pstring)
    pstring = primerex.sub('~\g<1>', pstring)
    return pstring

def read_liberty(filein, debug):
    global vdd

    celldefs = {}
    voltrex  = re.compile('[ \t]*nom_voltage[ \t]*:[ \t]*([^;]+);')
    cellrex  = re.compile('[ \t]*cell[ \t]*\(([^)]+)\)')
    pinrex   = re.compile('[ \t]*pin[ \t]*\(([^)]+)\)')
    busrex   = re.compile('[ \t]*bus[ \t]*\(([^)]+)\)')
    lat1rex  = re.compile('[ \t]*latch[ \t]*\(([^)]+)\)')
    lat2rex  = re.compile('[ \t]*latch[ \t]*\(([^, \t]+)[ \t]*,[ \t]*([^),]+)\)')
    ff1rex   = re.compile('[ \t]*ff[ \t]*\(([^)]+)\)')
    ff2rex   = re.compile('[ \t]*ff[ \t]*\(([^, \t]+)[ \t]*,[ \t]*([^),]+)\)')
    staterex = re.compile('[ \t]*next_state[ \t]*:[ \t]*([^;]+);')
    clockrex = re.compile('[ \t]*clocked_on[ \t]*:[ \t]*([^;]+);')
    setrex   = re.compile('[ \t]*preset[ \t]*:[ \t]*([^;]+);')
    resetrex = re.compile('[ \t]*clear[ \t]*:[ \t]*([^;]+);')
    datarex  = re.compile('[ \t]*data_in[ \t]*:[ \t]*([^;]+);')
    enarex   = re.compile('[ \t]*enable[ \t]*:[ \t]*([^;]+);')
    trirex   = re.compile('[ \t]*three_state[ \t]*:[ \t]*([^;]+);')
    funcrex  = re.compile('[ \t]*function[ \t]*:[ \t]*\"?[ \t]*([^"]+)[ \t]*\"?')
    with open(filein, 'r') as ifile:
        lines = ifile.readlines()
        if debug:
            print("Reading liberty file, " + str(len(lines)) + " lines.")
        for line in lines:
            vmatch = voltrex.match(line)
            if vmatch:
                vdd = float(vmatch.group(1))
                if debug:
                   print("Nominal process voltage is " + str(vdd))
                continue

            lmatch = cellrex.match(line)
            if lmatch:
                cellname = lmatch.group(1).strip('"')
                if debug:
                    print("Found cell " + cellname)
                cellrec = {}
                cellrec['inputs'] = []
                cellrec['outputs'] = []
                cellrec['nin'] = 0
                cellrec['nout'] = 0
                cellrec['function'] = []
                # NOTE:  average rise and fall times need to be
                # averaged from the data, to get a general relation
                # between timing and drive strength.
                cellrec['rise'] = 1.0
                cellrec['fall'] = 1.0
                cellrec['type'] = 'comb'
                celldefs[cellname] = cellrec
                continue
                
            pmatch = pinrex.match(line)
            if pmatch:
                pinname = pmatch.group(1).strip('"')
                if debug:
                    print("Found input pin " + pinname)
                cellrec['inputs'].append(pinname)
                cellrec['nin'] += 1
                continue

            bmatch = busrex.match(line)
            if bmatch:
                pinname = bmatch.group(1).strip('"')
                if debug:
                    print("Found input bus " + pinname)
                cellrec['inputs'].append(pinname)
                cellrec['nin'] += 1
                continue

            lmatch = lat2rex.match(line)
            if lmatch:
                if debug:
                    print("Found latch");
                cellrec['type'] = 'latch'
                cellrec['funcpos'] = lmatch.group(1).strip('"')
                cellrec['funcneg'] = lmatch.group(2).strip('"')
                continue

            lmatch = lat2rex.match(line)
            if lmatch:
                if debug:
                    print("Found latch");
                cellrec['type'] = 'latch'
                cellrec['funcpos'] = lmatch.group(1).strip('"')
                continue

            rmatch = ff2rex.match(line)
            if rmatch:
                if debug:
                    print("Found flop");
                cellrec['type'] = 'flop'
                cellrec['funcpos'] = rmatch.group(1).strip('"')
                cellrec['funcneg'] = rmatch.group(2).strip('"')
                continue
            else:
                rmatch = ff1rex.match(line)
                if rmatch:
                    if debug:
                        print("Found flop");
                    cellrec['type'] = 'flop'
                    cellrec['funcpos'] = rmatch.group(1).strip('"')
                    continue

            fmatch = funcrex.match(line)
            if fmatch:
                function = fmatch.group(1)
                if debug:
                    print("Found function " + function + " and output pin " + pinname)
                # If pin has a function, it's an output, not an input,
                # so add it to the outputs list and remove it from the
                # inputs list.
                cellrec['outputs'].append(pinname)
                cellrec['nout'] += 1
                cellrec['inputs'].remove(pinname)
                cellrec['nin'] -= 1
                cellrec['function'].append(function)
                continue

            smatch = staterex.match(line)
            if smatch:
                if debug:
                    print('Found data input')
                cellrec['data'] = parse_pin(smatch.group(1))
                continue

            cmatch = clockrex.match(line)
            if cmatch:
                if debug:
                    print('Found clock input')
                cellrec['clock'] = parse_pin(cmatch.group(1))
                continue

            smatch = setrex.match(line)
            if smatch:
                if debug:
                    print('Found set input ' + smatch.group(1))
                cellrec['set'] = parse_pin(smatch.group(1))
                continue

            rmatch = resetrex.match(line)
            if rmatch:
                if debug:
                    print('Found reset input ' + rmatch.group(1))
                cellrec['reset'] = parse_pin(rmatch.group(1))
                continue

            dmatch = datarex.match(line)
            if dmatch:
                if debug:
                    print('Found data input')
                cellrec['data'] = parse_pin(dmatch.group(1))
                continue

            ematch = enarex.match(line)
            if ematch:
                if debug:
                    print('Found enable input')
                cellrec['enable'] = parse_pin(ematch.group(1))
                continue

            tmatch = trirex.match(line)
            if tmatch:
                if debug:
                    print('Found tristate output')
                cellrec['tristate'] = parse_pin(tmatch.group(1))
                continue

    return celldefs


if __name__ == '__main__':

    options = []
    arguments = []
    for item in sys.argv[1:]:
        if item.find('-', 0) == 0:
            options.append(item)
        else:
            arguments.append(item)

    if '-debug' in options:
        debug = True
    else:
        debug = False

    # Timing options:  Pass timing and load values as the following:
    #
    #   -io_time=<value>  Rise and fall time for signals in and out of the digital block
    #   -time=<value>     Rise and fall time of gate outputs
    #   -idelay=<value>   Input delay at gate inputs
    #   -odelay=<value>   Throughput delay of the gate
    #   -cload=<value>    Gate output load capacitance
    #
    # Note that these are just average numbers for the process.  If not
    # specified, the defaults are as assigned below.

    io_time = '10n'
    time = '1n'
    idelay = '1n'
    odelay = '50n'
    cload = '1p'

    try:
        iotimeopt = next(item for item in options if item.startswith('-io_time='))
    except:
        pass
    else:
        io_time = iotimeopt.split('=')[1]

    try:
        timeopt = next(item for item in options if item.startswith('-time='))
    except:
        pass
    else:
        time = timeopt.split('=')[1]

    try:
        idelayopt = next(item for item in options if item.startswith('-idelay='))
    except:
        pass
    else:
        idelay = idelayopt.split('=')[1]

    try:
        odelayopt = next(item for item in options if item.startswith('-odelay='))
    except:
        pass
    else:
        odelay = odelayopt.split('=')[1]

    try:
        cloadopt = next(item for item in options if item.startswith('-cload='))
    except:
        pass
    else:
        cload = cloadopt.split('=')[1]

    timing = [io_time, time, idelay, odelay, cload]

    vdd = 3.0
    celldefs = {}
    if len(arguments) >= 3:
        print("Reading liberty netlist " + arguments[0])
        print("Reading spice netlist " + arguments[1])
        print("Writing xspice netlist " + arguments[2])
        for libfile in arguments[0].split():
            celldefs.update(read_liberty(libfile, debug))
        if len(arguments) >= 4:
            modelfile = arguments[3]
        else:
            modelfile = ''
        read_spice(arguments[1], arguments[2], celldefs, debug, modelfile, timing)
        print("Done.")
    elif len(arguments) == 2:
        # Library-only option
        print("Reading liberty netlist " + arguments[0])
        print("Writing xspice model library " + arguments[1])
        for libfile in arguments[0].split():
            celldefs.update(read_liberty(libfile, debug))
        write_lib(arguments[1], celldefs, debug, timing)
        print("Done.")
    else:
        print("Usage:")
        print("spi2xspice.py <liberty file> <input spice> <output spice> [<xspice lib>]")
        print("spi2xspice.py <liberty file> <output xspice lib>")

