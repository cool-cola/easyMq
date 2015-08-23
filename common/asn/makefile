SNACC = ../snacc-1.3-edison/

ASN := lottery_bons_pkt.asn

################################################################
all : asn 

asn : $(ASN) 
	${SNACC}compiler/snacc -C -s NS_LotteryBons $(ASN)
	@for asn in $(basename $(ASN)); do\
	    mv $(addsuffix .C,$$asn) $(addsuffix .cpp, $$asn);\
	done

clean:
	rm -rfv *_pkt.h
	rm -rfv *_pkt.cpp
	rm -rfv *_pkt.o
	rm -rfv *~
