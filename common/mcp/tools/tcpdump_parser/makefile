CFLAGS+=-g -Wall 
INC+= 
LIB+=-lpthread  

OBJ= \
	parse_tcpdump.o \
	user_func.o 

TARGET=parse_tcpdump
#############################################################
$(TARGET):$(OBJ)
	g++ $(CFLAGS) -o $@ $^ $(LIB) 

%.o: %.cpp
	g++ $(CFLAGS) $(INC) -c -o $@ $< 	 

clean:
	rm -f *.o
	rm -f $(TARGET) 
