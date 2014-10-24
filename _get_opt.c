/*
 * The following is derived from getopt() source placed into the public domain by AT&T
 * It has been modified somewhat to serve our own purposes
 */

#include <stdio.h>
#include <string.h>

#include "_stdtype.h"
#include "_get_opt.h"

#define GETOPTEOF (-1)

char   *opt_arg = NULL;
int     opt_ind = 1;
int     opt_err = 0;

static  int sp = 1;

int _get_opt( int argc, char *argv[], char *optstring)
{
  int   c;
  char* cp;

  if (sp == 1)
  {
    if (opt_ind >= argc
        || argv[opt_ind][0] != '-'
        || argv[opt_ind][1] == '\0')
    {
      return (GETOPTEOF);
    }
    else if (strcmp(argv[opt_ind], "--") == 0)
    {
      opt_ind++;
      return (GETOPTEOF);
    }
  }

  c = argv[opt_ind][sp];
  if (c == ':' || (cp = strchr(optstring, c)) == NULL)
  {
    if (opt_err)
    {
      printf("%s: illegal option -- %c\n", argv[0], c);
    }
    if (argv[opt_ind][++sp] == '\0')
    {
      opt_ind++;
      sp = 1;
    }
    opt_arg = NULL;
    return ('?');
  }

  if (*++cp == ':')
  {
    if (argv[opt_ind][sp + 1] != '\0')
    {
      opt_arg = &argv[opt_ind++][sp + 1];
    }
    else if (++opt_ind >= argc)
    {
      if (opt_err)
      {
        printf("%s: option requires an argument -- %c\n", argv[0], c);
      }
      sp = 1;
      opt_arg = NULL;
      return ('?');
    }
    else
    {
      opt_arg = argv[opt_ind++];
    }

    sp = 1;
  }
  else
  {
    if (argv[opt_ind][++sp] == '\0')
    {
      sp = 1;
      opt_ind++;
    }
    opt_arg = NULL;
  }
  return (c);
}
