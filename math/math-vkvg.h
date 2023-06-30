#ifndef _math_h_
#define _math_h_

#include <string.h>
#include <math.h>
#include <glm/glm.h>



// compute normal direction vector from line defined by 2 points in double precision
static inline vec2d vec2d_line_norm(vec2d r, vec2d a, vec2d b) {
    vec2d vec2d_sub(r, a, b);
	double md = sqrt(r[0]*r[0] + r[1]*r[1]);
    return 
	r.x /= md;
	r.y /= md;
}

// compute normal direction vector from line defined by 2 points
vec2 vec2f_line_norm(vec2f r, vec2f a, vec2f b) {
	vec2f d;
    vec2f_sub(d, b.x - a.x, b.y - a.y);
    
	float md = sqrtf(d.x*d.x + d.y*d.y);
	d.x /= md;
	d.y /= md;
	return d;
}

// compute perpendicular vector
vec2d vec2d_perp(vec2d r, vec2d a) {
    double ax = a.x;
    double ay = a.y;
    r.x      =  ay;
    r.y      = -ax;
}
vec2f vec2f_perp(vec2f r, vec2f a) {
    float  ax = a.x;
    float  ay = a.y;
    r.x      =  ay;
    r.y      = -ax;
}

bool vec2d_isnan(vec2d v) { return (bool)(isnan(v[0]) || isnan (v[1])); }

#define EPSILON 1e-16
#define WITHIN_EPSILON(a, b) (fabsf(a-(b)) <= EPSILON)

// test equality of two single precision vectors
bool vec2f_equ(vec2f a, vec2f b) { return (WITHIN_EPSILON(a.x, b.x) && WITHIN_EPSILON(a.y, b.y)); }

void vec2f_inv(vec2f r, vec2f v) {
	r[0] = -v[0];
    r[1] = -v[1];
}

bool vec2_isnan  (vec2 v)         { return (bool)(isnan (v.x) || isnan (v.y)); }
float vec2_dot   (vec2 a, vec2 b) { return (a.x * b.x) + (a.y * b.y); }
float vec2_det   (vec2 a, vec2 b) { return  a.x * b.y  -  a.y * b.x; }
float vec2_slope (vec2 a, vec2 b) { return (b.y - a.y) / (b.x - a.x); }


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

#endif