/*
 * Copyright 2004 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License 
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU Lesser General Public 
  License along with this program; if not, write the Free Software 
  Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, 
  USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


/*  $Header: /proj/osprey/CVS/open64/osprey1.0/libF77/c_exp.c,v 1.1.1.1 2005/10/21 19:00:00 marcel Exp $ */
#include <math.h>
#include "moremath.h"
#include "cmplx.h"
#include "defalias.h"

complex __cexp(float zreal, float zimag)
{
  float expx;
  complex r;

  expx = expf(zreal);
  r.real = expx * cosf(zimag);
  r.imag = expx * sinf(zimag);
  return r;
}

complex c_exp_(complex *z)
{
  return __cexp(z->real, z->imag);
}

#if defined(BUILD_OS_DARWIN)
/* Mach-O doesn't support aliases */
complex c_exp(complex *z) { return c_exp_(z); }
#else /* defined(BUILD_OS_DARWIN) */
defalias(c_exp_, c_exp);
#endif /* defined(BUILD_OS_DARWIN) */
