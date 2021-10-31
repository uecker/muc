/* Copyright 2021. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 * */

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "type.h"
#include "nested.h"

#include "print.h"


#define MIN(a, b) ((a < b) ? a : b)


typedef int print_f(int n, char dst[static n], type t);

const char* basic_names[] =
{
	[TYPE_VOID]	= "void",
	[TYPE_BOOL]	= "bool",
	[TYPE_CHAR]	= "char",
	[TYPE_SCHAR]	= "signed char",
	[TYPE_SHORT]	= "short",
	[TYPE_INT]	= "int",	// signed int
	[TYPE_LONG]	= "long",
	[TYPE_LONGLONG] = "long long",
	[TYPE_FLOAT]	= "float",
	[TYPE_DOUBLE]	= "double",
	[TYPE_LONGDOUBLE] = "long double",
};


static int p_char(int n, char buf[static n], char c)
{
	if (n > 0)
		buf[0] = c;

	return 1;
}


static int p_number(int n, char dst[static n], int i)
{
	if (n < 0)
		n = 0;

	int r = snprintf(dst, n, "%d", i);
	assert(r > 0);

	return r;
}

static int p_name(int n, char dst[static n], const char* name)
{
	if (NULL == name)
		return 0;

	int len = strlen(name);

	if (len < n)
		n = len + 1;

	if (0 < n)
		strncpy(dst, name, n);

	return len;
}

static int p_basic(int n, char dst[static n], type t)
{
//	assert(type_basic_p(t));
//
	return p_name(n, dst, basic_names[type_classify(t)]);
}

typedef int CLOSURE_TYPE(p_inner_f)(int n, char dst[static n]);

static int p_type(int n, char dst[static n], type t, p_inner_f inner);

#define CALL(fun, n, l, dst, ...) (l += fun((n - l), *(char(*)[MIN(n, l)])((dst) + MIN(n, l)), ## __VA_ARGS__))

static int p_array(int n, char dst[static n], type t, p_inner_f inner)
{
	NESTED(int, p_array_inner, (int n, char dst[static n]))
	{
		int l = 0;

		if (NULL != inner)
			CALL(inner, n, l, dst);

		CALL(p_char, n, l, dst, '[');

		if (type_complete_p(t)) {

			if (type_array_vla_p(t)) {

				CALL(p_char, n, l, dst, '*');

			} else {

				CALL(p_number, n, l, dst, type_array_length(t));
			}
		}

		CALL(p_char, n, l, dst, ']');

		return l;
	};

	int l = 0;

	CALL(p_type, n, l, dst, type_array_element(t), p_array_inner);

	return l;
}

static int p_qualifiers(int n, char dst[static n], type t)
{
	int l = 0;

	if (type_const_p(t))
		CALL(p_name, n, l, dst, "const ");

	if (type_volatile_p(t))
		CALL(p_name, n, l, dst, "volatile ");

	if (type_restrict_p(t))
		CALL(p_name, n, l, dst, "restrict ");

	if (type_atomic_p(t))
		CALL(p_name, n, l, dst, "atomic ");

	if (type_wide_p(t))
		CALL(p_name, n, l, dst, "_Wide ");

	return l;
}

static int p_pointer(int n, char dst[static n], type t, p_inner_f inner)
{
	NESTED(int, p_pointer_inner, (int n, char dst[static n]))
	{
		int l = 0;
		dst[l++] = '('; //?
		CALL(p_char, n, l, dst, '*');
		
		CALL(p_qualifiers, n, l, dst, t);

		if (NULL != inner)
			CALL(inner, n, l, dst);

		dst[l++] = ')';	//?
		return l;
	};

	return p_type(n, dst, type_pointer_referenced(t), p_pointer_inner);
}

static int p_arglist(int n, char dst[static n], type t)
{
	int l = 0;

	CALL(p_char, n, l, dst, '(');

	for (int i = 0; i < type_member_count(t); i++) {

		type e = type_member_type(t, i);

		NESTED(int, p_arglist_inner, (int n, char dst[static n]))
		{
			return p_name(n, dst, type_member_name(t, i));
		};

		if (i > 0) {

			CALL(p_char, n, l, dst, ',');
			CALL(p_char, n, l, dst, ' ');
		}

		if (NULL == e) {

			CALL(p_name, n, l, dst, "...");
			break;
		}

		CALL(p_type, n, l, dst, e, p_arglist_inner);
	}

	CALL(p_char, n, l, dst, ')');

	return l;
}


static int p_compound(int n, char dst[static n], type t)
{
	int l = 0;

	CALL(p_char, n, l, dst, '{');
	CALL(p_char, n, l, dst, ' ');

	for (int i = 0; i < type_member_count(t); i++) {

		type e = type_member_type(t, i);

		NESTED(int, p_compound_inner, (int n, char dst[static n]))
		{
			int l = 0;

		//	CALL(p_char, n, l, dst, ' ');
			CALL(p_name, n, l, dst, type_member_name(t, i));

			return l;
		};

		CALL(p_type, n, l, dst, e, p_compound_inner);

		if (type_bitfield_p(e)) {

			CALL(p_char, n, l, dst, ':');
			CALL(p_number, n, l, dst, type_bitfield_bits(e));
		}

		CALL(p_char, n, l, dst, ';');
		CALL(p_char, n, l, dst, ' ');
	}

	CALL(p_char, n, l, dst , '}');

	return l;
}


