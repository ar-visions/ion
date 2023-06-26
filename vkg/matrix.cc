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

#include <vkg/vkg_matrix.h>

#define ISFINITE(x) ((x) * (x) >= 0.) /* check for NaNs */

//matrix computations mainly taken from http://cairographics.org

/// must convert to glm in stages

VkeStatus VkgMatrix::invert()
{
	float det;

	/* Simple scaling|translation matrices are quite common... */
	if (data->xy == 0. && data->yx == 0.) {
		data->x0 = -data->x0;
		data->y0 = -data->y0;

		if (data->xx != 1.f) {
			if (data->xx == 0.)
			return VKE_STATUS_INVALID_MATRIX;

			data->xx = 1.f / data->xx;
			data->x0 *= data->xx;
		}

		if (data->yy != 1.f) {
			if (data->yy == 0.)
			return VKE_STATUS_INVALID_MATRIX;

			data->yy = 1.f / data->yy;
			data->y0 *= data->yy;
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

// member
void VkgMatrix::init_identity() {
	init(1, 0, 0, 1, 0, 0);
}

// member
void VkgMatrix::init (
	float xx, float yx,
	float xy, float yy,
	float x0, float y0)
{
	data->xx = xx; data->yx = yx;
	data->xy = xy; data->yy = yy;
	data->x0 = x0; data->y0 = y0;
}

// static
VkgMatrix VkgMatrix::translated(float tx, float ty) {
	VkgMatrix res;
	res.init (1, 0, 0, 1, tx, ty);
	return res;
}

// static
VkgMatrix VkgMatrix::scaled(float sx, float sy) {
	VkgMatrix res;
	res.init (sx, 0, 0, sy, 0, 0);
	return res;
}

// static
void VkgMatrix::rotated(float radians) {
	float s = sinf(radians);
	float c = cosf(radians);
	init(data, c, s, -s, c, 0, 0);
}

void VkgMatrix::translate(float tx, float ty) {
	VkgMatrix tmp;
	tmp.init_translate (tx, ty);
	multiply(data, &tmp, data);
}

void VkgMatrix::scale(float sx, float sy) {
	VkgMatrix tmp;
	init_scale(&tmp, sx, sy);
	multiply(data, &tmp, data);
}

void VkgMatrix::rotate(float radians) {
	VkgMatrix tmp;
	init_rotate (&tmp, radians);
	multiply (data, &tmp, data);
}

void VkgMatrix::multiply(VkgMatrix a, VkgMatrix b) {
	VkgMatrix r;
	r->xx = a->xx * b->xx + a->yx * b->xy;
	r->yx = a->xx * b->yx + a->yx * b->yy;
	r->xy = a->xy * b->xx + a->yy * b->xy;
	r->yy = a->xy * b->yx + a->yy * b->yy;
	r->x0 = a->x0 * b->xx + a->y0 * b->xy + b->x0;
	r->y0 = a->x0 * b->yx + a->y0 * b->yy + b->y0;
	return r;
}

void VkgMatrix::transform_distance(float *dx, float *dy) {
	float new_x, new_y;
	new_x = (data->xx * *dx + data->xy * *dy);
	new_y = (data->yx * *dx + data->yy * *dy);
	*dx = new_x;
	*dy = new_y;
}

void VkgMatrix::transform_point(float *x, float *y) {
	transform_distance(x, y);
	*x += data->x0;
	*y += data->y0;
}

void VkgMatrix::get_scale(float *sx, float *sy) {
	*sx = sqrt (data->xx * data->xx + data->xy * data->xy);
	*sy = sqrt (data->yx * data->yx + data->yy * data->yy);
}
