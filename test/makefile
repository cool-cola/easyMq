SODIR = ../so/
CC=g++ -g -Wall -std=c++11
LIB= -pthread -ldl
BIN = test_clnt.bin

#DBINC	= -I/usr/include/mysql/ -I/usr/local/mysql/include/
#DBLIB	= -L/usr/local/mysql/lib/ -L/usr/lib/mysql/ -lmysqlclient -lz -ldl

PUBLIC = ../common/mcp/public/
INC = -I$(PUBLIC) -I$(SODIR)
##################################
INC += $(DBINC)
LIB += $(DBLIB)

BASEOBJ = 	$(PUBLIC)TSocket.o \
			../common/easyMsg.o

SOOBJ =

MYOBJ =	test_clnt.o

##################################
OBJ = $(BASEOBJ) $(SOOBJ) $(MYOBJ)
##################################
all: $(BIN)

$(BIN): ${OBJ}
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)

%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN}

