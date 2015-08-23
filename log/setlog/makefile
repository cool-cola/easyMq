BIN = setlog
CC=g++ -g -Wall 

OBJ = setlog.o

all: $(BIN)

$(BIN): ${OBJ}
	$(CC) -o $@  $^ 
	cp $(BIN) ../../bin/

%.o: %.cpp
	$(CC) -c -o $@ $<

clean:
	rm -f *.o ${BIN} ${OBJ}
