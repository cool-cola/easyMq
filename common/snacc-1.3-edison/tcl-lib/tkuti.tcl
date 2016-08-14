# file: tkuti.tcl
# miscellaneous Tk utilities.
#
# $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tcl-lib/tkuti.tcl,v 1.1 2006/10/16 09:19:19 joywu Exp $
# $Log: tkuti.tcl,v $
# Revision 1.1  2006/10/16 09:19:19  joywu
# no message
#
# Revision 1.1  1997/01/01 23:12:03  rj
# first check-in
#

proc getpos {w xn yn} \
{
  upvar $xn x $yn y
  set geom [wm geometry $w]
  scan $geom {%dx%d+%d+%d} w h x y
}
