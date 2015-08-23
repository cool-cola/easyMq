PUBLIC=../../public/
CFLAGS+= -g -Wall 
INC+= -I../../public/
LIB+=-lpthread  

OBJ= \
	$(PUBLIC)TSocket.o \
	$(PUBLIC)tlib_cfg.o \
	presscall.o \
	user_func.o 

TARGET=presscall
#############################################################
$(TARGET):$(OBJ)
	g++ $(CFLAGS) -o $@ $^ $(LIB) 

%.o: %.cpp
	g++ $(CFLAGS) $(INC) -c -o $@ $< 	 

clean:
	rm -f *.o
	rm -f $(TARGET) 
