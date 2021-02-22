#!/usr/bin/env python3
#
# migrate.py
#
# This file converts a qflow project from one environment to another
# by changing paths in various files like qflow_vars.sh.
#
# Options:
#    -project_path=<path>	Path to project top level directory
#    -qflow_path=<path>		Path to qflow installation
#    -tech_path=<path>		Path to technology files

import re
import os
import sys
import glob
import stat
import shutil
import filecmp
import subprocess

# NOTE:  This version of copy_tree from distutils works like shutil.copytree()
# in Python 3.8 and up ONLY using "dirs_exist_ok=True".
from distutils.dir_util import copy_tree

def usage():
    print("")
    print("migrate.py [options...]")
    print("   -project_path=<path>  Path to project top level directory")
    print("   -qflow_path=<path>    Path to qflow installation")
    print("   -tech_path=<path>     Current path to technology files ")
    print("")
    print("project_path may be omitted if the project path is the current directory.")
    print("qflow_path may be omitted if qflow is installed in the default /usr/share/.")
    print("tech_path may be omitted if technology is embedded in the project.")

# Filter files to replace all strings matching "orig_text" with "new_text" for
# every file in "filelist".  If "filelist" is None, then apply recursively for
# all files in project_path.

def filter_files(project_path, filelist, orig_text, new_text):
    # Add any non-ASCII file types here
    bintypes = []

    if not os.path.exists(project_path):
        return
    elif os.path.islink(project_path):
        return

    if not orig_text:
        print('Error:  No original text string.  New text is ' + new_text)
        return
    if not new_text:
        print('Error:  No new text string.  Original text is ' + orig_text)
        return

    if orig_text == new_text:
        # Nothing to replace
        return

    if not filelist:
        filelist = glob.glob(project_path + '/*')

    print('Replacing ' + orig_text + ' with ' + new_text)

    for filepath in filelist:
        print('   in ' + os.path.split(filepath)[1]) 

        # Do not attempt to do text substitutions on a binary file!
        if os.path.splitext(filepath)[1] in bintypes:
            continue

        if os.path.islink(filepath):
            continue
        elif os.path.isdir(filepath):
            filter_files(filepath, None, orig_text, new_text)
        else:
            with open(filepath, 'r') as ifile:
                try:
                    flines = ifile.read().splitlines()
                except UnicodeDecodeError:
                    print('Failure to read file ' + filepath + '; non-ASCII content.')
                    continue

            with open(filepath, 'w') as ofile:
                for line in flines:
                    newline = line.replace(orig_text, new_text)
                    print(newline, file=ofile) 
        
#----------------------------------------------------------------
# This is the main entry point for the qflow migration script.
#----------------------------------------------------------------

