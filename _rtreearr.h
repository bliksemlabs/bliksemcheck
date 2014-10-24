#ifndef _RTREE_ARRAY_H_
#define _RTREE_ARRAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_stdtype.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
	AULONG   m_IncrBy;
	AULONG   m_Count;
	AULONG   m_Reserved;
	AULONG   m_DefSize;
	AULONG   m_EltSize;
	AUBYTE  *m_Elt;
	void (*remove_functie) (void * elt); 
} CArray;

AULONG  _CountCArray     (CArray *ca);
AULONG  _GrowCArray      (CArray *ca,     AULONG by);
CArray *_MakeCArray      (AULONG defsize, AULONG defincrease, AULONG eltsize);
CArray *_DeleteCArray    (CArray *ca);
AULONG  _AddCArray       (CArray *ca,  AUBYTE *newelt);
AUBYTE *_GetCArray       (CArray *ca,  AULONG  index);
AUBYTE *_BSearchCArray   (CArray *ca,  void   *key, int (*fn) (const void *keyval, const void *elt) );
void    _SortCArray      (CArray *ca,  int   (*fn) (const void *elt1, const void *elt2) );


AULONG  _GetIndexCArray  (CArray *ca,  AUBYTE *elt);
void _SetFreeFunctie(CArray * ca,void (*fn) (void * elt));

#ifdef __cplusplus
}
#endif

#endif
