/*
 * acconfig.h
 *
 * by Gary Wong, 1999
 *
 * $Id: acconfig.h,v 1.2 2000/01/12 21:55:03 gtw Exp $
 */

/* Installation directory (used to determine PKGDATADIR below). */
#undef DATADIR

@BOTTOM@

/* The directory where the weights and databases will be stored. */
#define PKGDATADIR DATADIR "/" PACKAGE

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
