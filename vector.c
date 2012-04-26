/*
 *  vector.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#include "vector.h"

#include <stdint.h>
#include <float.h>
#include <math.h>

const float kPif =  3.14159265358979;
const float k2Pif = 6.28318530717959;

/*
 * converts a unsigned integer to a float
 */

float u16tof(uint16_t s) {
  return ((float)s)/((float)INT16RADIX);
}

/*
 * converts a float to a unsigned integer
 */

uint16_t ftou16(float s) {
  return (int16_t)(s*((float)INT16RADIX));
}

/*
 * converts a integer to a float
 */

float i16tof(int16_t s) {
  return ((float)s)/((float)INT16RADIX);
}

/*
 * converts a float to a integer
 */

int16_t ftoi16(float s) {
  return (int16_t)(s*((float)INT16RADIX));
}

Vec2f make2f(float x, float y) {
  Vec2f r;
  r.x = x;
  r.y = y;
  return r;
}

Vec2f neg2f(Vec2f v) {
  return make2f(-v.x, -v.y);
}

Vec2f add2f(Vec2f v1, Vec2f v2) {
  return make2f(v1.x + v2.x, v1.y + v2.y);
}

Vec2f sub2f(Vec2f v1, Vec2f v2) {
  return make2f(v1.x - v2.x, v1.y - v2.y);
}

Vec2f mul2f(Vec2f v, float s) {
  return make2f(v.x*s, v.y*s);
}

Vec2f div2f(Vec2f v, float s) {
  return make2f(v.x/s, v.y/s);
}

float dot2f(Vec2f v1, Vec2f v2) {
  return v1.x*v2.x + v1.y*v2.y;
}

float mag2f(Vec2f v) {
  return sqrt(dot2f(v, v));
}

Vec2f unit2f(Vec2f v) {
  return div2f(v, mag2f(v));
}

Vec2f prj2f(Vec2f v1, Vec2f v2) {
  return mul2f(v1, dot2f(v1, v2)/dot2f(v1, v1));
}

float cmp2f(Vec2f v1, Vec2f v2) {
  return dot2f(v1, v2)/mag2f(v1);
}

int isequal2f(Vec2f v1, Vec2f v2) {
  return v1.x == v2.x && v1.y == v2.y;
}

Vec2f tan2f(float theta) {
  return make2f(cos(theta), sin(theta));
}

float _atan2f(Vec2f dir) {
  return atan2f(dir.y, dir.x);
}

Vec2i32 make2i32(int32_t x, int32_t y) {
  Vec2i32 r;
  r.x = x;
  r.y = y;
  return r;
}

Vec2i32 neg2i32(Vec2i32 v) {
  return make2i32(-v.x, -v.y);
}

Vec2i32 add2i32(Vec2i32 v1, Vec2i32 v2) {
  return make2i32(v1.x + v2.x, v1.y + v2.y);
}

Vec2i32 sub2i32(Vec2i32 v1, Vec2i32 v2) {
  return make2i32(v1.x - v2.x, v1.y - v2.y);
}

Vec2i32 mul2i32(Vec2i32 v, int32_t s) {
  return make2i32(v.x*s, v.y*s);
}

Vec2i32 div2i32(Vec2i32 v, int32_t s) {
  return make2i32(v.x/s, v.y/s);
}

int32_t dot2i32(Vec2i32 v1, Vec2i32 v2) {
  return v1.x*v2.x + v1.y*v2.y;
}

int32_t mag2i32(Vec2i32 v) {
  return sqrt(dot2i32(v, v));
}

Vec2i32 prj2i32(Vec2i32 v1, Vec2i32 v2) {
  return mul2i32(v1, dot2i32(v1, v2)/dot2i32(v1, v1));
}

int32_t cmp2i32(Vec2i32 v1, Vec2i32 v2) {
  return dot2i32(v1, v2)/mag2i32(v1);
}

int isequal2i32(Vec2i32 v1, Vec2i32 v2) {
  return v1.x == v2.x && v1.y == v2.y;
}

