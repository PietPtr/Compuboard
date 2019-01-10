# psftools
#
# a simple suite of utilities to provide text based editing of psf files.
#
# Author: Gunnar Zötl <gz@tset.de>, 2016.   
# Released under the terms of the MIT/X11 license. See file LICENSE for details.

# directory for installation
BINDIR=/usr/local/bin

# directories for tests: temp dir for test files, will be created for and
# removed after the tests
TESTDIR=./tests
# where to find the linux console font files (or other psf files)
CONSOLEFONTDIR=/usr/share/consolefonts

# build targets
ALL = psfc psfd psfid psft

# build flags
CC = gcc
CFLAGS = -Wall -Wextra -g
LD = gcc
LDFLAGS = -g

all: $(ALL)

$(ALL): %: %.o psf.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c psf.h psftools_version.h
	$(CC) $(CFLAGS) -o $@ -c $<

install: all
	cp $(ALL) $(BINDIR)

clean:; rm -rf *.o $(ALL) *.psf $(TESTDIR)

# test: roundtrip all installed psf fonts and compare results
test: all
	@mkdir -p $(TESTDIR)
	@rm -f $(TESTDIR)/*
	@cp $(CONSOLEFONTDIR)/* $(TESTDIR)
	@gzip -d $(TESTDIR)/*.gz
	@failed=0; \
	for f in $(TESTDIR)/*.psf; do \
		./psfd $$f $$f.txt; \
		if [ $$? != "0" ]; then failed=$$(($$failed + 1)); fi; \
		./psfc $$f.txt $$f.1; \
		if [ $$? != "0" ]; then failed=$$(($$failed + 1)); fi; \
		./psfd $$f.1 $$f.1.txt; \
		if [ $$? != "0" ]; then failed=$$(($$failed + 1)); fi; \
		diff -q $$f $$f.1; \
		if [ $$? != "0" ]; then failed=$$(($$failed + 1)); fi; \
		diff -q $$f.txt $$f.1.txt; \
		if [ $$? != "0" ]; then failed=$$(($$failed + 1)); fi; \
	done; \
	echo Failed: $$failed
	@rm -rf $(TESTDIR)
