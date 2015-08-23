# file: .../tbl-tools/ptbl/makefile
#
# $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tbl-tools/berdecode/makefile,v 1.1 2006/10/16 09:18:09 joywu Exp $
# $Log: makefile,v $
# Revision 1.1  2006/10/16 09:18:09  joywu
# no message
#
# Revision 1.1  1997/08/27 15:55:49  wan
# Added generic table decoding, debug routines, berdecode, and asnwish.
#
# Revision 1.1  1997/02/15 19:26:20  rj
# first check-in
#

include ../../makehead

TOP		= ../..

ASN1_C_LIB_DIR	= $(TOP)/c-lib
ASN1_C_INC_DIR	= $(ASN1_C_LIB_DIR)/inc
TTBL_ASN1_LIB	= $(ASN1_C_LIB_DIR)/libasn1ctbl.a

HFILES = \

CFILES = \
	berdecode.c

OFILES = \
	berdecode.o

DISTFILES = \
	README		\
	makefile	\
	$(HFILES)	\
	$(CFILES)

CPPFLAGS	+= -I$(TOP) -I$(ASN1_C_INC_DIR)

#-------------------------------------------------------------------------------

all::		berdecode

berdecode:	$(OFILES) $(TTBL_ASN1_LIB)
		$(REASON)
		$(CC) $(LDFLAGS) -o $@ $(OFILES) $(TTBL_ASN1_LIB) $(LIBS)

install-sh:
		ln $(TOP)/install-sh $@

$(bindir):
		$(TOP)/mkinstalldirs $@

install::	berdecode install-sh $(bindir)

install::
		$(INSTALL_PROGRAM) berdecode $(bindir)/

clean::
		$(RM) *.o *~ core .emacs*

clobber::
		$(RM) berdecode

include ../../maketail
