#
# qflow project included technology osu035 files
#

# Main compiler arguments
CFLAGS = -g -O2
DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_SETENV=1 -DHAVE_PUTENV=1 -DTCLSH_PATH=\"/bin/tclsh\" -DQFLOW_MAGIC_PATH=\"/usr/local/bin/magic\" -DQFLOW_NETGEN_PATH=\"/usr/local/bin/netgen\" -DQFLOW_QROUTER_PATH=\"/usr/local/bin/qrouter\" -DQFLOW_GRAYWOLF_PATH=\"/usr/local/bin/graywolf\" -DQFLOW_YOSYS_PATH=\"/usr/local/bin/yosys\" -DQFLOW_OPENTIMER_PATH=\"\" -DQFLOW_OPENSTA_PATH=\"/usr/local/bin/sta\"
LIBS = 
LDFLAGS = 
INSTALL = /bin/install -c

QFLOW_LIB_DIR = /usr/local/share/qflow

TECH_FILES = osu035.par osu035_stdcells.lef osu035.sh
TECH_FILES += osu035_stdcells.sp osu035.magicrc osu035.prm
TECH_FILES += SCN4M_SUBM.20.tech osu035_stdcells.lib
TECH_FILES += osu035_stdcells.v osu035_setup.tcl osu035_stdcells.gds2

TECHINSTALL = ${QFLOW_LIB_DIR}/tech

# Substitute the target qflow tech directory name in .magicrc so that magic
# can find the OSU035 techfile

all: osu035.magicrc.in
	$(RM) -f osu035.magicrc
	cat osu035.magicrc.in | sed -e \
		'/QFLOW_LIB_DIR/s#QFLOW_LIB_DIR#$(QFLOW_LIB_DIR)#' \
		> osu035.magicrc

install: ${TECH_FILES}
	@echo "Installing osu035 tech files"
	$(INSTALL) -d $(DESTDIR)$(TECHINSTALL)/osu035
	for target in $(TECH_FILES); do \
	   $(INSTALL) $$target $(DESTDIR)$(TECHINSTALL)/osu035 ;\
	done

clean:
	$(RM) -f osu035.magicrc

distclean:
	$(RM) -f osu035.magicrc

uninstall:

