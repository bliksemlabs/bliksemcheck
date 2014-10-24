
#include <stdio.h>
#include "_rtreearr.h"

AULONG _CountCArray (CArray* ca)
{
	return ca->m_Count;
}

AULONG _GrowCArray (CArray* ca, AULONG by)
{
	AULONG	 newsize;

	if ( by == 0 )
		return TRUE;

	newsize   = ca->m_Reserved + by;
	ca->m_Elt = (AUBYTE *) realloc (ca->m_Elt, newsize * ca->m_EltSize);
	if (ca->m_Elt)
		ca->m_Reserved = newsize;

	if ( ! ca->m_Elt )
		return FALSE;

	return TRUE;
}

CArray *_MakeCArray (AULONG defsize, AULONG defincrease, AULONG eltsize)
{
	CArray	*ca;

	ca = (CArray *) malloc (sizeof (CArray));
	ca->m_Elt      = 0;
	ca->m_Count    = 0;
	ca->m_Reserved = 0;
	ca->m_EltSize  = eltsize;
	ca->m_IncrBy   = defincrease;
	ca->m_DefSize  = defsize;
	if (! _GrowCArray(ca, defsize)) {
		free (ca);
		return NULL;
	}

	ca->remove_functie = NULL;
	return ca;
}

CArray *_DeleteCArray (CArray* ca)
{
	int tel;

	if (ca == NULL)
		return NULL;

	if (ca->m_Elt)
	{
		if (ca->remove_functie != NULL)
		{
			for (tel = 0; tel < (int) ca->m_Count; tel++)
			{	ca->remove_functie(ca->m_Elt + tel * ca->m_EltSize);
			}
		}

		free (ca->m_Elt);
	}

	free (ca);
	return NULL;
}

AULONG _AddCArray (CArray* ca, AUBYTE* newelt)
{
	if (ca->m_Count == ca->m_Reserved) {
		if ( ( ca->m_IncrBy == 0 ) || ( ! _GrowCArray (ca, ca->m_IncrBy) ) )
 			return FALSE;
	}
	memcpy (ca->m_Elt + ca->m_Count * ca->m_EltSize, newelt, ca->m_EltSize);
	return (++(ca->m_Count));
}

AUBYTE *_GetCArray (CArray* ca, AULONG index)
{
	if (index >= ca->m_Count)
		return NULL;

	return (ca->m_Elt +  index * ca->m_EltSize );
}

AUBYTE *_BSearchCArray (CArray* ca, void *key, int (*fn) (const void *keyval, const void *elt) )
{
	return (AUBYTE *) bsearch (key, ca->m_Elt, ca->m_Count, ca->m_EltSize, fn);
}

void _SortCArray(CArray* ca, int (*fn) (const void *elt1, const void *elt2) )
{
	qsort(ca->m_Elt, ca->m_Count, ca->m_EltSize, fn);
}

AULONG _GetIndexCArray(CArray *ca, AUBYTE *elt)
{

	int    displacement;
	AULONG index;
	
	displacement = ( elt - ca->m_Elt );

	if ( displacement < 0 )
		return 0;
	
	index = displacement / ca->m_EltSize;

	if ( index > ca->m_Count )
		return 0;

	return index;
}

void _SetFreeFunctie(CArray * ca,void (*fn) (void * elt))
{
	ca->remove_functie = fn;
}
