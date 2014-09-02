
ifeq ($(OS),Windows_NT)
include makefile.windows
else

all: libconf_ux.h wcm__

libconf_ux.h: libtester/libconf.create
	cd libtester && ./libconf.create

wcm__:
	make -f makefile.linux
	cat info.txt

clean: libconf_ux.h
	rm -f libconf_ux.h
	make -f makefile.linux clean

install: libconf_ux.h
	make -f makefile.linux install

endif
