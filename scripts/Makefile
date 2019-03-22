#
# qflow project scripts makefile
#

# Main compiler arguments
CFLAGS = -g -O2
DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_SETENV=1 -DHAVE_PUTENV=1 -DTCLSH_PATH=\"/bin/tclsh\" -DQFLOW_MAGIC_PATH=\"/usr/local/bin/magic\" -DQFLOW_NETGEN_PATH=\"/usr/local/bin/netgen\" -DQFLOW_QROUTER_PATH=\"/usr/local/bin/qrouter\" -DQFLOW_GRAYWOLF_PATH=\"/usr/local/bin/graywolf\" -DQFLOW_YOSYS_PATH=\"/usr/local/bin/yosys\" -DQFLOW_OPENTIMER_PATH=\"\" -DQFLOW_OPENSTA_PATH=\"/usr/local/bin/sta\"
LIBS = 
LDFLAGS = 
INSTALL = /bin/install -c

VERSION = 1.3
REVISION = 11

QFLOW_LIB_DIR = /usr/local/share/qflow
QFLOW_BIN_DIR = /usr/local/bin
TCLSH_PATH = /bin/tclsh
ENV_PATH = /bin/env
HAVE_PYTHON3 = yes
HAVE_OPENTIMER = 0
HAVE_OPENSTA = 1

DEFAULTPARSER = @QFLOW_DEFAULT_PARSER@

TCL_SCRIPTS = blif2cel.tcl place2def.tcl place2lef2.tcl
TCL_SCRIPTS += place2net2.tcl ypostproc.tcl ybuffer.tcl
TCL_SCRIPTS += decongest.tcl addspacers.tcl getfillcell.tcl
TCL_SCRIPTS += getantennacell.tcl blifanno.tcl annotate.tcl
TCL_SCRIPTS += powerbus.tcl getpowerground.tcl arrangepins.tcl
TCL_SCRIPTS += removeblocks.tcl
PYTHON_SCRIPTS = spi2xspice.py consoletext.py pinmanager.py
PYTHON_SCRIPTS += tksimpledialog.py helpwindow.py qflow_manager.py
PYTHON_SCRIPTS += textreport.py tooltip.py count_lvs.py
HELP_TEXT = qflow_help.txt

SHELL_SCRIPTS = synthesize.sh placement.sh router.sh
SHELL_SCRIPTS += vesta.sh opentimer.sh opensta.sh
SHELL_SCRIPTS += qflow.sh checkdirs.sh
SHELL_SCRIPTS += cleanup.sh display.sh
SHELL_SCRIPTS += drc.sh lvs.sh gdsii.sh migrate.sh
MAIN_SCRIPT = qflow

SCRIPTINSTALL = ${QFLOW_LIB_DIR}/scripts
TECHINSTALL = ${QFLOW_LIB_DIR}/tech
QFLOWEXECPATH = ${QFLOW_LIB_DIR}/bin
EXECINSTALL = ${QFLOW_BIN_DIR}

all:	$(MAIN_SCRIPT).in qflow.sh
	$(MAKE) launcher

launcher:  $(MAIN_SCRIPT).in
	sed -e '/QFLOW_SCRIPT_DIR/s#QFLOW_SCRIPT_DIR#$(SCRIPTINSTALL)#' \
	$(MAIN_SCRIPT).in > $(MAIN_SCRIPT)

checkdirs.sh:  checkdirs.sh.in
	sed -e '/SUBST_TECH_DIR/s#SUBST_TECH_DIR#$(TECHINSTALL)#' \
	-e '/SUBST_SCRIPT_DIR/s#SUBST_SCRIPT_DIR#$(SCRIPTINSTALL)#' \
	-e '/SUBST_BIN_DIR/s#SUBST_BIN_DIR#$(QFLOWEXECPATH)#' \
	checkdirs.sh.in > checkdirs.sh

qflow.sh:  qflow.sh.in
	sed -e '/QFLOW_SCRIPT_DIR/s#QFLOW_SCRIPT_DIR#$(SCRIPTINSTALL)#' \
	-e '/QFLOW_DEFAULT_PARSER/s#QFLOW_DEFAULT_PARSER#$(DEFAULTPARSER)#' \
	-e '/QFLOW_REVISION/s#QFLOW_REVISION#$(REVISION)#' \
	-e '/QFLOW_VERSION/s#QFLOW_VERSION#$(VERSION)#' \
	-e '/HAVE_OPENTIMER/s#HAVE_OPENTIMER#$(HAVE_OPENTIMER)#' \
	-e '/HAVE_OPENSTA/s#HAVE_OPENSTA#$(HAVE_OPENSTA)#' \
	qflow.sh.in > qflow.sh

