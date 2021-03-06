// file: .../c++-lib/src/asn-list.C
//
//  Mike Sample
//  92/07/02
//
// *** NOTE - this is not tested and not used  ****
//     snacc generates a new class for each list type,
//     methods and all.
//       (gcc choked on templates)
// Copyright (C) 1992 Michael Sample and the University of British Columbia
//
// This library is free software; you can redistribute it and/or
// modify it provided that this copyright/license information is retained
// in original form.
//
// If you modify this file, you must clearly indicate your changes.
//
// This source code is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/c++-lib/src/asn-list.C,v 1.1 2006/10/16 09:03:11 joywu Exp $
// $Log: asn-list.C,v $
// Revision 1.1  2006/10/16 09:03:11  joywu
// no message
//
// Revision 1.8  1997/09/04 13:54:09  wan
// A little more portability
//
// Revision 1.7  1997/01/02 08:42:39  rj
// names of Tcl*-functions fixed (obviously they weren't needed :-)
//
// Revision 1.6  1995/07/24  20:18:15  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.
//
// Revision 1.5  1995/02/18  14:06:02  rj
// #pragma interface/implementation are GNU specific and need to be wrapped.
//
// Revision 1.4  1994/10/08  04:18:25  rj
// code for meta structures added (provides information about the generated code itself).
//
// code for Tcl interface added (makes use of the above mentioned meta code).
//
// virtual inline functions (the destructor, the Clone() function, BEnc(), BDec() and Print()) moved from inc/*.h to src/*.C because g++ turns every one of them into a static non-inline function in every file where the .h file gets included.
//
// made Print() const (and some other, mainly comparison functions).
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.3  1994/08/31  23:38:24  rj
// FALSE/TRUE turned into false/true
//
// Revision 1.2  1994/08/28  10:01:14  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:02  rj
// first check-in. for a list of changes to 1.1 please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"

#ifdef __GNUG__
#pragma implementation
#endif

#include "asn-list.h"

template <class T>
void AsnList<T>::SetCurrElmt (unsigned long int index)
{
    unsigned long int i;
    curr = first;
    for (i = 0; (i < (count-1)) && (i < index); i++)
        curr = curr->next;
}


// print routine for lists
template <class T>
ostream &operator << (ostream &os, AsnList<T> &l)
{
    os << "SEQUENCE OF { ";

    l.SetCurrToFirst();
    for (; l.Curr() != NULL; l.GoNext())
    {
        os << *l.Curr();
        if (l.Curr() != l.Last())
            os << ", ";
    }

    os << " }";
    return os;
}



// alloc new list elmt, put at end of list
//  and return the component type
template <class T>
T &AsnList<T>::Append()
{
    AsnListElmt *newElmt;

    newElmt = new AsnListElmt;

    newElmt->next = NULL;

    if (last == NULL)
    {
	newElmt->prev = NULL;
	first = last  = newElmt;
    }
    else
    {
	newElmt->prev = last;
        last->next    = newElmt;
	last          = newElmt;
    }

    count++;

    return newElmt->elmt;

} /* AsnList::Append */


// alloc new list elmt, put at beggining of list
//  and return the component type
template <class T>
T &AsnList<T>::Prepend()
{
    AsnListElmt *newElmt;

    newElmt = new AsnListElmt;

    newElmt->prev = NULL;

    if (first == NULL)
    {
	newElmt->next = NULL;
	first = last  = newElmt;
    }
    else
    {
	newElmt->next = first;
        first->prev   = newElmt;
	first         = newElmt;
    }

    count++;

    return newElmt->elmt;

} /* AsnList::Prepend */

template <class T>
AsnList<T>& AsnList<T>::AppendAndCopy (T &elmt)
{
    AsnListElmt *newElmt;

    newElmt = new AsnListElmt;

    newElmt->elmt = elmt;

    newElmt->next = NULL;

    if (last == NULL)
    {
	newElmt->prev = NULL;
	first = last  = newElmt;
    }
    else
    {
	newElmt->prev = last;
        last->next    = newElmt;
	last          = newElmt;
    }

    count++;

    return this;

} /* AppendAndCopy */

template <class T>
AsnList<T>& AsnList<T>::PrependAndCopy (T &elmt)
{
    AsnListElmt *newElmt;

    newElmt = new AsnListElmt;

    newElmt->elmt = elmt;

    newElmt->prev = NULL;

    if (first == NULL)
    {
	newElmt->next = NULL;
	first = last  = newElmt;
    }
    else
    {
	newElmt->next = first;
        first->prev   = newElmt;
	first         = newElmt;
    }

    count++;

    return this;

} /* PrependAndCopy */


template <class T>
AsnLen AsnList<T>::BEncContent (BUF_TYPE b)
{
    AsnListElmt *currElmt;
    AsnLen sum = 0;

    for (currElmt = last; currElmt != NULL; currElmt = currElmt->prev)
	sum += currElmt->elmt.BEnc (b);

    return sum;
}

template <class T>
void AsnList<T>::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
    T listElmt;
    AsnTag listElmtTagId;
    AsnLen localBytesDecoded = 0;
    AsnLen listElmtLen = 0;


     while ((localBytesDecoded < elmtLen) || (elmtLen == INDEFINITE_LEN))
     {
         listElmtTagId = BDecTag (b, bytesDecoded, env);

         //if ((listElmtTagId == EOC) && (elmtLen == INDEFINITE_LEN))
         if ((listElmtTagId == EOC_TAG_ID) && (elmtLen == INDEFINITE_LEN))
             break;


         listElmt = Append();
         listElmtLen = BDecLen (b, bytesDecoded, env);
         listElmt.BDecContent (b, listElmtTagId, listElmtLen, localBytesDecoded, env);
     }
    bytesDecoded += localBytesDecoded;

}  /* AsnList<T>::BDecContent */

template <class T>
AsnLen AsnList<T>::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l =  BEncContent (b);
    l += BEncDefLen (b, l);
    l += BEncTag1 (b, UNIV, CONS, SEQ_TAG_CODE);
    return l;
}

template <class T>
void AsnList<T>::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    if (BDecTag (b, bytesDecoded, env) != MAKE_TAG_ID (UNIV, CONS, SEQ_TAG_CODE))
    {
	Asn1Error << "AsnList::BDec: ERROR tag on SEQUENCE OF is wrong." << endl;
	longjmp (env,-54);
    }
    elmtLen = BDecLen (b, bytesDecoded, env);

    BDecContent (b, MAKE_TAG_ID (UNIV, CONS, SEQ_TAG_CODE), elmtLen, bytesDecoded, env);
}

template <class T, class U>
int ListsEquiv (AsnList<T>& l1, AsnList<U>& l2)
{
    if (l1.Count() != l2.Count())
        return false;

    l1.SetCurrToFirst();
    l2.SetCurrToFirst();

    for (; l1.Curr() != NULL; l1.GoNext(), l2.GoNext())
    {
        if (*l1.Curr() !=  *l2.Curr())
        {
            return false;
        }
    }
    return true;
}

#if 0
#if META

const AsnTypeDesc AsnList::_desc (NULL, NULL, false, AsnTypeDesc::SET_or_SEQUENCE_OF, NULL);

const AsnTypeDesc *AsnList::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnList::TclGetVal (Tcl_Interp *interp) const
{
	return TCL_ERROR;
}

int AsnList::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
	return TCL_ERROR;
}

#endif /* TCL */
#endif /* META */
#endif /* 0 */
