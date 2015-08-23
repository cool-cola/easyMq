/*
 * file: .../tbl-tools/ptbl/pasn1.h
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/tbl-tools/ptbl/pasn1.h,v 1.1 2006/10/16 09:18:09 joywu Exp $
 * $Log: pasn1.h,v $
 * Revision 1.1  2006/10/16 09:18:09  joywu
 * no message
 *
 * Revision 1.1  1997/02/15 19:26:22  rj
 * first check-in
 *
 */


void PrintTblTypeDefInAsn1 PROTO ((FILE *f, TBL *tbl, TBLTypeDef *td));

void PrintTblTypeInAsn1 PROTO ((FILE *f, TBL *tbl, TBLTypeDef *td, TBLType *t));

void PrintTblInAsn1 PROTO ((FILE *f, TBL *tbl));
