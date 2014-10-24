#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "_stdtype.h"
#include "_date.h"

#define MAX_TIJD_STRING 25
#define REF_TYPE  1
#define REF_JAAR  1990
#define ETMAAL_MNTN    1440 /* minuten in een etmaal */


int  normaal_dagen[12] =
{
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};


int  schrikkel_dagen[12] =
{
  31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

char* maandnaam[] =
{
  "",
  "januari",
  "februari",
  "maart",
  "april",
  "mei",
  "juni",
  "juli",
  "augustus",
  "september",
  "oktober",
  "november",
  "december"
};

char *maandnaam_kort[]= { "jan","feb","mrt","apr","mei","jun","jul","aug","sep","okt","nov","dec"};


char *dagnaam[] =
{
  "zondag",
  "maandag",
  "dinsdag",
  "woensdag",
  "donderdag",
  "vrijdag",
  "zaterdag"
};

char *dagnaam_kort[]=
{
	"zo","ma","di","wo","do","vr","za"
};

char* maandnaam_eng[] =
{
  "",
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
};


char *dagnaam_eng[] =
{
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saterday"
};

AUWORD _datum_naar_dag(int jaar, int maand, int dag)
{
  int dgnr;
  int    *p;

  dgnr = dag - 1;
  if (SCHRIKKEL(jaar))
  {
    p = schrikkel_dagen;
  }
  else
  {
    p = normaal_dagen;
  }

  maand--;
  while (--maand >= 0)
  {
    dgnr += *p;
    p++;
  }
  jaar--;
  for (; jaar >= REF_JAAR; jaar--)
  {
    dgnr += 365;
    if (SCHRIKKEL(jaar))
    {
      dgnr++;
    }
  }
  return(dgnr);
}

void  _dag_naar_datum(int dgnr, int *weekdag, int *dag, int *maand, int *jaar)
{
  int schrikkel, nr_dag;
  int jr, mnd, dg;
  int    *p;

  *weekdag = (dgnr + REF_TYPE) % 7;
  nr_dag = 0;
  jr = REF_JAAR;
  if (SCHRIKKEL(jr))
  {
    schrikkel = 1;
  }
  else
  {
    schrikkel = 0;
  }
  while (dgnr - nr_dag >= 365 + schrikkel)
  {
    nr_dag += 365 + schrikkel;
    jr++;
    if (SCHRIKKEL(jr))
    {
      schrikkel = 1;
    }
    else
    {
      schrikkel = 0;
    }
  }

  mnd = 1;
  if (schrikkel)
  {
    p = schrikkel_dagen;
  }
  else
  {
    p = normaal_dagen;
  }
  while (dgnr - nr_dag >= *p)
  {
    mnd++;
    nr_dag += *p;
    p++;
  }
  dg = dgnr - nr_dag + 1;

  *jaar  = jr;
  *maand = mnd;
  *dag   = dg;
}

