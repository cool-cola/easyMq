/*
 * compiler/back-ends/c-gen/gen-vals.h
 *
 * MS 92
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/compiler/back-ends/c-gen/gen-vals.h,v 1.1 2006/10/16 09:06:30 joywu Exp $
 * $Log: gen-vals.h,v $
 * Revision 1.1  2006/10/16 09:06:30  joywu
 * no message
 *
 * Revision 1.3  1995/07/25 18:44:13  rj
 * file name has been shortened for redundant part: c-gen/gen-c-vals -> c-gen/gen-vals.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  03:48:15  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:48:34  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

void PrintCValueDef PROTO ((FILE *src, CRules *r, ValueDef *v));

void PrintCValueExtern  PROTO ((FILE *hdr, CRules *r, ValueDef *v));

void PrintCOidValue PROTO ((FILE *f, CRules *r, AsnOid *oid));
