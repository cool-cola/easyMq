
CC = gcc
CFLAGS = -g -O2 -DFLEX_IN_USE
CXX = c++
CXXFLAGS = -g -O2 -Wno-deprecated -Wno-non-template-friend

CPPFLAGS += -I./inc -I../

OFILES = \
	src/asn-any.o		\
	src/asn-bits.o		\
	src/asn-bool.o		\
	src/asn-enum.o		\
	src/asn-int.o		\
	src/asn-len.o		\
	src/asn-list.o		\
	src/asn-null.o		\
	src/asn-octs.o		\
	src/asn-oid.o		\
	src/asn-real.o		\
	src/asn-tag.o		\
	src/asn-type.o		\
	src/asn-useful.o	\
	src/hash.o		\
	src/meta.o		\
	src/print.o		\
	src/tcl-if.o		\
	src/str-stk.o		\
	src/tkAppInit.o

LIB	= libasn1c++.a

#-------------------------------------------------------------------------------

libasn1c++.a : $(OFILES)
	ar rv $@ $?
	ranlib $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.C
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OFILES) *.a
