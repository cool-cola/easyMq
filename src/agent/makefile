OPT = -g -Wall -Wno-deprecated -Wno-non-template-friend
CXX = g++ $(OPT)
CC= gcc $(OPT)

BIN = easy_mq_mcp
INC = -I./ -I../../common
LIB = -lpthread -ldl

##################################
PUBLIC = ../../common/mcp/public/
INC += -I$(PUBLIC)

##################################
# mysql
INC += -I/usr/include/mysql/ -I/usr/local/mysql/include/
LIB += -L/usr/local/mysql/lib -lmysqlclient

##################################
COMMON_OBJ = frame_svrd.o frame_ctrl.o \
	$(PUBLIC)Base.o \
	$(PUBLIC)tlib_cfg.o \
	$(PUBLIC)tlib_log.o \
	$(PUBLIC)CodeQueue.o \
	$(PUBLIC)CodeQueueMutil.o \
	$(PUBLIC)EpollFlow.o \
	$(PUBLIC)Statistic.o \
	../../common/comm_mysql/comm_mysql.o

OBJS = $(COMMON_OBJ) \
	agent_ctrl.o \
	easyMQAgent.o \
	frame_svrd.o \
	frame_ctrl.o

all: $(BIN)

$(BIN): $(OBJS)
	rm -f $@
	$(CXX) -o $@ $(INC) $^ $(LIB)
	cp -f $@ ../../bin/

%.o: %.cpp
	$(CXX) $(INC) -c -o $@ $<
%.o: %.c
	$(CC) $(INC) -c -o $@ $<
clean:
	rm -f *.o $(BIN) $(OBJS)

cleanall:
	rm -f *.o $(BIN) $(OBJS)

