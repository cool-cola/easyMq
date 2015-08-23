/*
 * file: compiler/core/meta.c
 *
 * Copyright � 1994 1995 Robert Joop <rj@rainbow.in-berlin.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program and the associated libraries are distributed in the hope
 * that they will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License and GNU Library General
 * Public License for more details.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/compiler/core/meta.c,v 1.1 2006/10/16 09:07:19 joywu Exp $
 * $Log: meta.c,v $
 * Revision 1.1  2006/10/16 09:07:19  joywu
 * no message
 *
 * Revision 1.2  1995/08/17 15:00:11  rj
 * the PDU flag belongs to the metacode, not only to the tcl interface. (type and variable named adjusted)
 *
 * Revision 1.1  1995/07/27  10:54:11  rj
 * new file
 *
 */

#include <stdio.h>

#include "snacc.h"
#include "meta.h"

#if META

int isMetaPDU PARAMS ((module, type, pdus),
  const char *module _AND_
  const char *type _AND_
  MetaPDU *pdus)
{
  MetaPDU *pdu;

  for (pdu=pdus; pdu; pdu=pdu->next)
    if (!strcmp (pdu->module, module) && !strcmp (pdu->type, type))
    {
      pdu->used = TRUE;
      return TRUE;
    }

  return FALSE;
}

#endif /* META */