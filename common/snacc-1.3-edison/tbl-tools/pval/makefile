# file: .../tbl-tools/pval/makefile
#
# $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tbl-tools/pval/makefile,v 1.1 2006/10/16 09:18:09 joywu Exp $
# $Log: makefile,v $
# Revision 1.1  2006/10/16 09:18:09  joywu
# no message
#
# Revision 1.1  1997/02/15 19:15:08  rj
# first check-in
#

include ../../makehead

TOP		= ../..

ASN1_C_LIB_DIR	= $(TOP)/c-lib
ASN1_C_INC_DIR	= $(ASN1_C_LIB_DIR)/inc
TTBL_ASN1_LIB	= $(ASN1_C_LIB_DIR)/libasn1ctbl.a

CFILES = \
	pval.c

OFILES = \
	pval.o

DISTFILES = \
	README		\
	makefile	\
	$(CFILES)

BUFFLAG		= -DUSE_GEN_BUF

CPPFLAGS	+= -I$(TOP) -I$(ASN1_C_INC_DIR) $(BUFFLAG)

#-------------------------------------------------------------------------------

all::		pval

pval:		$(OFILES) $(TTBL_ASN1_LIB)
		$(REASON)
		$(CC) $(LDFLAGS) -o $@ $(OFILES) $(TTBL_ASN1_LIB) $(LIBS)

install-sh:
		ln $(TOP)/install-sh $@

$(bindir):
		$(TOP)/mkinstalldirs $@

install::	pval install-sh $(bindir)

install::
		$(INSTALL_PROGRAM) pval $(bindir)/

clean::
		$(RM) *.o *~ core .emacs*

clobber::
		$(RM) pval

include ../../maketail