Vec2i32 tan2i32(uint8_t dir) {
  return make2i32(cos(dir*(kPif/8.0f))*INT32_MAX, sin(dir*(kPif/8.0f))*INT32_MAX);
}

Vec2i32 scale2i32(uint8_t dir, int32_t scale) {
  return div2i32(tan2i32(dir), INT32_MAX/scale);
}

Vec2i16 c2i32to2i16(Vec2i32 v) {
  return make2i16(v.x, v.y);
}

/*
 * returns a vector with x and y
 */

Vec2i16 make2i16(int16_t x, int16_t y) {
  Vec2i16 r;
  r.x = x;
  r.y = y;
  return r;
}

/*
 * r = -v
 */

Vec2i16 neg2i16(Vec2i16 v) {
  Vec2i16 r;
  r.x = -v.x;
  r.y = -v.y;
  return r;
}

/*
 * r = v1 + v2
 */

Vec2i16 add2i16(Vec2i16 v1, Vec2i16 v2) {
  Vec2i16 r;
  r.x = v1.x + v2.x;
  r.y = v1.y + v2.y;
  return r;
}

/*
 * r = v1 - v2
 */

Vec2i16 sub2i16(Vec2i16 v1, Vec2i16 v2) {
  Vec2i16 r;
  r.x = v1.x - v2.x;
  r.y = v1.y - v2.y;
  return r;
}

/*
 * r = v X s
 */

Vec2i16 mul2i16(Vec2i16 v, int16_t s) {
  Vec2i16 r;
//  r.x = ((int32_t)v.x)*((int32_t)s)/((int32_t)INT16RADIX);
//  r.y = ((int32_t)v.y)*((int32_t)s)/((int32_t)INT16RADIX);
  r.x = v.x*s;
  r.y = v.y*s;
  return r;
}

/*
 * r = v/s
 */

Vec2i16 div2i16(Vec2i16 v, int16_t s) {
  Vec2i16 r;
//  r.x = ((int32_t)v.x)*((int32_t)INT16RADIX)/((int32_t)s);
//  r.y = ((int32_t)v.y)*((int32_t)INT16RADIX)/((int32_t)s);
  r.x = v.x/s;
  r.y = v.y/s;
  return r;
}

/*
 * r = v1*v2
 */

int16_t dot2i16(Vec2i16 v1, Vec2i16 v2) {
  return v1.x*v2.x + v1.y*v2.y;
}

/*
 * r = |v|
 */

int16_t mag2i16(Vec2i16 v) {
  return sqrt(dot2i16(v, v));
}

/*
 * r = v1.proj(v2)
 */

Vec2i16 prj2i16(Vec2i16 v1, Vec2i16 v2) {
  return mul2i16(v1, dot2i16(v1, v2)/dot2i16(v1, v1));
}

/*
 * r = v1.comp(v2)
 */

int16_t cmp2i16(Vec2i16 v1, Vec2i16 v2) {
  return dot2i16(v1, v2)/mag2i16(v1);
}

/*
 * returns 0 if v1 != v2
 */

int isequal2i16(Vec2i16 v1, Vec2i16 v2) {
  return v1.x == v2.x && v1.y == v2.y;
}

Vec2i16 tan2i16(uint8_t dir) {
  return make2i16(cos(dir*(kPif/8.0f))*INT16_MAX, sin(dir*(kPif/8.0f))*INT16_MAX);
}

Vec2i16 scale2i16(uint8_t dir, int16_t scale) {
  return div2i16(tan2i16(dir), INT16_MAX/scale);
}

Vec2i8 c2i16to2i8(Vec2i16 v) {
  return make2i8(v.x, v.y);
}

/*
 * U8to2i16() converts dir to a vector.
 */

Vec2i8 make2i8(int8_t x, int8_t y) {
  Vec2i8 r;
  r.x = x;
  r.y = y;
  return r;
}

int isequal2i8(Vec2i8 v1, Vec2i8 v2) {
  return v1.x == v2.x && v1.y == v2.y;
}
