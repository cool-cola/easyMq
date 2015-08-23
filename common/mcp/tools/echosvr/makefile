INC = -I./ 
CC=g++ -g -Wall 
LIB = -ldl
BIN = echosvr

##################################

OBJ = echosvr.o

all: $(BIN) 

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN} 


