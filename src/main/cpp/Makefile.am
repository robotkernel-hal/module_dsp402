lib_LTLIBRARIES = libmodule_dsp402.la

libmodule_dsp402_la_SOURCES  = moddsp402.cpp
libmodule_dsp402_la_CXXFLAGS = -Wall -I$(top_srcdir)/src -I$(top_builddir) \
							@ROBOTKERNEL_CFLAGS@
libmodule_dsp402_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(top_builddir)/share/ln/* $(prefix)/share/ln/