if __name__ == '__main__':

    optionlist = []
    newopt = []

    project_orig = None
    project_path = None
    qflow_orig = None
    qflow_path = None
    tech_orig = None
    tech_path = None
    qflow_verson = None
    qflow_current = None

    # Check for command-line options

    for option in sys.argv[1:]:
        if option.find('-', 0) == 0:
            optionlist.append(option[1:])

    link_name = None
    for option in optionlist:
        optionpair = option.split('=')
        if len(optionpair) > 0:
            if optionpair[0] == 'project_path':
                project_path = optionpair[1]
            elif optionpair[0] == 'qflow_path':
                qflow_path = optionpair[1]
            elif optionpair[0] == 'tech_path':
                tech_path = optionpair[1]

    # If project_path is not specified, then it is assumed to be the
    # current directory.
    if not project_path:
        print("Assuming current working directory is the project path.")
        project_path = os.getcwd()

    # Must have a "qflow_vars.sh" in the project path (sanity check)
    if not os.path.exists(os.path.join(project_path, "qflow_vars.sh")):
        print("No qflow_vars.sh found in project path.")
        usage() 
        print("Exiting.")
        sys.exit(1)
    else:
        print("Okay.")

    # If qflow_path is not specified, then it is assumed to be the
    # default install location
    if not qflow_path:
        print("Assuming qflow installation is the default (/usr/local/share/qflow).")
        qflow_path = '/usr/local/share/qflow'

    # Must have a "qflow.sh" in the qflow path (sanity check)
    if not os.path.exists(os.path.join(qflow_path, "scripts", "qflow.sh")):
        print("No qflow.sh found in qflow path scripts subdirectory.")
        usage() 
        print("Exiting.")
        sys.exit(1)
    else:
        print("Okay.")

    # Read qflow_vars.sh and get the original paths to the project, qflow, and
    # technology.

    qflow_vars_file = os.path.join(project_path, 'qflow_vars.sh')
    print('Parsing qflow_vars.sh for original paths.')
    with open(qflow_vars_file, 'r') as ifile:
        qlines = ifile.read().splitlines()
        for line in qlines:
            tokens = line.split()
            if len(tokens) > 1 and tokens[0] == 'set':
                setpair = tokens[1].split('=')
                if len(setpair) == 2:
                    if setpair[0] == 'projectpath':
                        project_orig = setpair[1]
                    elif setpair[0] == 'scriptdir':
                        qflow_orig = os.path.split(setpair[1])[0]
                    elif setpair[0] == 'techdir':
                        tech_orig = setpair[1]
                    elif setpair[0] == 'qflowversion':
                        qflow_version = setpair[1]

    if not project_orig:
        print('Warning:  Project path not found in qflow_vars.sh')
    if not qflow_orig:
        print('Warning:  Qflow path not found in qflow_vars.sh')
    if not tech_orig:
        print('Warning:  Technology path not found in qflow_vars.sh')
    if not qflow_version:
        print('Warning:  Qflow version not found in qflow_vars.sh')

    print('Diagnostic:  After reading qflow_vars.sh,')
    print('   project_orig = ' + project_orig)
    print('   qflow_orig = ' + qflow_orig)
    print('   tech_orig = ' + tech_orig)
    print('   qflow_version = ' + qflow_version)

    # If tech_path is not set, then tech_orig must be a subdirectory of the
    # original project path so that we know how to set tech_path;  otherwise
    # raise an error.

    if not tech_path:
        print('Assuming technology is defined within the project.')
        if project_orig in tech_orig:
            tech_suffix = tech_orig[len(project_orig):]
            tech_path = project_path + tech_suffix
            print('Okay.')
        else:
            print("No path to the technology specified, and technology is not in the project.")
            usage() 
            print("Exiting.")
            sys.exit(1)

    # All paths should be absolute, not relative.
    tech_path = os.path.abspath(tech_path)
    project_path = os.path.abspath(project_path)
    qflow_path = os.path.abspath(qflow_path)

    # Get qflow version to replace "qflowversion" in qflow_vars.sh file
    qproc = subprocess.Popen(['qflow', '-v'], stdin = subprocess.DEVNULL,
	    stdout = subprocess.PIPE, stderr = subprocess.PIPE,
	    universal_newlines = True)
    qpair = qproc.communicate(timeout=1)
    for line in qpair[0].splitlines():
        tokens = line.split()
        if len(tokens) == 5:
            qflow_current = tokens[2] + '.' + tokens[4]
            print("Current qflow version is " + qflow_current)
            break
        elif len(tokens) > 0:
            print("Error:  Cannot get version number from qflow")
            usage()
            print("Exiting.")
            sys.exit(1)

    filelist = ['qflow_vars.sh', 'project_vars.sh', 'qflow_exec.sh']

    print('')
    filter_files(project_path, filelist, project_orig, project_path)
    filter_files(project_path, filelist, tech_orig, tech_path)
    filter_files(project_path, filelist, qflow_orig, qflow_path)
    filter_files(project_path, filelist[0:1], qflow_version, qflow_current)

    print('Done with qflow project migration.')
    sys.exit(0)
