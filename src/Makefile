#
# Makefile
#

PROG=olive
SRCS=main.c \
	db_handler.c \
	camp_handler.c \
	slog.c \
	ast_handler.c \
	htp_handler.c \
	common.c \
	base64.c \
	memdb_handler.c \
	chan_handler.c \
	agent_handler.c \
	cmd_handler.c \
	sip_handler.c

.if DEV
CFLAGS+=-Wall
.endif

CFLAGS+=-I. -I/usr/include/event2 `mysql_config --cflags`
LDADD+=`mysql_config --libs` -levhtp -lpthread -levent -lssl -lcrypto -levent_openssl -lzmq -lm -ljansson -lsqlite3 -luuid



.include <bsd.prog.mk>
#.include <scripts.mk>
