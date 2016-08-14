/*
 * tbl-util.h - type table utilities.
 *
 *
 * Mike Sample
 *
 * Copyright (C) 1993 Michael Sample
 *            and the University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/c-lib/inc/tbl-dec.h,v 1.1 2006/10/16 09:04:59 joywu Exp $
 * $Log: tbl-dec.h,v $
 * Revision 1.1  2006/10/16 09:04:59  joywu
 * no message
 *
 * Revision 1.1  1995/07/27 08:55:52  rj
 * first check-in after being merged into .../c-lib/.
 *
 */


AVal *TblDecode PROTO ((TBL *tbl, char *modName, char *typeName, BUF_TYPE b, unsigned long int *bytesDecoded));

AVal *TblDecodeType PROTO ((TBLType *tblT, BUF_TYPE b, int implicit, unsigned long int *bytesDecoded, ENV_TYPE env));
