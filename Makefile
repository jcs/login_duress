# $OpenBSD$

PROG=	login_duress
MAN=	login_duress.8
SRCS=	login.c login_duress.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
