#
# Makefile
#

PROG=olive
SRCS=main.c \
	db_handler.c \
	campaign.c \
	slog.c \
	ast_handler.c \
	htp_handler.c \
	common.c \
	base64.c \
	memdb_handler.c \
	call_handler.c \
	agent_handler.c \
	cmd_handler.c

CFLAGS+=-Wall -I. -I/usr/include/event2 `mysql_config --cflags`
LDADD+=`mysql_config --libs` -levhtp -lpthread -levent -lssl -lcrypto -levent_openssl -lzmq -lm -ljansson -lsqlite3 -luuid



.include <bsd.prog.mk>
#.include <scripts.mk>
