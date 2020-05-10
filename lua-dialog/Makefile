NAME=					luadialog
VERSION=				1
REVISION=				0.0b1
PREFIX?=				/
.if defined(A)
KDEFINED=				-I/usr/local/include/lua53 -fsanitize=safe-stack
KLDFLAGS=				-L/usr/local/lib
PREFIX=					/usr/local/lib/lua/5.3
KUSE=					lua-5.3 m dialog ncursesw
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
LIST_FILES=				${NAME}.c

MD=						/bin/mkdir -p
SED=					/usr/bin/sed
TR=						/usr/bin/tr
AWK=					/usr/bin/awk
FIND=					/usr/bin/find
.if !defined(VERBOSE)
_v=
.endif


all:

.if !exists( ${SOURCE}/.make.done)
. for i in ${LIST_FILES}
	${_v} cd ${SRCDIR}/${SOURCE}; ${CC} ${CFLAGS} ${i}
. endfor
	${_v} cd ${SRCDIR}/${SOURCE}; ${CC} -o lib${NAME}.so.${VERSION} -llua-5.3 $$([ -n "${KUSES}" ] && echo "${KUSES}" | ${AWK} '{ for (i=1; i<=NF; i++) printf "-l"$$i" " }' || echo -n "" )  ${LDFLAGS} $$(echo `${FIND} * -type f -name "*.o"` )
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
