C
C
C  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
C
C  This program is free software; you can redistribute it and/or modify it
C  under the terms of version 2.1 of the GNU Lesser General Public License 
C  as published by the Free Software Foundation.
C
C  This program is distributed in the hope that it would be useful, but
C  WITHOUT ANY WARRANTY; without even the implied warranty of
C  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
C
C  Further, this software is distributed without any warranty that it is
C  free of the rightful claim of any third person regarding infringement 
C  or the like.  Any license provided herein, whether implied or 
C  otherwise, applies only to this software file.  Patent licenses, if
C  any, provided herein do not apply to combinations of this program with 
C  other software, or any other product whatsoever.  
C
C  You should have received a copy of the GNU Lesser General Public 
C  License along with this program; if not, write the Free Software 
C  Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, 
C  USA.
C
C  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
C  Mountain View, CA 94043, or:
C
C  http://www.sgi.com
C
C  For further information regarding this notice, see:
C
C  http://oss.sgi.com/projects/GenInfo/NoticeExplan
C
C



	subroutine eoshift_wd1_s2@(result, source, shift, bound, dim,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
*
CDIR$ ID "@(#) libfi/mpp_shift/eoshift_s_2@.f	92.0	10/08/98 14:37:14"
*
**************************************************************************
*
* Purpose:	Perform a end-of shift of an array in argument SOURCE
*		by SHIFT elements along dimension DIM.  This routine handles
*		integer source arrays of rank 2.  This routine is called
*		from the C routine _EOSHIFT_JP2.
*
* Arguments:	result	- integer array of rank 2.
*		source	- integer array of rank 2.
*		shift	- integer array of rank 1.
*		bound	- integer array of rank 1.
*		dim	- integer scalar: dimension to shift over.
*		sext	- integer array of rank 1: extent of source array.
*		shext	- integer array of rank 1: extent of shift array.
*		bext	- integer array of rank 1: extent of bound array.
*		shflag	- logical scalar: shift scalar flag
*		shftval	- integer scalar: shift value if scalar
*		bndflag	- logical scalar: bound scalar flag
*		bndval	- integer scalar: bound value if scalar
*
* Variables:	mype	- integer scalar: logical PE number.
*
* Intrinsic functions called:
*		my_pe()
*		in_parallel()
*
* Documentation:
*		T3D Fortran 90 Array Intrinsics Interface Description
*		T3D Fortran 90 Array Intrinsics Design Description
*
* Author:	Kenneth Bussart
*		Cray Research, Inc.
*************************************************************************
	implicit	none
	intrinsic	my_pe, in_parallel, home
	integer		mype
	integer		sext(2), shext(1), bext(1)
	integer		shift(shext(1))
	integer		dim
	integer		bound(bext(1))
	integer		result(sext(1),sext(2))
	integer		source(sext(1),sext(2))
	integer		rindx, sindx
	integer		shftval
	logical		shflag, bndflag
	integer		bndval
	integer		i1, i2
*
cdir$	unknown_shared	source, shift, bound
*
	if (dim .eq. 1) then
	    call eoshift_wd1_s2_dim1@ (result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
	else
	    call eoshift_wd1_s2_dim2@ (result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
	endif
*
	return
	end


	subroutine eoshift_wd2_s2@(result, source, shift, bound, dim,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
*
**************************************************************************
*
* Purpose:	Perform a end-of shift of an array in argument SOURCE
*		by SHIFT elements along dimension DIM.  This routine handles
*		complex source arrays of rank 2.  This routine is called
*		from the C routine _EOSHIFT_CP2.
*
* Arguments:	result	- complex array of rank 2.
*		source	- complex array of rank 2.
*		shift	- integer array of rank 1.
*		bound	- complex array of rank 1.
*		dim	- integer scalar: dimension to shift over.
*		sext	- integer array of rank 1: extent of source array.
*		shext	- integer array of rank 1: extent of shift array.
*		bext	- integer array of rank 1: extent of bound array.
*		shflag	- logical scalar: shift scalar flag
*		shftval	- integer scalar: shift value if scalar
*		bndflag	- logical scalar: bound scalar flag
*		bndval	- complex scalar: bound value if scalar
*
* Variables:	mype	- integer scalar: logical PE number.
*
* Intrinsic functions called:
*		my_pe()
*		in_parallel()
*
* Documentation:
*		T3D Fortran 90 Array Intrinsics Interface Description
*		T3D Fortran 90 Array Intrinsics Design Description
*
* Author:	Kenneth Bussart
*		Cray Research, Inc.
*************************************************************************
	implicit	none
	intrinsic	my_pe, in_parallel, home
	integer		mype
	integer		sext(2), shext(1), bext(1)
	integer		shift(shext(1))
	integer		dim
	complex		bound(bext(1))
	complex		result(sext(1),sext(2))
	complex		source(sext(1),sext(2))
	integer		rindx, sindx
	integer		shftval
	logical		shflag, bndflag
	complex		bndval
	integer		i1, i2
*
cdir$	unknown_shared	source, shift, bound
*
	if (dim .eq. 1) then
	    call eoshift_wd2_s2_dim1@ (result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
	else
	    call eoshift_wd2_s2_dim2@ (result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
	endif
*
	return
	end


	subroutine eoshift_wd1_s2_dim1@(result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
*
**************************************************************************
*
* Purpose:	Perform a end-of shift of an array in argument SOURCE
*		by SHIFT elements along dimension DIM.  This routine handles
*		integer source arrays of rank 2.  This routine is called
*		from the C routine _EOSHIFT_JP2@.
*
* Arguments:	result	- integer array of rank 2.
*		source	- integer array of rank 2.
*		shift	- integer array of rank 1.
*		bound	- integer array of rank 1.
*		sext	- integer array of rank 1: extent of source array.
*		shext	- integer array of rank 1: extent of shift array.
*		bext	- integer array of rank 1: extent of bound array.
*		shflag	- logical scalar: shift scalar flag
*		shftval	- integer scalar: shift value if scalar
*		bndflag	- logical scalar: bound scalar flag
*		bndval	- integer scalar: bound value if scalar
*
* Variables:	mype	- integer scalar: logical PE number.
*
* Intrinsic functions called:
*		my_pe()
*		in_parallel()
*
* Documentation:
*		T3D Fortran 90 Array Intrinsics Interface Description
*		T3D Fortran 90 Array Intrinsics Design Description
*
* Author:	Kenneth Bussart
*		Cray Research, Inc.
*************************************************************************
	implicit	none
	intrinsic	my_pe, in_parallel, home
	integer		mype
	integer		sext(2), shext(1), bext(1)
	integer		shift(shext(1))
	integer		dim
	integer		bound(bext(1))
	integer		result(sext(1),sext(2))
	integer		source(sext(1),sext(2))
	integer		rindx, sindx
	integer		shftval
	logical		shflag, bndflag
	integer		bndval
	integer		i1, i2
*
cdir$	unknown_shared	source, shift, bound
*
	include "eoshift_s2_dim1@.fh"
*
	return
	end


	subroutine eoshift_wd1_s2_dim2@(result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
*
**************************************************************************
*
* Purpose:	Perform a end-of shift of an array in argument SOURCE
*		by SHIFT elements along dimension DIM.  This routine handles
*		integer source arrays of rank 2.  This routine is called
*		from the C routine _EOSHIFT_JP2@.
*
* Arguments:	result	- integer array of rank 2.
*		source	- integer array of rank 2.
*		shift	- integer array of rank 1.
*		bound	- integer array of rank 1.
*		sext	- integer array of rank 1: extent of source array.
*		shext	- integer array of rank 1: extent of shift array.
*		bext	- integer array of rank 1: extent of bound array.
*		shflag	- logical scalar: shift scalar flag
*		shftval	- integer scalar: shift value if scalar
*		bndflag	- logical scalar: bound scalar flag
*		bndval	- integer scalar: bound value if scalar
*
* Variables:	mype	- integer scalar: logical PE number.
*
* Intrinsic functions called:
*		my_pe()
*		in_parallel()
*
* Documentation:
*		T3D Fortran 90 Array Intrinsics Interface Description
*		T3D Fortran 90 Array Intrinsics Design Description
*
* Author:	Kenneth Bussart
*		Cray Research, Inc.
*************************************************************************
	implicit	none
	intrinsic	my_pe, in_parallel, home
	integer		mype
	integer		sext(2), shext(1), bext(1)
	integer		shift(shext(1))
	integer		dim
	integer		bound(bext(1))
	integer		result(sext(1),sext(2))
	integer		source(sext(1),sext(2))
	integer		rindx, sindx
	integer		shftval
	logical		shflag, bndflag
	integer		bndval
	integer		i1, i2
*
cdir$	unknown_shared	source, shift, bound
*
	include "eoshift_s2_dim2@.fh"
*
	return
	end


	subroutine eoshift_wd2_s2_dim1@(result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
*
**************************************************************************
*
* Purpose:	Perform a end-of shift of an array in argument SOURCE
*		by SHIFT elements along dimension DIM.  This routine handles
*		complex source arrays of rank 2.  This routine is called
*		from the Fortran routine _EOSHIFT_CP2@.
*
* Arguments:	result	- complex array of rank 2.
*		source	- complex array of rank 2.
*		shift	- integer array of rank 1.
*		bound	- complex array of rank 1.
*		sext	- integer array of rank 1: extent of source array.
*		shext	- integer array of rank 1: extent of shift array.
*		bext	- integer array of rank 1: extent of bound array.
*		shflag	- logical scalar: shift scalar flag
*		shftval	- integer scalar: shift value if scalar
*		bndflag	- logical scalar: bound scalar flag
*		bndval	- complex scalar: bound value if scalar
*
* Variables:	mype	- integer scalar: logical PE number.
*
* Intrinsic functions called:
*		my_pe()
*		in_parallel()
*
* Documentation:
*		T3D Fortran 90 Array Intrinsics Interface Description
*		T3D Fortran 90 Array Intrinsics Design Description
*
* Author:	Kenneth Bussart
*		Cray Research, Inc.
*************************************************************************
	implicit	none
	intrinsic	my_pe, in_parallel, home
	integer		mype
	integer		sext(2), shext(1), bext(1)
	integer		shift(shext(1))
	integer		dim
	complex		bound(bext(1))
	complex		result(sext(1),sext(2))
	complex		source(sext(1),sext(2))
	integer		rindx, sindx
	integer		shftval
	logical		shflag, bndflag
	complex		bndval
	integer		i1, i2
*
cdir$	unknown_shared	source, shift, bound
*
	include "eoshift_s2_dim1@.fh"
*
	return
	end


	subroutine eoshift_wd2_s2_dim2@(result, source, shift, bound,
     *		sext, shext, bext, shflag, shftval, bndflag, bndval)
*
**************************************************************************
*
* Purpose:	Perform a end-of shift of an array in argument SOURCE
*		by SHIFT elements along dimension DIM.  This routine handles
*		complex source arrays of rank 2.  This routine is called
*		from the Fortran routine _EOSHIFT_CP2@.
*
* Arguments:	result	- complex array of rank 2.
*		source	- complex array of rank 2.
*		shift	- integer array of rank 1.
*		bound	- complex array of rank 1.
*		sext	- integer array of rank 1: extent of source array.
*		shext	- integer array of rank 1: extent of shift array.
*		bext	- integer array of rank 1: extent of bound array.
*		shflag	- logical scalar: shift scalar flag
*		shftval	- integer scalar: shift value if scalar
*		bndflag	- logical scalar: bound scalar flag
*		bndval	- complex scalar: bound value if scalar
*
* Variables:	mype	- integer scalar: logical PE number.
*
* Intrinsic functions called:
*		my_pe()
*		in_parallel()
*
* Documentation:
*		T3D Fortran 90 Array Intrinsics Interface Description
*		T3D Fortran 90 Array Intrinsics Design Description
*
* Author:	Kenneth Bussart
*		Cray Research, Inc.
*************************************************************************
	implicit	none
	intrinsic	my_pe, in_parallel, home
	integer		mype
	integer		sext(2), shext(1), bext(1)
	integer		shift(shext(1))
	integer		dim
	complex		bound(bext(1))
	complex		result(sext(1),sext(2))
	complex		source(sext(1),sext(2))
	integer		rindx, sindx
	integer		shftval
	logical		shflag, bndflag
	complex		bndval
	integer		i1, i2
*
cdir$	unknown_shared	source, shift, bound
*
	include "eoshift_s2_dim2@.fh"
*
	return
	end
