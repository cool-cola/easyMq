CC=g++ -g -Wall -O2 -fPIC 
LIB = -ldl -lpthread 

SO = asn13_complete.so asn20_complete.so http_msg_complete.so totallen_complete.so memcached_complete.so
##################################

all: $(SO)

memcached_complete.so: memcached_complete.cpp
	rm -f $@
	$(CC) -o $@ $^ -shared
	cp memcached_complete.so ../../bin/
	
asn13_complete.so: asn13_complete.cpp
	rm -f $@
	$(CC) -o $@ $^ -shared
	cp asn13_complete.so ../../bin/

asn20_complete.so: asn20_complete.cpp
	rm -f $@
	$(CC) -o $@ $^ -shared
	cp asn20_complete.so ../../bin/

http_msg_complete.so: http_msg_complete.cpp
	rm -f $@
	$(CC) -o $@ $^ -shared
	cp http_msg_complete.so ../../bin/

totallen_complete.so: totallen_complete.cpp
	rm -f $@
	$(CC) -o $@ $^ -shared
	cp totallen_complete.so ../../bin/
	
clean:
	rm -f *.o ${SO}

cleanall:
	rm -f *.o ${SO}
	

