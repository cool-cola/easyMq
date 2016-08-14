/*
 * file: .../c++-lib/inc/init.h
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/c++-lib/inc/init.h,v 1.1 2006/10/16 09:03:11 joywu Exp $
 * $Log: init.h,v $
 * Revision 1.1  2006/10/16 09:03:11  joywu
 * no message
 *
 * Revision 1.1  1995/07/27 09:22:35  rj
 * new file: .h file containing a declaration for a function defined in a C++ file, but with C linkage.
 *
 */

extern
#ifdef __cplusplus
	"C"
#endif
	     int Snacc_Init (Tcl_Interp *interp);
