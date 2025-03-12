/*****************************************************************************
 * strerror_l.c: POSIX strerror_l() replacement
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

char *strerror_l(int errnum, locale_t locale) {
    (void)locale;
    return strerror(errnum);
}

