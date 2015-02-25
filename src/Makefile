#
# Makefile
#

PROG=olive
SRCS=main.c db_handler.c campaign.c slog.c ast_handler.c htp_handler.c common.c base64.c
CFLAGS+=-Wall -I. -I/usr/include/mysql -I/usr/include/event2 
LDADD+=-lmysqlclient -levhtp -lpthread -levent -lssl -lcrypto -levent_openssl -lzmq -lm -ljansson -lsqlite3


.include <bsd.prog.mk>
#.include <scripts.mk>
