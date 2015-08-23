ASN=../../../../kccd/kccd_asn/
SNACC=../../../../tools/snacc-2.0
PUBLIC = ../../../public/

LIB = -ldl -lpthread -L${SNACC}/c++-lib -lasn1c++
INC = -I./ -I../ -I${PUBLIC} -I${SNACC}/c++-lib/inc/ -I${SNACC} -I${ASN}
FLG= -Wno-deprecated -Wno-non-template-friend 
CC=g++ -g -Wall -O2 -fPIC 

BIN = kccd_admin_msg.so 
##################################

OBJ = kccd_admin_msg.o \
	$(ASN)kccd_msg_pkt.o \
	$(PUBLIC)Base.o \
	$(PUBLIC)Sem.o \
	$(PUBLIC)IdxObjMng.o \
	$(PUBLIC)HashFunc.o \
	$(PUBLIC)CodeQueue.o 
	
all: $(BIN)

$(BIN): ${OBJ}
	rm -f $@
	$(CC) $(FLG) -o $@ $(INC) $^ $(LIB) -shared

%.o: %.cpp
	$(CC) $(FLG) $(INC) -c -o $@ $<
	
clean:
	rm -f *.o ${BIN}
	rm -f *.o

cleanall:
	rm -f *.o ${BIN} ${OBJ}

