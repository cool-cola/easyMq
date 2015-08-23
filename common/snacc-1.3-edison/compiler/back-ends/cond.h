/*
 * compiler/back_ends/cond.h - generate conditional include for C(++) hdr files
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/compiler/back-ends/cond.h,v 1.1 2006/10/16 09:22:19 joywu Exp $
 * $Log: cond.h,v $
 * Revision 1.1  2006/10/16 09:22:19  joywu
 * no message
 *
 * Revision 1.1  1995/07/25 19:13:50  rj
 * PrintConditionalIncludeOpen() and PrintConditionalIncludeClose() moved from back-ends/c-gen/gen-code.[ch].
 *
 * Revision 1.2  1994/10/08  03:48:09  rj
 */


void PrintConditionalIncludeOpen PROTO ((FILE *f, char *fileName));
void PrintConditionalIncludeClose PROTO ((FILE *f, char *fileName));
