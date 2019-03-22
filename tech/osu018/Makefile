#
# qflow project included technology osu018 files
#

# Main compiler arguments
CFLAGS = -g -O2
DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_SETENV=1 -DHAVE_PUTENV=1 -DTCLSH_PATH=\"/bin/tclsh\" -DQFLOW_MAGIC_PATH=\"/usr/local/bin/magic\" -DQFLOW_NETGEN_PATH=\"/usr/local/bin/netgen\" -DQFLOW_QROUTER_PATH=\"/usr/local/bin/qrouter\" -DQFLOW_GRAYWOLF_PATH=\"/usr/local/bin/graywolf\" -DQFLOW_YOSYS_PATH=\"/usr/local/bin/yosys\" -DQFLOW_OPENTIMER_PATH=\"\" -DQFLOW_OPENSTA_PATH=\"/usr/local/bin/sta\"
LIBS = 
LDFLAGS = 
INSTALL = /bin/install -c

QFLOW_LIB_DIR = /usr/local/share/qflow

TECH_FILES = osu018.par osu018_stdcells.lef osu018.sh
TECH_FILES += osu018_stdcells.sp osu018.magicrc osu018.prm
TECH_FILES += SCN6M_SUBM.10.tech osu018_stdcells.lib
TECH_FILES += osu018_stdcells.v osu018_setup.tcl osu018_stdcells.gds2

TECHINSTALL = ${QFLOW_LIB_DIR}/tech

# Substitute the target qflow tech directory name in .magicrc so that magic
# can find the OSU018 techfile

all: osu018.magicrc.in
	$(RM) -f osu018.magicrc
	cat osu018.magicrc.in | sed -e \
		'/QFLOW_LIB_DIR/s#QFLOW_LIB_DIR#$(QFLOW_LIB_DIR)#' \
		> osu018.magicrc

install: ${TECH_FILES}
	@echo "Installing osu018 tech files"
	$(INSTALL) -d $(DESTDIR)$(TECHINSTALL)/osu018
	for target in $(TECH_FILES); do \
	   $(INSTALL) $$target $(DESTDIR)$(TECHINSTALL)/osu018 ;\
	done

clean:
	$(RM) -f osu018.magicrc

distclean:
	$(RM) -f osu018.magicrc

uninstall:

