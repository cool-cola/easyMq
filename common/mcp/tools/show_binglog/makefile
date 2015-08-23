INC = -I./ -I../../public/
CC=g++ -g -Wall 
LIB = -ldl
BIN = show_binlog 

##################################
PUBLIC = ../../public/

OBJ = show_binlog.o \
$(PUBLIC)BinlogNR.o 

all: $(BIN) 

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN} ${SO}


