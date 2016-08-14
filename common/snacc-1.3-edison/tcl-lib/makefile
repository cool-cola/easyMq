# file: .../tcl-lib/makefile
#
# $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tcl-lib/makefile,v 1.1 2006/10/16 09:19:19 joywu Exp $
# $Log: makefile,v $
# Revision 1.1  2006/10/16 09:19:19  joywu
# no message
#
# Revision 1.1  1997/01/01 23:11:57  rj
# first check-in
#

include ../makehead

TOP		= ..

TCLFILES.dist	= \
	bindings.tcl	\
	help.tcl	\
	selbox.tcl	\
	snacced.tcl	\
	tkuti.tcl	\
	uti.tcl

TCLFILES	= \
	$(TCLFILES.dist)	\
	tclIndex

DISTFILES	= \
	$(TCLFILES.dist)	\
	make-snacced	\
	makefile

default:	tclIndex

tclIndex:	$(TCLFILES.dist) makefile
		echo 'auto_mkindex . $(TCLFILES.dist)' | $(TCLSH)

install-sh:
		ln $(TOP)/install-sh $@

install::	$(TCLFILES) install-sh $(tcldir)

$(tcldir):
		$(TOP)/mkinstalldirs $@

install::
		$(INSTALL_PROGRAM) make-snacced	$(bindir)/
		for tcl in $(TCLFILES); do\
		  $(INSTALL_DATA) $$tcl $(tcldir)/;\
		done

clean::
		$(RM) *~ *.dvi *.log *.aux *.toc *.lof *.lot *.bbl *.blg sp_errs .emacs*

clobber::
		$(RM) tclIndex
		$(RM) install-sh

gen-distfiles::	$(DISTFILES)

distfiles::
		echo $(DISTFILES)

# dummy:
depend::
		@true
