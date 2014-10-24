#ifndef F_DATE
#define F_DATE
#include <time.h>
#include "_stdtype.h"

#ifdef __cplusplus
extern "C" {
#endif

#define days_1970_to_refyear   7305

extern AUWORD  _datum_naar_dag(int jaar, int maand, int dag);
extern void   _dag_naar_datum(int dgnr, int *weekdag, int *dag, 
                             int *maand, int *jaar);


#define SCHRIKKEL(jaar) (((jaar)%4 == 0 && (jaar)%100 != 0) || (jaar)%400 == 0)
extern int normaal_dagen[12];
extern int schrikkel_dagen[12];

#ifdef __cplusplus
}
#endif

#endif   /* F_DATE */
