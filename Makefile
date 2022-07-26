# Compiler Info ('g++-4.8 --version')
# g++-4.8 (GCC) 4.8.2 20140120 (Red Hat 4.8.2-15)
# Copyright (C) 2013 Free Software Foundation, Inc.
# This is free software; see the source for copying conditions.  There is NO
# warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# End Compiler Info Output
CXX ?= g++
LINK ?= g++
NDKDIR ?= /usr/local/Nuke11.2v3
CXXFLAGS ?= -g -c \
            -DUSE_GLEW \
			-D_GLIBCXX_USE_CXX11_ABI=0 \
            -I$(NDKDIR)/include \
            -DFN_EXAMPLE_PLUGIN -fPIC -msse 
LINKFLAGS ?= -L$(NDKDIR) \
             -L./ 
LIBS ?= -lDDImage
LINKFLAGS += -shared

fitsReader.so: fitsReader.os
	$(LINK) $(LINKFLAGS) \
	        $(LIBS) \
			-lcfitsio \
			-Wl,--no-undefined \
	        -o $(@) $^

.PRECIOUS : %.os
%.os: %.cpp
	$(CXX) $(CXXFLAGS) -o $(@) $<
%.so: %.os
	$(LINK) $(LINKFLAGS) $(LIBS) -o $(@) $<
%.a: %.cpp
	$(CXX) $(CXXFLAGS) -o lib$(@) $<

ndkexists:
	if test -d $(NDKDIR); \
	then echo "using NDKDIR from ${NDKDIR}"; \
	else echo "NDKDIR dir not found! Please set NDKDIR"; exit 2; \
	fi
clean:
	rm -rf *.os \
	       *.o \
	       *.a \
	       *.so
