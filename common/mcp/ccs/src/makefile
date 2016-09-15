INC = -I./ -I../../public/

#gprof is -pg , usage: stop ccs , gprof ccs ./gmon.out  -fPIC Position-Independent Code
# -D_MUTI_THREAD

CC=g++ -g -Wall -O0 -fPIC -D_BUFFMNG_APPEND_SKIP
LIB = -ldl -lpthread
BIN = ccs.bin
##################################
PUBLIC = ../../public/

OBJ = mainsvrd.o \
mainctrl.o \
auxhandle.o \
$(PUBLIC)EpollFlow.o \
$(PUBLIC)Base.o \
$(PUBLIC)tlib_cfg.o \
$(PUBLIC)tlib_log.o \
$(PUBLIC)IdxObjMng.o \
$(PUBLIC)Sem.o \
$(PUBLIC)thread.o \
$(PUBLIC)CodeQueue.o \
$(PUBLIC)CodeQueueMutil.o \
$(PUBLIC)HashFunc.o \
$(PUBLIC)Statistic.o

all: $(BIN) $(SO)

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)
	cp $(BIN) ../bin/
	chmod +x ../bin/*

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN}

cleanall:
	rm -f *.o ${BIN}  ${OBJ}

