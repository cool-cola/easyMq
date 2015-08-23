/*
 * compiler/core/depedency.h
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/compiler/core/dependency.h,v 1.1 2006/10/16 09:07:19 joywu Exp $
 * $Log: dependency.h,v $
 * Revision 1.1  2006/10/16 09:07:19  joywu
 * no message
 *
 * Revision 1.2  1994/10/08 03:48:38  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:49:01  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


void SortAllDependencies PROTO ((ModuleList *m));
