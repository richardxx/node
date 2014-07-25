CC=g++
CFLAGS= -w -g
.PHONY: all install clean
.DEFAULT: all


all: jsweeter


type-info.o: type-info.hh type-info.cc automata.hh
	${CC} ${CFLAGS} -c type-info.cc

automata.o: automata.hh automata.cc type-info.hh
	${CC} ${CFLAGS} -c automata.cc

modeler.o: modeler.hh modeler.cc events.h infer-deopt.hh type-info.hh
	${CC} ${CFLAGS} -c modeler.cc

infer-deopt.o: infer-deopt.hh infer-deopt.cc type-info.hh automata.hh
	$(CC) $(CFLAGS) -c infer-deopt.cc

jsweeter: jsweeter.cc options.h automata.o modeler.o infer-deopt.o type-info.o
	${CC} ${CFLAGS} jsweeter.cc automata.o modeler.o infer-deopt.o type-info.o -o jsweeter

install:
	cp jsweeter ${HOME}/bin/


clean:
	-rm jsweeter *.o 


