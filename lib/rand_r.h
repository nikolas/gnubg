/*
 * rand_r.h
 *
 * by Gary Wong, 2000
 *
 * $Id: rand_r.h,v 1.1 2000/01/03 17:50:40 gtw Exp $
 */

/* Don't use this prototype in Cygwin32 , -Oystein */

#ifndef _RAND_R_H_
#ifndef __CYGWIN__
extern unsigned int rand_r( unsigned int *pnSeed );
#endif /* CYGWIN */
#endif
