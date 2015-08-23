INC = -I./ 
CC=g++ -g -Wall 
LIB = -ldl
BIN = MirrorShm

##################################

OBJ = MirrorShm.o

all: $(BIN) 

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN} 


