/*
 * compiler/back_ends/idl_gen/rules.h
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/compiler/back-ends/idl-gen/rules.h,v 1.1 2006/10/16 09:06:30 joywu Exp $
 * $Log: rules.h,v $
 * Revision 1.1  2006/10/16 09:06:30  joywu
 * no message
 *
 * Revision 1.1  1997/01/01 20:25:39  rj
 * first draft
 *
 * Revision 1.2  1994/10/08  03:47:50  rj
 */

/* see ../../core/asn1module.h for IDLTDI (C++ type def info) */

typedef struct IDLRules
{
  int		maxDigitsToAppend;

  char		*typePrefix,
		*typeSuffix;

  char		*choiceEnumSuffix;
  char		*choiceUnionFieldName;	/* what the name of the choice's union is */
  char		*choiceUnionName;	/* name (tag) for choice union def name */

  int		capitalizeNamedElmts;

  IDLTDI	typeConvTbl[BASICTYPE_MACRODEF + 1];
} IDLRules;

extern IDLRules idlRulesG;