static int p_struct(int n, char dst[static n], type t)
{
	int l = 0;

	CALL(p_name, n, l, dst, "struct");
	CALL(p_char, n, l, dst, ' ');
	CALL(p_name, n, l, dst, type_compound_tag(t));

	if (type_complete_p(t)) {

		CALL(p_char, n, l, dst, ' ');
		CALL(p_compound, n, l, dst, t);
	}

	return l;
}


static int p_union(int n, char dst[static n], type t)
{
	int l = 0;
	
	CALL(p_name, n, l, dst, "union");
	CALL(p_char, n, l, dst, ' ');
	CALL(p_name, n, l, dst, type_compound_tag(t));

	if (type_complete_p(t)) {

		CALL(p_char, n, l, dst, ' ');
		CALL(p_compound, n, l, dst, t);
	}

	return l;
}


static int p_enum(int n, char dst[static n], type t)
{
	int l = 0;

	CALL(p_name, n, l, dst, "enum");
	CALL(p_char, n, l, dst, ' ');
	CALL(p_name, n, l, dst, type_compound_tag(t));
	CALL(p_char, n, l, dst, ' ');

	if (!type_complete_p(t))
		return l;

	CALL(p_char, n, l, dst, '{');
	CALL(p_char, n, l, dst, ' ');

	for (int i = 0; i < type_member_count(t); i++) {

		CALL(p_name, n, l, dst, type_member_name(t, i));

		CALL(p_char, n, l, dst, ' ');
		CALL(p_char, n, l, dst, '=');
		CALL(p_char, n, l, dst, ' ');

		CALL(p_number, n, l, dst, type_enum_value(t, i));

		CALL(p_char, n, l, dst, ',');
		CALL(p_char, n, l, dst, ' ');
	}

	CALL(p_char, n, l, dst, '}');

	return l;
}


static int p_function(int n, char dst[static n], type t, p_inner_f inner)
{
	NESTED(int, p_function_inner, (int n, char dst[static n]))
	{
		int l = 0;

		CALL(p_char, n, l, dst, '(');
		CALL(p_qualifiers, n, l, dst, t);

		if (NULL != inner)
			CALL(inner, n, l, dst);

		CALL(p_char, n, l, dst, ')');
		CALL(type_print, n, l, dst, type_function_arguments(t));

		return l;
	};

	int l = 0;

	CALL(p_type, n, l, dst, type_function_return(t), p_function_inner);

	return l;
}


static int p_type(int n, char dst[static n], type t, p_inner_f inner)
{
	int l = 0;

	if (   (TYPE_POINTER != type_classify(t))
	    && (TYPE_FUNCTION != type_classify(t)))
		CALL(p_qualifiers, n, l, dst, t);

	switch (type_classify(t)) {

	case TYPE_STRUCT:
		CALL(p_struct, n, l, dst, t);
		break;

	case TYPE_UNION:
		CALL(p_union, n, l, dst, t);
		break;

	case TYPE_ARRAY:
		CALL(p_array, n, l, dst, t, inner);
		break;

	case TYPE_POINTER:
		CALL(p_pointer, n, l, dst, t, inner);
		break;

	case TYPE_FUNCTION:
		CALL(p_function, n, l, dst, t, inner);
		break;

	case TYPE_ARGLIST:
		CALL(p_arglist, n, l, dst, t);
		break;

	case TYPE_ENUM:
		CALL(p_enum, n, l, dst, t);
		break; 

	case TYPE_VOID:
	case TYPE_BOOL:
	case TYPE_CHAR:
 	case TYPE_SCHAR:
	case TYPE_SHORT:
	case TYPE_INT:
	case TYPE_LONG:
	case TYPE_LONGLONG:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LONGDOUBLE:

	default:

		if (   type_unsigned_p(t)
		    && (TYPE_BOOL != type_classify(t)))
			CALL(p_name, n, l, dst, "unsigned ");

		if (type_arithmetic_p(t) && type_complex_p(t))
			CALL(p_name, n, l, dst, "complex ");

		CALL(p_basic, n, l, dst, t);

		break;
	}

	switch (type_classify(t)) {

	case TYPE_FUNCTION: break;
	case TYPE_ARRAY: break;
	case TYPE_POINTER: break;

	default:
		
		if (NULL != inner) {

			CALL(p_char, n, l, dst, ' ');
			CALL(inner, n, l, dst);
		}
	}

	return l;
}


int type_print(int n, char dst[static n], type t)
{
	int l = 0;

	CALL(p_type, n, l, dst, t, NULL);
	CALL(p_char, n, l, dst, '\0');

	return l;
}


int type_decl_print(int n, char dst[static n], const char* id, type t)
{
	NESTED(int, inner, (int n, char dst[static n]))
	{
		int l = 0;

		CALL(p_name, n, l, dst, id);

		return l;
	};

	int l = 0;
		
	CALL(p_type, n, l, dst, t, inner);
	CALL(p_char, n, l, dst + l, '\0');

	return l;
}

