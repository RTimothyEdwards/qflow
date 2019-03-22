#
# qflow project included technology gscl45nm files
#

# Main compiler arguments
CFLAGS = -g -O2
DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_SETENV=1 -DHAVE_PUTENV=1 -DTCLSH_PATH=\"/bin/tclsh\" -DQFLOW_MAGIC_PATH=\"/usr/local/bin/magic\" -DQFLOW_NETGEN_PATH=\"/usr/local/bin/netgen\" -DQFLOW_QROUTER_PATH=\"/usr/local/bin/qrouter\" -DQFLOW_GRAYWOLF_PATH=\"/usr/local/bin/graywolf\" -DQFLOW_YOSYS_PATH=\"/usr/local/bin/yosys\" -DQFLOW_OPENTIMER_PATH=\"\" -DQFLOW_OPENSTA_PATH=\"/usr/local/bin/sta\"
LIBS = 
LDFLAGS = 
INSTALL = /bin/install -c

QFLOW_LIB_DIR = /usr/local/share/qflow

TECH_FILES = gscl45nm.par gscl45nm.lef gscl45nm.sh
TECH_FILES += gscl45nm.sp gscl45nm.magicrc gscl45nm.prm
TECH_FILES += gscl45nm.tech gscl45nm.lib
TECH_FILES += gscl45nm.v gscl45nm_setup.tcl

TECHINSTALL = ${QFLOW_LIB_DIR}/tech

# Substitute the target qflow tech directory name in .magicrc so that magic
# can find the gscl45nm techfile

all: gscl45nm.magicrc.in
	$(RM) -f gscl45nm.magicrc
	cat gscl45nm.magicrc.in | sed -e \
		'/QFLOW_LIB_DIR/s#QFLOW_LIB_DIR#$(QFLOW_LIB_DIR)#' \
		> gscl45nm.magicrc

install: ${TECH_FILES}
	@echo "Installing gscl45nm tech files"
	$(INSTALL) -d $(DESTDIR)$(TECHINSTALL)/gscl45nm
	for target in $(TECH_FILES); do \
	   $(INSTALL) $$target $(DESTDIR)$(TECHINSTALL)/gscl45nm ;\
	done

clean:
	$(RM) -f gscl45nm.magicrc

distclean:
	$(RM) -f gscl45nm.magicrc

uninstall:

