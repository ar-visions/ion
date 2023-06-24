//most of the matrix logic is grabbed from cairo, so here is the
//licence:
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */


/// must be folded into glm wrapper

#include <vkg/vkvg_matrix.h>

#define ISFINITE(x) ((x) * (x) >= 0.) /* check for NaNs */

//matrix computations mainly taken from http://cairographics.org

VkeStatus VkvgMatrix::invert ()
{
	float det;

	/* Simple scaling|translation matrices are quite common... */
	if (matrix->xy == 0. && matrix->yx == 0.) {
		matrix->x0 = -matrix->x0;
		matrix->y0 = -matrix->y0;

		if (matrix->xx != 1.f) {
			if (matrix->xx == 0.)
			return VKE_STATUS_INVALID_MATRIX;

			matrix->xx = 1.f / matrix->xx;
			matrix->x0 *= matrix->xx;
		}

		if (matrix->yy != 1.f) {
			if (matrix->yy == 0.)
			return VKE_STATUS_INVALID_MATRIX;

			matrix->yy = 1.f / matrix->yy;
			matrix->y0 *= matrix->yy;
		}

		return VKE_STATUS_SUCCESS;
	}

	/* inv (A) = 1/det (A) * adj (A) */
	det = compute_determinant ();

	if (! ISFINITE (det))
		return VKE_STATUS_INVALID_MATRIX;

	if (det == 0)
		return VKE_STATUS_INVALID_MATRIX;

	compute_adjoint ();
	scalar_multiply (1 / det);

	return VKE_STATUS_SUCCESS;
}
void VkvgMatrix::init_identity (vkvg_matrix_t *matrix) {
	init(1, 0, 0, 1, 0, 0);
}

void VkvgMatrix::init (
	float xx, float yx,
	float xy, float yy,
	float x0, float y0)
{
	matrix->xx = xx; matrix->yx = yx;
	matrix->xy = xy; matrix->yy = yy;
	matrix->x0 = x0; matrix->y0 = y0;
}

void VkvgMatrix::init_translate (vkvg_matrix_t *matrix, float tx, float ty) {
	init (1, 0, 0, 1, tx, ty);
}

void VkvgMatrix::init_scale (vkvg_matrix_t *matrix, float sx, float sy) {
	init (sx, 0, 0, sy, 0, 0);
}

void VkvgMatrix::init_rotate (vkvg_matrix_t *matrix, float radians) {
	float  s;
	float  c;

	s = sinf (radians);
	c = cosf (radians);

	init (matrix, c, s, -s, c, 0, 0);
}
void VkvgMatrix::translate (vkvg_matrix_t *matrix, float tx, float ty)
{
	vkvg_matrix_t tmp;

	tmp.init_translate (tx, ty);

	tmp.multiply (matrix, &tmp, matrix);
}
void VkvgMatrix::scale (vkvg_matrix_t *matrix, float sx, float sy)
{
	vkvg_matrix_t tmp;

	vkvg_matrix_init_scale (&tmp, sx, sy);

	vkvg_matrix_multiply (matrix, &tmp, matrix);
}
void VkvgMatrix::rotate (vkvg_matrix_t *matrix, float radians)
{
	vkvg_matrix_t tmp;

	init_rotate (&tmp, radians);

	multiply (matrix, &tmp, matrix);
}
void VkvgMatrix::multiply (vkvg_matrix_t *result, const vkvg_matrix_t *a, const vkvg_matrix_t *b)
{
	vkvg_matrix_t r;

	r.xx = a->xx * b->xx + a->yx * b->xy;
	r.yx = a->xx * b->yx + a->yx * b->yy;

	r.xy = a->xy * b->xx + a->yy * b->xy;
	r.yy = a->xy * b->yx + a->yy * b->yy;

	r.x0 = a->x0 * b->xx + a->y0 * b->xy + b->x0;
	r.y0 = a->x0 * b->yx + a->y0 * b->yy + b->y0;

	*result = r;
}
void VkvgMatrix::transform_distance (const vkvg_matrix_t *matrix, float *dx, float *dy)
{
	float new_x, new_y;

	new_x = (matrix->xx * *dx + matrix->xy * *dy);
	new_y = (matrix->yx * *dx + matrix->yy * *dy);

	*dx = new_x;
	*dy = new_y;
}
void VkvgMatrix::transform_point (const vkvg_matrix_t *matrix, float *x, float *y)
{
	vkvg_matrix_transform_distance (matrix, x, y);

	*x += matrix->x0;
	*y += matrix->y0;
}
void VkvgMatrix::get_scale (const vkvg_matrix_t *matrix, float *sx, float *sy) {
	*sx = sqrt (matrix->xx * matrix->xx + matrix->xy * matrix->xy);
	/*if (matrix->xx < 0)
		*sx = -*sx;*/
	*sy = sqrt (matrix->yx * matrix->yx + matrix->yy * matrix->yy);
	/*if (matrix->yy < 0)
		*sy = -*sy;*/
}
