NAME=					luajail
VERSION=				1
REVISION=				0.0b1
PREFIX?=				/
.if defined(A)
KDEFINED=				-I/usr/local/include/lua53 -fsanitize=safe-stack
KLDFLAGS=				-L/usr/local/lib
PREFIX=					/usr/local/lib/lua/5.3
KUSE=					lua-5.3 m jail kvm util l base
.endif
KUSES=					${KUSE}
CC=						clang
MAINTAINER=				aakauova@kcs.kz
PATHLIB=
SRCDIR!=				/bin/pwd
INCDIR=					${DESTDIR}${PREFIX}/include
SOURCE=					src
CFLAGS=					-c -O3  -fpic ${KDEFINED}
LDFLAGS=				-O3 -pipe -DHARDENEDBSD -shared ${KLDFLAGS}
LDCONFIG=				/sbin/ldconfig
LIST_FILES=				${NAME}.c jail.c command.c config.c state.c jaillex.c jailparse.c jexec.c
LIST_FILES_L=			jaillex.l 
LIST_FILES_Y=			jailparse.y

MD=						/bin/mkdir -p
SED=					/usr/bin/sed
TR=						/usr/bin/tr
AWK=					/usr/bin/awk
YACC=					/usr/bin/yacc
LEX=					/usr/bin/lex
FIND=					/usr/bin/find
.if !defined(VERBOSE)
_v=
.endif

all:
	${_v} cd ${SRCDIR}/${SOURCE}; ${LEX} -ojaillex.c ${LIST_FILES_L}
	${_v} cd ${SRCDIR}/${SOURCE}; ${YACC} -d -v ${LIST_FILES_Y}
	${_v} cd ${SRCDIR}/${SOURCE}; cp y.tab.c jailparse.c
.if !exists( ${SOURCE}/.make.done)
. for i in ${LIST_FILES}
	${_v} cd ${SRCDIR}/${SOURCE}; ${CC} ${CFLAGS} ${i}
. endfor
	${_v} cd ${SRCDIR}/${SOURCE}; ${CC} -o lib${NAME}.so.${VERSION} ${LDFLAGS} $$(echo `${FIND} * -type f -name "*.o"` )  -llua-5.3 $$([ -n "${KUSES}" ] && echo "${KUSES}" | ${AWK} '{ for (i=1; i<=NF; i++) printf "-l"$$i" " }' || echo -n "" ) 
	${_v} cd ${SRCDIR}/${SOURCE}; ${AR} -rc lib${NAME}.a $$(echo `${FIND} * -type f -name "*.o"` )
.endif
configure:
install:
.if !exists(${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.so)
	${INSTALL} ${SRCDIR}/${SOURCE}/lib${NAME}.so.${VERSION} ${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.so.${VERSION}
	${INSTALL} ${SRCDIR}/${SOURCE}/lib${NAME}.a ${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.a
	${INSTALL} -ls lib${NAME}.so.${VERSION} ${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.so
.else
	@echo Already installing, please deinstall befor install
.endif
deinstall:
	${_v} rm -Rfv ${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.so ${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.a  ${DESTDIR}${PREFIX}${PATHLIB}/lib${NAME}.so.${VERSION} ${INCDIR}/${NAME}.h
clean:
	${_v} cd ${SRCDIR}/${SOURCE}; rm -Rfv *.o *.so *.a lib${NAME}.so.${VERSION} 

test:
	@echo ${CFLAGS}
