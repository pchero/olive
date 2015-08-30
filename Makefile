#Makefile
# Created on: Aug 12, 2014
#     Author: pchero
# Master makefile for olive project

SUBDIR= src 

PROD_TARGET?=olive

.if exists(Makefile.inc)
.include "Makefile.inc"
.endif

mktree:
	[ -d ${prefix} ] || mkdir -p -m 755 ${BINDIR} ${SBINDIR} ${LIBDIR} \
		${LOGDIR} ${ETCDIR} ${MANDIR} ${INCSDIR} ${RUNDIR} \
		${SSLDIR} ${WWWDIR}


#.if ("${PROD_TARGET}"=="olive")
#SUBDIR+= src
#.endif

#SUBDIR_TARGETS+= realclean

.include <bsd.subdir.mk>