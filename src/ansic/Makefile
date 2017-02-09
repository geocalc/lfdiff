
CFLAGS=-O0 -ggdb -Wall -I.
#CXXFLAGS=$(CFLAGS)
#LDFLAGS=
#LINK.o = $(LINK.cc)	# set cpp linker by default

OBJECTS=lfdiff.o difflist.o
#HEADER=$(patsubst %.o,%.h,$(OBJECTS))

all: lfdiff

#$(OBJECTS): $(HEADER)
#%.cpp: %.h

#lfdiff: $(HEADER)
lfdiff: $(OBJECTS)

clean:
#	-rm lfdiff
	-rm lfdiff $(OBJECTS)

#$(HEADER):

PHONY: clean all
