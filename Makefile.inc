#Makefile.inc
# Created on: Aug 12, 2014
#     Author: pchero

# For now always compile with debug
#DEBUG=1

# Do not install with ownership because RPM build wants to control this from its spec file and cast fatal error
.ifdef RPMBUILD
LIBOWN?=
MANOWN?=
BINOWN?=
STRIP_FLAG?=
BINMODE?=       755
NONBINMODE?=    644
.endif

# Define our paths for installation etc.
prefix   ?= /opt/olive
#DESTDIR ?= ${prefix}
BINDIR    = ${prefix}/bin
SBINDIR   = ${prefix}/sbin
LIBDIR    = ${prefix}/lib
LOGDIR    = ${prefix}/var/log
RUNDIR    = ${prefix}/var/run
WWWDIR    = ${prefix}/var/www
ETCDIR    = ${prefix}/etc
MANDIR    = ${prefix}/share/man
INCSDIR   = ${prefix}/include
SSLDIR    = ${ETCDIR}/ssl

.ifndef __PRE_MK
__PRE_MK=""

.ifdef PIDDIR
afterinstall:
	[ -d ${PIDDIR} ] || mkdir -p -m 755 ${PIDDIR}
.endif

.ifdef prefix
CFLAGS+=-DPREFIX='"$(prefix)"'
.endif

.endif

.if exists(Makefile.local)
.include "Makefile.local"
.endif
