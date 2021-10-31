/* Copyright 2021. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 * */

#include <assert.h>
#include <string.h>
#include <limits.h>

#include "type.h"

#include "abi.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))




static size_t sizeof_union(type t)
{
	size_t max = 0;
	int N = type_member_count(t);

	for (int i = 0; i < N; i++)
		max = MAX(max, type_sizeof(type_member_type(t, i)));

	return max;
}


static size_t sizeof_struct(type t)
{
	int N = type_member_count(t);

	assert(0 < N);

	size_t off = type_offsetof_n(t, N - 1);

	if (!type_struct_has_fam_p(t))
		off += type_sizeof(type_member_type(t, N - 1));

	assert(0 < off);

	return off;
}

static size_t alignof_union(type t)
{
	size_t max = 0;
	int N = type_member_count(t);

	for (int i = 0; i < N; i++)
		max = MAX(max, type_alignof(type_member_type(t, i)));

	return max;
}


static size_t alignof_struct(type t)
{
	return alignof_union(t);
}


struct abi {

	struct {

		size_t size;
		size_t alignment;

	} table[TYPE_NR_KINDS];
};

#define TENTRY(x) { sizeof(x), _Alignof(x) }
struct abi abi_host = { {
	[TYPE_BOOL] = TENTRY(bool),
	[TYPE_CHAR] = TENTRY(char),
	[TYPE_SCHAR] = TENTRY(signed char),
	[TYPE_SHORT] = TENTRY(signed int),
	[TYPE_INT] = TENTRY(signed int),
	[TYPE_LONG] = TENTRY(long),
	[TYPE_LONGLONG] = TENTRY(long long),
	[TYPE_FLOAT] = TENTRY(float),
	[TYPE_DOUBLE] = TENTRY(double),
	[TYPE_POINTER] = TENTRY(void*),
	[TYPE_ENUM] = TENTRY(int),
} };

static struct abi* abi = &abi_host;

size_t type_sizeof(type t)
{
	assert(type_known_const_size_p(t));

	if (type_arithmetic_p(t) && type_complex_p(t))
		return 2 * type_sizeof(type_real(t));

	switch (type_category(t)) {

	case TC_UNION:
		return sizeof_union(t);

	case TC_STRUCT:
		return sizeof_struct(t);

	case TC_ARRAY:
		return type_array_length(t) * type_sizeof(type_array_element(t));

	case TC_FUNCTION:
		assert(0);
		break;

	case TC_POINTER:
		return (type_wide_p(type_pointer_referenced(t)) ? 2 : 1)
				* abi->table[TYPE_POINTER].size;
	
	case TC_ATOMIC:
		assert(0);	// FIXME: the horror
		break;

	case TC_SELF:
		return abi->table[type_classify(t)].size;
	}

	assert(0);
}

size_t type_alignof(type t)
{
	switch (type_category(t)) {

	case TC_UNION:
		return alignof_union(t);

	case TC_STRUCT:
		return alignof_struct(t);

	case TC_ARRAY:
		return type_alignof(type_array_element(t));

	case TC_FUNCTION:
		assert(0);

	case TC_POINTER:
		return abi->table[TYPE_POINTER].alignment;

	case TC_ATOMIC:
		assert(0);	// FIXME: the horror
		break;

	case TC_SELF:
		return abi->table[type_classify(t)].alignment;
	}

	assert(0);
}

size_t type_offsetof_n(type t, int n)
{
	assert(type_compound_p(t));

	int N = type_member_count(t);
	assert(n < N);

	if (type_union_p(t))
		return 0;

	size_t sum = 0;
	int bits = 0;

	for (int i = 0; i < N; i++) {

		type m = type_member_type(t, i);

		if (type_bitfield_p(m)) {

			int nbits = type_bitfield_bits(m);

			if ((0 != nbits) && (bits >= nbits)) {

				bits -= nbits;
				continue;
			}

			bits = type_sizeof(m) * CHAR_BIT;
		}

		size_t al = type_alignof(m);
		size_t pad = (al - sum % al) % al;

		assert((i > 0) || (0 == pad));

		sum += pad;

		if (i == n)
			return sum;

		sum += type_sizeof(m);
	}

	return sum;
}


size_t type_offsetof(type t, const char* name)
{
	assert(type_compound_p(t));

	if (type_union_p(t))
		return 0;

	int N = type_member_count(t);

	for (int i = 0; i < N; i++)
		if (0 == strcmp(type_member_name(t, i), name))
			return type_offsetof_n(t, i);

	assert(0);
}


size_t type_widthof(type t)
{
	assert(type_integer_p(t));

	if (TYPE_BOOL == type_classify(t))
		return 1;

	if (type_bitfield_p(t))
		return type_bitfield_bits(t);

	return type_sizeof(t) * CHAR_BIT;
}

