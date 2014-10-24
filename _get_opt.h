/*
   TITLE
      Get commandline options

   SYNOPSYS
      #include "get_opt.h"

   DESCRIPTION
      get_opt() is fully compatible with getopt(3) except for
      the error messages which are optional in this version.
      The messages:
   '%s: illegal option -- %c' and
   '%s: option requires an argument -- %c'
      are only displayed if the global variable opt_err <> 0
      (it is initialised to 0).

      To prevent conflicts with the standard getopt(), the
      usual symbols have been renamed:

      - get_opt() (instead of getopt)
      - opt_ind (instead of optind)
      - opt_arg (instead of optarg)
      - opt_err (unknown in getopt(3C))

   AUTHOR(S)
      AT&T (released in public domain, adapted by J. Valkonet)

   CREATION DATE
      Tue Aug 29 16:22:20 MES 1995
   
   RCS
      $Id: _get_opt.h,v 1.1 2014/09/23 11:01:57 risedpij Exp $
 */

#ifndef GET_OPT_H
#define GET_OPT_H

#ifdef __cplusplus
extern "C" {
#endif

/****************************
* Imported Functions
****************************/
extern  char *opt_arg;
extern  int   opt_ind;
extern  int   opt_err;
extern  int _get_opt(int argc, char* argv[], char* optstring );

#ifdef __cplusplus
}
#endif

#endif /* GET_OPT_H */
