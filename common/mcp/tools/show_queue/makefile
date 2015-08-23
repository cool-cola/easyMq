INC = -I./ -I../../public/
CC=g++ -g -Wall 
LIB = -ldl -lpthread
BIN = showqueue 

##################################
PUBLIC = ../../public/

OBJ = showqueue.o \
$(PUBLIC)tlib_cfg.o \
$(PUBLIC)CodeQueue.o \
$(PUBLIC)CodeQueueMutil.o \
$(PUBLIC)Sem.o

all: $(BIN) 

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN} ${SO}


