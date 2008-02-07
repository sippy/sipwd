CC?=    gcc
CFLAGS+=-I../siplog
LIBS+=  -L../siplog -lsiplog

# Uncomment this on Solaris
#LIBS+= -lresolv -lsocket -lnsl

all: sipwd

sipwd: main.o
	${CC} -o sipwd main.o ${LIBS}

main.o: main.c
	${CC} ${CFLAGS} -o main.o -c main.c

clean:
	rm -f main.o sipwd
