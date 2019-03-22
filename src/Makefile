#
# qflow project source compile makefile
#

# Main compiler arguments
CFLAGS = -g -O2
LIBS = 
LDFLAGS = 
INSTALL = /bin/install -c

DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_SETENV=1 -DHAVE_PUTENV=1 -DTCLSH_PATH=\"/bin/tclsh\" -DQFLOW_MAGIC_PATH=\"/usr/local/bin/magic\" -DQFLOW_NETGEN_PATH=\"/usr/local/bin/netgen\" -DQFLOW_QROUTER_PATH=\"/usr/local/bin/qrouter\" -DQFLOW_GRAYWOLF_PATH=\"/usr/local/bin/graywolf\" -DQFLOW_YOSYS_PATH=\"/usr/local/bin/yosys\" -DQFLOW_OPENTIMER_PATH=\"\" -DQFLOW_OPENSTA_PATH=\"/usr/local/bin/sta\" -DQFLOW_VERSION=\"1.3\" -DQFLOW_REVISION=\"11\"

QFLOW_LIB_DIR = /usr/local/share/qflow
QFLOW_GRAYWOLF_PATH = /usr/local/bin/graywolf
QFLOW_QROUTER_PATH = /usr/local/bin/qrouter
QFLOW_ABC_PATH = @QFLOW_ABC_PATH@
QFLOW_ODIN_PATH = @QFLOW_ODIN_PATH@
QFLOW_MAGIC_PATH = /usr/local/bin/magic
QFLOW_NETGEN_PATH = /usr/local/bin/netgen
QFLOW_YOSYS_PATH = /usr/local/bin/yosys
QFLOW_OPENTIMER_PATH = 
QFLOW_OPENSTA_PATH = /usr/local/bin/sta

HAVE_ABC = @HAVE_ABC@
HAVE_YOSYS = 1
HAVE_ODIN_II = @HAVE_ODIN_II@
HAVE_MAGIC = 1
HAVE_NETGEN = 1
HAVE_OPENTIMER = 0
HAVE_OPENSTA = 1

OBJECTS = blif2BSpice.o blif2Verilog.o blifFanout.o vesta.o spice2delay.o rc2dly.o
HASHLIB = hash.o
LIBERTYLIB = readliberty.o
SOURCES := $(patsubst %.o,%.c,$(OBJECTS))
TARGETS := $(patsubst %.o,%$(EXEEXT),$(OBJECTS))

BININSTALL = ${QFLOW_LIB_DIR}/bin

all: $(TARGETS)

blif2BSpice$(EXEEXT): blif2BSpice.o
	$(CC) $(LDFLAGS) blif2BSpice.o -o $@ $(LIBS)

blifFanout$(EXEEXT): blifFanout.o $(HASHLIB) $(LIBERTYLIB)
	$(CC) $(LDFLAGS) blifFanout.o $(HASHLIB) $(LIBERTYLIB) \
		-DQFLOW_VERSION=\"VERSION\" -DQFLOW_REVISION=\"REVISION\" \
		$(DEPENDS) -o $@ $(LIBS) -lm

blif2Verilog$(EXEEXT): blif2Verilog.o
	$(CC) $(LDFLAGS) blif2Verilog.o -o $@ $(LIBS)

vesta$(EXEEXT): vesta.o $(HASHLIB)
	$(CC) $(LDFLAGS) vesta.o $(HASHLIB) -o $@ $(LIBS)

spice2delay$(EXEEXT): spice2delay.o $(HASHLIB) $(LIBERTYLIB)
	$(CC) $(LDFLAGS) spice2delay.o $(HASHLIB) $(LIBERTYLIB) -o $@ $(LIBS)

rc2dly$(EXEEXT): rc2dly.o $(LIBERTYLIB)
	$(CC) $(LDFLAGS) rc2dly.o $(LIBERTYLIB) -o $@ $(LIBS)

install: $(TARGETS)
	@echo "Installing verilog and BDNET file format handlers"
	$(INSTALL) -d $(DESTDIR)${BININSTALL}
	@for target in $(TARGETS); do \
	   $(INSTALL) $$target $(DESTDIR)${BININSTALL} ;\
	done
	@echo "Installing links to graywolf, qrouter, odin_ii, and abc"
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f graywolf;  ln -s $(QFLOW_GRAYWOLF_PATH) graywolf)
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f qrouter;  ln -s $(QFLOW_QROUTER_PATH) qrouter)
	@if test "${HAVE_ABC}" = "1"; then \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f abc;  ln -s $(QFLOW_ABC_PATH) abc); \
	fi
	@if test "${HAVE_MAGIC}" = "1"; then \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f magic;  ln -s $(QFLOW_MAGIC_PATH) magic); \
	fi
	@if test "${HAVE_NETGEN}" = "1"; then \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f netgen;  ln -s $(QFLOW_NETGEN_PATH) netgen); \
	fi
	@if test "${HAVE_YOSYS}" = "1"; then \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f yosys;  ln -s $(QFLOW_YOSYS_PATH) yosys); \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f yosys-abc;  ln -s $(QFLOW_YOSYS_PATH)-abc yosys-abc); \
	fi
	@if test "${HAVE_OPENTIMER}" = "1"; then \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f ot-shell;  ln -s $(QFLOW_OPENTIMER_PATH) ot-shell); \
	fi
	@if test "${HAVE_OPENSTA}" = "1"; then \
	(cd $(DESTDIR)${BININSTALL};  $(RM) -f sta;  ln -s $(QFLOW_OPENSTA_PATH) sta); \
	fi

uninstall:
	$(RM) -rf ${BININSTALL}

clean:
	$(RM) -f $(OBJECTS) $(HASHLIB) $(LIBERTYLIB)
	$(RM) -f $(TARGETS)

veryclean:
	$(RM) -f $(OBJECTS) $(HASHLIB) $(LIBERTYLIB)
	$(RM) -f $(TARGETS)

distclean:
	$(RM) -f $(OBJECTS) $(HASHLIB) $(LIBERTYLIB)
	$(RM) -f $(TARGETS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFS) -c $< -o $@
