#
# qflow project included technology osu050 files
#

# Main compiler arguments
CFLAGS = -g -O2
DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_SETENV=1 -DHAVE_PUTENV=1 -DTCLSH_PATH=\"/bin/tclsh\" -DQFLOW_MAGIC_PATH=\"/usr/local/bin/magic\" -DQFLOW_NETGEN_PATH=\"/usr/local/bin/netgen\" -DQFLOW_QROUTER_PATH=\"/usr/local/bin/qrouter\" -DQFLOW_GRAYWOLF_PATH=\"/usr/local/bin/graywolf\" -DQFLOW_YOSYS_PATH=\"/usr/local/bin/yosys\" -DQFLOW_OPENTIMER_PATH=\"\" -DQFLOW_OPENSTA_PATH=\"/usr/local/bin/sta\"
LIBS = 
LDFLAGS = 
INSTALL = /bin/install -c

QFLOW_LIB_DIR = /usr/local/share/qflow

TECH_FILES = osu050.par osu050_stdcells.lef osu050.sh
TECH_FILES += osu050_stdcells.sp osu050.magicrc osu050.prm
TECH_FILES += SCN3ME_SUBM.30.tech osu05_stdcells.lib
TECH_FILES += osu05_stdcells.v osu050_setup.tcl osu05_stdcells.gds2

TECHINSTALL = ${QFLOW_LIB_DIR}/tech

# Substitute the target qflow tech directory name in .magicrc so that magic
# can find the OSU050 techfile

all: osu050.magicrc.in
	$(RM) -f osu050.magicrc
	cat osu050.magicrc.in | sed -e \
		'/QFLOW_LIB_DIR/s#QFLOW_LIB_DIR#$(QFLOW_LIB_DIR)#' \
		> osu050.magicrc

install: ${TECH_FILES}
	@echo "Installing osu050 tech files"
	$(INSTALL) -d $(DESTDIR)$(TECHINSTALL)/osu050
	for target in $(TECH_FILES); do \
	   $(INSTALL) $$target $(DESTDIR)$(TECHINSTALL)/osu050 ;\
	done

clean:
	$(RM) -f osu050.magicrc

distclean:
	$(RM) -f osu050.magicrc

uninstall:

