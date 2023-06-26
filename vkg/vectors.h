/*

#ifndef VKVG_VECTORS_H
#define VKVG_VECTORS_H

#include "vkg_internal.h"

typedef struct {
	float x;
	float y;
}vec2;

static const vec2 vec2_unit_x = {1.f,0};
static const vec2 vec2_unit_y = {0,1.f};

typedef struct {
	double x;
	double y;
}vec2d;

//const vec2d vec2d_unit_x = {1.0,0};
//const vec2d vec2d_unit_y = {0,1.0};

typedef struct {
	float x;
	float y;
	float z;
}vec3;

typedef struct {
	union {
		float x;
		float r;
		float xMin;
	};
	union {
		float y;
		float g;
		float yMin;
	};
	union {
		float z;
		float width;
		float b;
		float xMax;
	};
	union {
		float w;
		float height;
		float a;
		float yMax;
	};

}vec4;

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t z;
	uint16_t w;
}vec4i16;

typedef struct {
	int16_t x;
	int16_t y;
}vec2i16;

typedef struct {
	vec2 row0;
	vec2 row1;
}mat2;

// compute length of float vector 2d
vkg_inline	float vec2_length(vec2 v){
	return sqrtf (v.x*v.x + v.y*v.y);
}
// compute normal direction vector from line defined by 2 points in double precision
vkg_inline	vec2d vec2d_line_norm(vec2d a, vec2d b)
{
	vec2d d = {b.x - a.x, b.y - a.y};
	double md = sqrt (d.x*d.x + d.y*d.y);
	d.x/=md;
	d.y/=md;
	return d;
}
// compute normal direction vector from line defined by 2 points
vkg_inline	vec2 vec2_line_norm(vec2 a, vec2 b)
{
	vec2 d = {b.x - a.x, b.y - a.y};
	float md = sqrtf (d.x*d.x + d.y*d.y);
	d.x/=md;
	d.y/=md;
	return d;
}
// compute sum of two double precision vectors
vkg_inline	vec2d vec2d_add (vec2d a, vec2d b){
	return (vec2d){a.x + b.x, a.y + b.y};
}
// compute subbstraction of two double precision vectors
vkg_inline	vec2d vec2d_sub (vec2d a, vec2d b){
	return (vec2d){a.x - b.x, a.y - b.y};
}
// multiply 2d vector by scalar
vkg_inline	vec2d vec2d_scale(vec2d a, double m){
	return (vec2d){a.x*m,a.y*m};
}

vkg_inline	vec2d vec2d_perp (vec2d r, vec2d a) {
	return (vec2d){a.y, -a.x};
}

vkg_inline	bool vec2d_isnan (vec2d v){
	return (bool)(isnan (v.x) || isnan (v.y));
}

// test equality of two single precision vectors
vkg_inline	bool vec2_equ (vec2 a, vec2 b){
	return (EQUF(a.x,b.x)&EQUF(a.y,b.y));
}
// compute sum of two single precision vectors
vkg_inline	vec2 vec2_add (vec2 a, vec2 b){
	return (vec2){a.x + b.x, a.y + b.y};
}
// compute subbstraction of two single precision vectors
vkg_inline	vec2 vec2_sub (vec2 a, vec2 b){
	return (vec2){a.x - b.x, a.y - b.y};
}
// multiply 2d vector by scalar
vkg_inline	vec2 vec2_mult_s(vec2 a, float m){
	return (vec2){a.x*m,a.y*m};
}
// devide 2d vector by scalar
vkg_inline	vec2 vec2_div_s(vec2 a, float m){
	return (vec2){a.x/m,a.y/m};
}
// normalize float vector
vkg_inline	vec2 vec2_norm(vec2 a)
{
	float m = sqrtf (a.x*a.x + a.y*a.y);
	return (vec2){a.x/m, a.y/m};
}
// compute perpendicular vector
vkg_inline	vec2 vec2_perp (vec2 a){
	return (vec2){a.y, -a.x};
}
// compute opposite of single precision vector
vkg_inline	void vec2_inv (vec2* v){
	v->x = -v->x;
	v->y = -v->y;
}
// test if one component of float vector is nan
vkg_inline	bool vec2_isnan (vec2 v){
	return (bool)(isnan (v.x) || isnan (v.y));
}
// test if one component of double vector is nan
vkg_inline float vec2_dot (vec2 a, vec2 b) {
	return (a.x * b.x) + (a.y * b.y);
}
vkg_inline float vec2_det (vec2 a, vec2 b) {
	return a.x * b.y - a.y * b.x;
}
vkg_inline float vec2_slope (vec2 a, vec2 b) {
	return (b.y - a.y) / (b.x - a.x);
}


// convert double precision vector to single precision
vkg_inline	vec2 vec2d_to_vec2(vec2d vd){
	return (vec2){(float)vd.x,(float)vd.y};
}
vkg_inline	bool vec4_equ (vec4 a, vec4 b){
	return (EQUF(a.x,b.x)&EQUF(a.y,b.y)&EQUF(a.z,b.z)&EQUF(a.w,b.w));
}
vkg_inline	vec2 mat2_mult_vec2 (mat2 m, vec2 v) {
	return (vec2){
		(m.row0.x * v.x) + (m.row0.y * v.y),
		(m.row1.x * v.x) + (m.row1.y * v.y)
	};
}
vkg_inline	float mat2_det (mat2* m) {
	return (m->row0.x * m->row1.y) - (m->row0.y * m->row1.y);
}
*/
