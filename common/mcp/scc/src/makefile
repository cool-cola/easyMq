INC = -I./ -I../../public/

#gprof is -pg , usage: stop ccs , gprof ccs ./gmon.out , -fPIC Position-Independent Code
#CC=g++ -g -Wall -O2 -fPIC -D_BUFFMNG_APPEND_SKIP
CC=g++ -g -Wall -O0 -fPIC -D_BUFFMNG_APPEND_SKIP
LIB = -ldl -lpthread
BIN = scc.bin

##################################
PUBLIC = ../../public/

OBJ = mainsvrd.o \
mainctrl.o \
$(PUBLIC)EpollFlow.o \
$(PUBLIC)Base.o \
$(PUBLIC)tlib_cfg.o \
$(PUBLIC)tlib_log.o \
$(PUBLIC)IdxObjMng.o \
$(PUBLIC)Sem.o \
$(PUBLIC)CodeQueue.o \
$(PUBLIC)HashFunc.o

all: $(BIN)

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)
	cp $(BIN) ../bin/
	chmod +x ../bin/*

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN} ${SO}

cleanall:
	rm -f *.o ${BIN} ${SO} ${OBJ}

