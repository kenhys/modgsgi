##
##  Makefile -- Build procedure for sample gsgi Apache module
##  Autogenerated via ``apxs -n gsgi -g''.
##

builddir=.
top_srcdir=/usr/lib/httpd
top_builddir=/usr/lib/httpd
include /usr/share/httpd/build/special.mk

#   the used tools
APXS=apxs
APACHECTL=apachectl

#   additional defines, includes and libraries
#DEFS=-Dmy_define=my_value
#INCLUDES=-Imy/include/dir
#LIBS=-Lmy/lib/dir -lmylib
INCLUDES=`gauche-config -I`
LIBS=`gauche-cofig -L`

#   the default target
all: local-shared-build

#   install the shared object file into Apache
install: install-modules-yes

#   cleanup
clean:
	-rm -f mod_gsgi.o mod_gsgi.lo mod_gsgi.slo mod_gsgi.la

#   simple test
test: reload
	lynx -mime_header http://localhost/gsgi

#   install and activate shared object by reloading Apache to
#   force a reload of the shared object file
reload: install restart

#   the general Apache start/restart/stop
#   procedures
start:
	$(APACHECTL) start
restart:
	$(APACHECTL) restart
stop:
	$(APACHECTL) stop

