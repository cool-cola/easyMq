/*
 * compiler/back_ends/idl_gen/gen_vals.h
 *
 * MS 92
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/compiler/back-ends/idl-gen/gen-vals.h,v 1.1 2006/10/16 09:06:30 joywu Exp $
 * $Log: gen-vals.h,v $
 * Revision 1.1  2006/10/16 09:06:30  joywu
 * no message
 *
 * Revision 1.1  1997/01/01 20:25:37  rj
 * first draft
 *
 */


void PrintIDLValueDef PROTO ((FILE *src, IDLRules *r, ValueDef *v));

void PrintIDLValueExtern PROTO ((FILE *hdr, IDLRules *r, ValueDef *v));

void PrintIDLValuesClass PROTO ((FILE *f, IDLRules *r, Value *v));

void PrintIDLValueInstatiation PROTO ((FILE *f, IDLRules *r, Value *v));

void PrintIDLOidValue PROTO ((FILE *f, IDLRules *r, AsnOid *oid));

void PrintIDLIntValue PROTO ((FILE *f, IDLRules *r, AsnInt oid));
