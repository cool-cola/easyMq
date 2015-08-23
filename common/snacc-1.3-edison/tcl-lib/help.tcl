# file: help.tcl
# toplevel widget to display a help text (modal)
#
# $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tcl-lib/help.tcl,v 1.1 2006/10/16 09:19:19 joywu Exp $
# $Log: help.tcl,v $
# Revision 1.1  2006/10/16 09:19:19  joywu
# no message
#
# Revision 1.1  1997/01/01 23:11:54  rj
# first check-in
#

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc help {w helptext} \
{
  set help .help
  set text $help.text
  set sb $help.sb
  set dismiss $help.dismiss

  getpos $w x y
  incr x -100
  toplevel $help -class Dialog
  wm title $help {Help}
  wm transient $help .
  wm geometry $help +$x+$y
  wm minsize $help 0 0

  text $text -borderwidth 2 -relief sunken -yscrollcommand [list $sb set] -width 32 -height 8
  scrollbar $sb -relief sunken -command [list $text yview] -width 10 -cursor arrow
  button $dismiss -text Dismiss -command [list destroy $help]

  pack $dismiss -side bottom -pady 2
  pack $sb -side right -fill y
  pack $text -expand true -fill both

  bind $text <Any-Key> [list destroy $help]

  $text insert end $helptext

  set oldfocus [focus]
  focus $text
  tkwait window $help
  focus $oldfocus
}
