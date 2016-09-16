SODIR = ../so/
CC=g++ -g -Wall -std=c++11
LIB= -pthread -ldl

PRODUCER_BIN = producer.bin

CONSUMER_BIN = consumer.bin

BIN = $(PRODUCER_BIN) $(CONSUMER_BIN)

PUBLIC = ../common/mcp/public/
INC = -I$(PUBLIC) -I$(SODIR)

BASEOBJ = 	$(PUBLIC)TSocket.o \
			../common/easyMsg.o

PRODUCER_OBJ = producer.o $(BASEOBJ)

CONSUMER_OBJ = consumer.o $(BASEOBJ)

all: $(CONSUMER_BIN) $(PRODUCER_BIN)

$(PRODUCER_BIN):$(PRODUCER_OBJ)
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)


$(CONSUMER_BIN): $(CONSUMER_OBJ)
	rm -f $@
	$(CC) -o $@ $(INC) $^ $(LIB)



%.o: %.cpp
	$(CC) $(INC) -c -o $@ $<

clean:
	rm -f *.o ${BIN}

