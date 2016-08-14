/*
 * file: tcl-p.c
 * purpose: check and return via exit code whether the tcl interface needs to be made
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tcl-p.c,v 1.1 2006/10/16 09:21:02 joywu Exp $
 * $Log: tcl-p.c,v $
 * Revision 1.1  2006/10/16 09:21:02  joywu
 * no message
 *
 * Revision 1.1  1995/07/25 22:24:48  rj
 * new file
 *
 */

#define COMPILER	1

#include "snacc.h"

main()
{
#if TCL
  return 0;
#else
  return 1;
#endif
}