count_lvs.py: count_lvs.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

spi2xspice.py: spi2xspice.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

consoletext.py: consoletext.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

pinmanager.py: pinmanager.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

tksimpledialog.py: tksimpledialog.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

helpwindow.py: helpwindow.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

count_lvs.py: count_lvs.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

qflow_manager.py: qflow_manager.py.in
	sed -e '/QFLOW_SCRIPT_DIR/s#QFLOW_SCRIPT_DIR#$(SCRIPTINSTALL)#' \
	-e '/QFLOW_VERSION/s#QFLOW_VERSION#$(VERSION)#' \
	-e '/QFLOW_REVISION/s#QFLOW_REVISION#$(REVISION)#' \
	-e '/SUBST_TECH_DIR/s#SUBST_TECH_DIR#$(TECHINSTALL)#' \
	-e '/SUBST_SCRIPT_DIR/s#SUBST_SCRIPT_DIR#$(SCRIPTINSTALL)#' \
	-e '/SUBST_BIN_DIR/s#SUBST_BIN_DIR#$(QFLOWEXECPATH)#' \
	-e '/HAVE_OPENTIMER/s#HAVE_OPENTIMER#$(HAVE_OPENTIMER)#' \
	-e '/HAVE_OPENSTA/s#HAVE_OPENSTA#$(HAVE_OPENSTA)#' \
	-e '/ENV_PATH/s#ENV_PATH#$(ENV_PATH)#' $< > $@

textreport.py: textreport.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

tooltip.py: tooltip.py.in
	sed -e 's#ENV_PATH#$(ENV_PATH)#' $< > $@

blif2cel.tcl: blif2cel.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

place2def.tcl: place2def.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

place2lef2.tcl: place2lef2.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

place2net2.tcl: place2net2.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

ypostproc.tcl: ypostproc.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

ybuffer.tcl: ybuffer.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

decongest.tcl: decongest.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

powerbus.tcl: powerbus.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

addspacers.tcl: addspacers.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

removeblocks.tcl: removeblocks.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

getpowerground.tcl: getpowerground.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

getfillcell.tcl: getfillcell.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

getantennacell.tcl: getantennacell.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

blifanno.tcl: blifanno.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

annotate.tcl: annotate.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

arrangepins.tcl: arrangepins.tcl.in
	sed -e 's#TCLSH_PATH#$(TCLSH_PATH)#' $< > $@

install: $(TCL_SCRIPTS) $(PYTHON_SCRIPTS) $(SHELL_SCRIPTS) $(MAIN_SCRIPT) $(HELP_TEXT)
	@echo "Installing qflow TCL scripts"
	$(INSTALL) -d $(DESTDIR)${SCRIPTINSTALL}
	for target in $(TCL_SCRIPTS); do \
	   $(INSTALL) $$target $(DESTDIR)${SCRIPTINSTALL} ;\
	done
	@echo "Installing qflow shell scripts"
	for target in $(SHELL_SCRIPTS); do \
	   $(INSTALL) $$target $(DESTDIR)${SCRIPTINSTALL} ;\
	done
	@echo "Installing qflow python scripts"
	for target in $(PYTHON_SCRIPTS); do \
	   $(INSTALL) $$target $(DESTDIR)${SCRIPTINSTALL} ;\
	done
	@echo "Installing help text files"
	for target in $(HELP_TEXT); do \
	   $(INSTALL) $$target $(DESTDIR)${SCRIPTINSTALL} ;\
	done
	@echo "Installing qflow executable"
	$(INSTALL) -d $(DESTDIR)${EXECINSTALL}
	$(INSTALL) ${MAIN_SCRIPT} $(DESTDIR)${EXECINSTALL}

clean:
	$(RM) $(MAIN_SCRIPT)
	$(RM) checkdirs.sh
	$(RM) qflow.sh
	$(RM) $(TCL_SCRIPTS)
	$(RM) $(PYTHON_SCRIPTS)

distclean:
	$(RM) $(MAIN_SCRIPT)
	$(RM) checkdirs.sh
	$(RM) qflow.sh
	$(RM) $(TCL_SCRIPTS)
	$(RM) $(PYTHON_SCRIPTS)

uninstall:
	$(RM) -rf ${SCRIPTINSTALL}
	$(RM) ${EXECINSTALL}/${MAIN_SCRIPT}
