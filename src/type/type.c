/* Copyright 2021. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 * */

#define _GNU_SOURCE
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "type.h"


#define UNSIGNED	1
#define COMPLEX		2
#define CONST		4
#define VOLATILE	8
#define RESTRICT	16
#define ATOMIC		32
#define BITFIELD	64
#define WIDE		128

static void* xmalloc(size_t s)
{
	void* p = malloc(s);

	if (NULL == p)
		abort();

	return p;
}

static void xfree(const void* x)
{
	free((void*)x);
}

struct type_member {

	const char* name;

	union {
		type typ;
		int value;
	};
};

struct type {

	int refcount;
	enum type_kind kind;

	union {	
		struct { 

			const char* tag;
			int n;
			struct type_member* members;
			bool vna;
		};

		struct {

			int value;
		};

		type referenced;

		struct {

			unsigned int flags;
			type base;
			int bits;
		};

		struct {

			int length;
			type element;

			void* targ;	// value this type depends on
		};

		struct {

			type ret;
			type args;
		};
	};
};

static struct type* type_alloc(enum type_kind k)
{
	struct type* t = xmalloc(sizeof(struct type));
	t->kind = k;
	t->refcount = 1;
	return t;
}

type type_basic(enum type_kind kind)
{
	type t = type_alloc(kind);
//	assert(type_basic_p(t));
	return t;
}

type type_void(void)
{
	return type_alloc(TYPE_VOID);
}

type type_ref(type t)
{
	((struct type*)t)->refcount++;
	return t;
}

void type_free(type t)
{
	if (0 != --((struct type*)t)->refcount)
		return;

	switch (t->kind) {

	case TYPE_POINTER:

       		type_free(t->referenced);
		break;

	case TYPE_ARRAY:

		type_free(t->element);
		break;

	case TYPE_FUNCTION:

		type_free(t->ret);
		type_free(t->args);
		break;

	case TYPE_ARGLIST:
	case TYPE_STRUCT:
	case TYPE_UNION:
	case TYPE_ENUM:

		for (int i = 0; i < t->n; i++) {

			if (TYPE_ENUM != t->kind)
				type_free(t->members[i].typ);

			xfree(t->members[i].name);
		}

		xfree(t->tag);
		xfree(t->members);
		break;

	case TYPE_MODIFIED:

		type_free(t->base);
		break;

	default:
		break;
	}

	xfree(t);
}

type type_pointer(type t)
{
	struct type* n = type_alloc(TYPE_POINTER);
	n->referenced = t;
	return n;
}

type type_array(int N, type t)
{
	struct type* n = type_alloc(TYPE_ARRAY);
	n->length = N;
	n->element = t;
	n->targ = NULL;
	return n;
}

type type_incomplete_array(type t)
{
	struct type* n = type_alloc(TYPE_ARRAY);
	n->length = -1;
	n->element = t;
	n->targ = NULL;
	return n;
}

type type_variable_array(type t, void* targ)
{
	struct type* n = type_alloc(TYPE_ARRAY);
	n->length = -2;
	n->element = t;
	n->targ = targ;
	return n;
}

type type_arglist(int N, type args[N], const char* names[N])
{
	struct type* n = type_alloc(TYPE_ARGLIST);

	n->tag = NULL;
	n->members = xmalloc(N * sizeof(struct type_element));

	for (int i = 0; i < N; i++) {

		n->members[i].typ = args[i];
		n->members[i].name = names[i];
	}

	n->n = N;
	n->vna = false;

	return n;
}

type type_function2(type ret, int N, type args[N], const char* names[N])
{
	struct type* n = type_alloc(TYPE_FUNCTION);

	n->ret = ret;
	n->args = type_arglist(N, args, names);

	return n;
}

type type_function(type ret, int N, type args[N])
{
	const char* names[N];

	for (int i = 0; i < N; i++)
		names[i] = NULL;

	return type_function2(ret, N, args, names);
}

static struct type* type_compound(const char* tag, int N, struct type_element e[N])
{
	struct type* n = type_alloc(TYPE_VOID);

	n->n = N;
	n->tag = strdup(tag);
	n->members = NULL;

	if (NULL == e) { // incomplete

		assert(0 == N);
		return n;
	}
	
	n->members = xmalloc(N * sizeof(struct type_member));

	for (int i = 0; i < N; i++) {

		n->members[i].name = strdup(e[i].name);
		n->members[i].typ = e[i].typ;
	}

	return n;
}

type type_struct(const char* tag, int N, struct type_element e[N])
{
	struct type* n = type_compound(tag, N, e);
	n->kind = TYPE_STRUCT;
	return n;
}

type type_struct_inc(const char* tag)
{
	struct type* n = type_compound(tag, 0, NULL);
	n->kind = TYPE_STRUCT;
	return n;
}


type type_union(const char* tag, int N, struct type_element e[N])
{
	struct type* n = type_compound(tag, N, e);
	n->kind = TYPE_UNION;
	return n;
}

type type_union_inc(const char* tag)
{
	struct type* n = type_compound(tag, 0, NULL);
	n->kind = TYPE_UNION;
	return n;
}

type type_enum(const char* tag, int N, struct type_enum e[N])
{
	struct type* n = type_compound(tag, 0, NULL);

	n->n = N;
	n->kind = TYPE_ENUM;

	if (NULL == e) { // incomplete

		assert(0 == N);
		return n;
	}

	n->members = xmalloc(N * sizeof(struct type_member));

	for (int i = 0; i < N; i++) {

		n->members[i].name = strdup(e[i].name);
		n->members[i].value = e[i].value;
	}

	return n;
}


type type_enum_inc(const char* tag)
{
	struct type* n = type_compound(tag, 0, NULL);
	n->kind = TYPE_ENUM;
	return n;
}


type type_base(type t)
{
	return (TYPE_MODIFIED == t->kind) ? t->base : t;
}

static unsigned int type_flags(type t)
{
	return (TYPE_MODIFIED == t->kind) ? t->flags : 0;
}

static struct type* type_modify(type t, unsigned int flags)
{
	struct type* n = type_alloc(TYPE_MODIFIED);

	if (TYPE_MODIFIED == t->kind) {

		n->base = t->base;
		n->flags = t->flags | flags;

	} else {

		n->base = t;
		n->flags = flags;
	}

	return n;
}

type type_unsigned(type t)
{
	if (type_unsigned_p(t))
		return t;

	return type_modify(t, UNSIGNED);
}

type type_complex(type t)
{
	assert(type_float_p(t));
	return type_modify(t, COMPLEX);
}

type type_atomic(type t)
{
	return type_modify(t, ATOMIC);
}

type type_bitfield(type t, int bits)
{
	struct type* t2 = type_modify(t, BITFIELD);
	t2->bits = bits;
	return t2;
}

type type_unqualified(type t)
{
	int flags = type_flags(t);

	if (0 == flags)
		return t;

	flags &= ~(CONST|VOLATILE|RESTRICT|WIDE);

	if (0 == flags)
		return t->base;

	return type_modify(t->base, flags);
}

type type_const(type t)
{
	return type_modify(t, CONST);
}

type type_volatile(type t)
{
	return type_modify(t, VOLATILE);
}

type type_restrict(type t)
{
	return type_modify(t, RESTRICT);
}

type type_wide(type t)
{
	return type_modify(t, WIDE);
}

type type_real(type t)
{
	assert(type_float_p(t));

	if (TYPE_MODIFIED != t->kind) 
		return t;

	if (!(t->flags & COMPLEX))
		return t;

	assert(t->base->flags == (t->flags & ~COMPLEX));
#if 0
	struct type* n = type_alloc(TYPE_MODIFIED); // FIXME

	n->base = t->base;
	n->flags = t->flags & ~COMPLEX;

	return n;
#else
	return t->base;
#endif
}

bool type_float_p(type t)
{
	switch (type_classify(t)) {

	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LONGDOUBLE:
		return true;

	default:
		return false;
	}
}

bool type_real_p(type t)
{
	assert(type_arithmetic_p(t));
	return (type_flags(t) & ~COMPLEX);
}

bool type_complex_p(type t)
{
	// assert(type_arithmetic_p(t));
	return (type_flags(t) & COMPLEX);
} 

bool type_atomic_p(type t)
{
	return (type_flags(t) & ATOMIC);
} 
	
bool type_const_p(type t)
{
	return (type_flags(t) & CONST);
}

bool type_volatile_p(type t)
{
	return (type_flags(t) & VOLATILE);
}

bool type_restrict_p(type t)
{
	// assert(type_pointer_p(t));
	return (type_flags(t) & RESTRICT);
}

bool type_wide_p(type t)
{
	// assert(type_pointer_p(t));
	return (type_flags(t) & WIDE);
}

bool type_bitfield_p(type t)
{
	return (type_flags(t) & BITFIELD);
}

bool type_unsigned_p(type t)
{
	return (   (TYPE_BOOL == type_classify(t))
	 	|| (type_signed_p(type_base(t)) 
		    && (type_flags(t) & UNSIGNED)));
}

bool type_signed_p(type t)
{
	if (type_flags(t) & UNSIGNED)
		return false;

	switch (type_base(t)->kind) {

	case TYPE_SCHAR:
	case TYPE_SHORT:
	case TYPE_INT:
	case TYPE_LONG:
	case TYPE_LONGLONG:
		return true;

	default:
		return false;
	}
}

bool type_scalar_p(type t)
{
	return (type_pointer_p(t) || type_arithmetic_p(t));
}

bool type_aggregate_p(type t)
{
	return (type_array_p(t) || type_struct_p(t));
}

bool type_compound_p(type t)
{
	return (type_union_p(t) || type_struct_p(t));
}

bool type_array_vla_p(type t)
{
	if (!type_array_p(t))
		return false;

	return (-2 == type_base(t)->length);
}

bool type_struct_has_fam_p(type t)
{
	assert(type_struct_p(t));

	int N = type_member_count(t);

	type last = type_member_type(t, N - 1);

	if (type_array_p(last)
            && !type_complete_p(last))
		return true;

	return false;
}

bool type_known_const_size_p(type t)
{
	if (!type_complete_p(t))
		return false;

	if (type_compound_p(t)) {

		int N = type_member_count(t);

		for (int i = 0; i < N; i++)
			if (!type_known_const_size_p(type_member_type(t, i)))
				return false;
	}

	if (   type_array_p(t)
	    && (   type_array_vla_p(t)
	        || (!type_known_const_size_p(type_array_element(t)))))
		return false;

	return true;
}


static void walk(type t, bool (*fun)(type x))
{
	if (!fun(t))
		return;

	switch (type_classify(t)) {

	case TYPE_FUNCTION:

		walk(type_function_return(t), fun);

		//t = type_function_arguments(t);

		break;

	case TYPE_STRUCT:
	case TYPE_UNION:

		;
#if 0
		int N = type_member_count(t);

		for (int i = 0; i < N; i++)
			walk(type_member_type(t, i), fun);
#endif
		break;

	case TYPE_POINTER:

		walk(type_pointer_referenced(t), fun);

		break;

	case TYPE_ARRAY:

		walk(type_array_element(t), fun);

		break;

	default:
		;
	}
}


int type_dependencies(type t)
{
	int d = 0;

	bool fun(type t)
	{
		if (type_compound_p(t))
			return false;

		if (   type_array_p(t)
		    && type_array_vla_p(t))
			d += 1;

		return true;
	}

	walk(t, fun);

	return d;
}

void* type_get_dependency(type t, int n)
{
	int d = 0;
	void* ptr = NULL;

	bool fun(type t)
	{
		if (type_compound_p(t))
			return false;

		if (   type_array_p(t)
		    && type_array_vla_p(t)) {

			if (n == d++) {

				ptr = t->targ;
				return false;
			}
		}

		return true;
	}

	walk(t, fun);

	return ptr;
}

bool type_derived_decl_p(type t)
{
	return (type_pointer_p(t) || type_array_p(t) || type_function_p(t));
}

enum type_category type_category(type t)
{
	if (type_atomic_p(t))
		return TC_ATOMIC;

	switch (type_classify(t)) {

	case TYPE_STRUCT: return TC_STRUCT;
	case TYPE_UNION: return TC_UNION;
	case TYPE_POINTER: return TC_POINTER;
	case TYPE_ARRAY: return TC_ARRAY;
	case TYPE_FUNCTION: return TC_FUNCTION;

	default: break;
	}

	return TC_SELF;
}

bool type_has_category_p(enum type_category c, type t)
{
	return (c == type_category(t));
}

bool type_qualified_p(type t)
{
	return (type_const_p(t) || type_volatile_p(t) || type_restrict_p(t) || type_wide_p(t));
}

bool type_character_p(type t)
{
	return ((TYPE_CHAR == type_classify(t))
		|| (TYPE_SCHAR == type_classify(t)));
}

bool type_integer_p(type t)
{
	return (type_enum_p(t) || type_signed_p(t) || type_unsigned_p(t) || type_character_p(t));
}

bool type_basic_p(type t)
{
	return ((TYPE_CHAR == type_classify(t))
		|| type_signed_p(t) 
		|| type_unsigned_p(t)
		|| type_float_p(t));
}

bool type_arithmetic_p(type t)
{
	return type_integer_p(t) || type_float_p(t);;
}


int type_rank(type t)
{
	assert(type_integer_p(t));

	switch (type_classify(t)) {

	case TYPE_BOOL: return 1;
	case TYPE_CHAR: return 2;
	case TYPE_SCHAR: return 2;
	case TYPE_SHORT: return 3;
	case TYPE_ENUM:
	case TYPE_INT: return 4;
	case TYPE_LONG: return 5;
	case TYPE_LONGLONG: return 6;

	default: assert(0);
	}
}


type type_promote(type t)
{
	assert(!type_qualified_p(t));

	switch (type_classify(t)) {

	case TYPE_CHAR: 
	case TYPE_SCHAR:
	case TYPE_SHORT:
		return type_basic(TYPE_INT);

	case TYPE_ARRAY:
		return type_pointer(type_array_element(t));

	case TYPE_FUNCTION:
		return type_pointer(t);

	case TYPE_FLOAT:
		return type_basic(TYPE_DOUBLE);

	case TYPE_BOOL:
	case TYPE_LONG:
	case TYPE_LONGLONG:
	default:
		return t;
	}
}


bool type_complete_p(type t)
{
	if (type_basic_p(t))
		return true;

	switch (type_classify(t)) {

	case TYPE_VOID:
		return false;

	case TYPE_ARRAY:
		return !(-1 == type_base(t)->length);

	case TYPE_STRUCT:
	case TYPE_UNION:
		return (NULL != type_base(t)->members);

	default: break;
	};
	
	return true;
}


bool type_identical_p(type a, type b)
{
	if (a == b)
		return true;

	if (type_flags(a) != type_flags(b))
		return false;

	if (type_classify(a) != type_classify(b))
		return false;

	assert(type_category(a) == type_category(b));
	
	switch (type_category(a)) {

	case TC_POINTER:
		return type_identical_p(type_pointer_referenced(a), type_pointer_referenced(b));

	case TC_ARRAY:
		if (type_array_vla_p(a) != type_array_vla_p(b))
			return false;

		if (type_complete_p(a) != type_complete_p(b))
			return false;

		return (type_known_const_size_p(a)
			&& (type_array_length(a) == type_array_length(b))
			&& type_identical_p(type_array_element(a), type_array_element(b)));

	case TC_FUNCTION:
		return ((type_identical_p(type_function_return(a), type_function_return(b)))
			&& (type_identical_p(type_function_arguments(a), type_function_arguments(b))));

	case TC_STRUCT:
	case TC_UNION:
		return false;

	case TC_ATOMIC:
		return (type_identical_p(type_base(a), type_base(b)));

	case TC_SELF:

		if (type_arglist_p(a)) {

			if (type_member_count(a) != type_member_count(b))
				return false;
	
			int N = type_member_count(a);
		
			for (int i = 0; i < N; i++)
				if (!type_identical_p(type_member_type(a, i),
						      type_member_type(b, i)))
					return false;

			return true;
		}

		break;
	}

	return true;
}



struct pair {

	const type a;
	const type b;
	const struct pair* link;
};

static bool type_compatible_inner(type a, type b, const struct pair* v);

static bool struct_compatible_p(type a, type b, const struct pair* v)
{
	const struct pair v2 = { a, b, v };

	// pair seen before -> assume equivalence

	for (; NULL != v; v = v->link)
		if (   ((a == v->a) && (b == v->b))
		    || ((a == v->b) && (b == v->a)))
			return true;

	if (0 != strcmp(a->tag, b->tag))
		return false;

	if (   (!type_complete_p(a))	// FIXME: we should record a constraint
	    || (!type_complete_p(b)))
		return true;

	if (a->n != b->n)
		return false;

	for (int i = 0; i < a->n; i++)
		if (   (0 != strcmp(a->members[i].name,
				    b->members[i].name))
		    || !type_compatible_inner(a->members[i].typ,
					      b->members[i].typ, &v2))
			return false;

	return true;
}


static bool type_compatible_inner(type a, type b, const struct pair* v)
{
	if (type_identical_p(a, b))
		return true;

	// also takes care of qualifiers 6.7.2.4(10)
	if (type_flags(a) != type_flags(b))
		return false;

	if (   type_bitfield_p(a)
	    && (type_bitfield_bits(a) != type_bitfield_bits(b)))
		return false;

	if (type_classify(a) != type_classify(b))
		return false;

	assert(type_category(a) == type_category(b));


	// we only have to accept cases not covered by identical_p
	switch (type_category(a)) {

	case TC_ARRAY: // 6.7.6.2(6)

		if (!type_identical_p(type_array_element(a), type_array_element(b)))
			return false;

		if (type_known_const_size_p(a) && type_known_const_size_p(b))
			return (type_array_length(a) == type_array_length(b));

		return true;

	case TC_FUNCTION: { // 6.7.6.3(15)
	
		if (!type_compatible_p(type_unqualified(type_function_return(a)),
				       type_unqualified(type_function_return(b))))
			return false;

		type argsa = type_function_arguments(a);
		type argsb = type_function_arguments(b);

		if ((NULL == argsa) || (NULL == argsb))
			return true;

		if (argsa->n != argsb->n)
			return false;

		for (int i = 0; i < argsa->n; i++)
			if (!type_compatible_p(type_unqualified(type_member_type(argsa, i)),
					       type_unqualified(type_member_type(argsb, i))))
				return false;

		return true;
	}

	case TC_STRUCT:
		return struct_compatible_p(a, b, v);

	case TC_UNION:
		// NOTE: C makes them non-compatible depending on scope, translation unit
		return (0 == strcmp(type_compound_tag(a), type_compound_tag(b)));

	case TC_ATOMIC:
	case TC_POINTER:
	case TC_SELF:

		if (type_enum_p(a) || type_enum_p(b)) {

			if (!type_enum_p(a)) {

				type tmp = a;
				a = b;
				b = tmp;
			}

			if (type_compatible_p(b, type_basic(TYPE_INT))) // pick int to be the compatible type
				return true;
			
			return false;
		}
		break;
	}

	return false;	
}

bool type_compatible_p(type a, type b)
{
	return type_compatible_inner(a, b, NULL);
}

type type_composite(type a, type b)
{
	assert(type_compatible_p(a, b));

	if (type_identical_p(a, b))
		return type_ref(b);
	
	switch (type_category(a)) {

	case TC_ARRAY:	// 6.2.7(3)

		if (type_known_const_size_p(a) || type_known_const_size_p(b)) {

			int length = (type_known_const_size_p(a))
					? type_array_length(a)
					: type_array_length(b);

			return type_array(length, 
					  type_composite(type_array_element(a),
					                 type_array_element(b)));
		}

		assert(0); // not yet supported
		
	case TC_FUNCTION: {

		type argsa = type_function_arguments(a);
		type argsb = type_function_arguments(b);

		if (NULL == argsa)
			return type_ref(b);

		if (NULL == argsb)
			return type_ref(a);

		assert(argsa->n == argsb->n);
		int N = argsa->n;
		type cargs[N];

		for (int i = 0; i < N; i++)
			cargs[i] = type_composite(type_unqualified(type_member_type(argsa, i)),
						  type_unqualified(type_member_type(argsb, i)));

		return type_function(type_ref(type_function_return(a)), N, cargs);
	}

	case TC_POINTER:
	case TC_STRUCT:
	case TC_UNION:
	case TC_ATOMIC:
	case TC_SELF: break;
	}

	return NULL;
}



enum type_kind type_classify(type t)
{
	return type_base(t)->kind;
}

bool type_has_class_p(type t, enum type_kind k)
{
	return (k == type_classify(t));
}

bool type_function_p(type t)
{
	return type_has_class_p(t, TYPE_FUNCTION);
}

bool type_pointer_p(type t)
{
	return type_has_class_p(t, TYPE_POINTER);
}

bool type_struct_p(type t)
{
	return type_has_class_p(t, TYPE_STRUCT);
}

bool type_union_p(type t)
{
	return type_has_class_p(t, TYPE_UNION);
}

bool type_array_p(type t)
{
	return type_has_class_p(t, TYPE_ARRAY);
}
bool type_arglist_p(type t)
{
	return type_has_class_p(t, TYPE_ARGLIST);
}

bool type_enum_p(type t)
{
	return type_has_class_p(t, TYPE_ENUM);
}

type type_function_return(type t)
{
	assert(type_function_p(t));
	return type_base(t)->ret;
}

type type_pointer_referenced(type t)
{
	assert(type_pointer_p(t));
	return type_base(t)->referenced;
}

type type_array_element(type t)
{
	assert(type_array_p(t));
	return type_base(t)->element;
}

int type_array_length(type t)
{
	assert(type_array_p(t));
	int l = type_base(t)->length;
	assert(0 <= l);
	return l;
}

type type_function_arguments(type t)
{
	assert(type_function_p(t));
	return type_base(t)->args;
}

const char* type_compound_tag(type t)
{
	assert(type_compound_p(t) || type_enum_p(t));
	return type_base(t)->tag;
}

int type_member_count(type t)
{
	assert(type_compound_p(t) || type_enum_p(t) || type_arglist_p(t));
	return type_base(t)->n;
}

type type_member_type(type t, int n)
{
	assert(0 <= n);
	assert(type_compound_p(t) || type_arglist_p(t));
	return type_base(t)->members[n].typ;
}

const char* type_member_name(type t, int n)
{
	assert(0 <= n);
	assert(type_compound_p(t) || type_enum_p(t) || type_arglist_p(t));
	return type_base(t)->members[n].name;
}

int type_enum_value(type t, int n)
{
	assert(0 <= n);
	assert(type_enum_p(t));
	return type_base(t)->members[n].value;
}

int type_bitfield_bits(type t)
{
	assert(type_bitfield_p(t));
	return t->bits;
}

type type_int_promotion(type x)
{
	assert(type_integer_p(x));

	// those do not really exist as types
	assert(!type_bitfield_p(x));

	type INT = type_basic(TYPE_INT);

	// NOTE: condition is that all values
	// must be representable, we assume
	// this is true for smaller rank

	if (type_rank(x) < type_rank(INT)) // rank smaller int
		return INT;

	if (type_rank(x) == type_rank(INT))
		return type_signed_p(x) ? INT : type_unsigned(INT);

	return x;
}


// 6.3.1.8 Usual arithmetic conversions
// this determines the common real type

type type_usual_conversion(type a, type b)
{
	assert(type_arithmetic_p(a));
	assert(type_arithmetic_p(b));

	if (type_float_p(a) || type_float_p(b)) {

		enum type_kind T[3] = { TYPE_LONGDOUBLE, TYPE_DOUBLE, TYPE_FLOAT };

		for (int i = 0; i < 3; i++)
			if (   (T[i] == type_classify(a))
			    || (T[i] == type_classify(b)))
				return type_basic(T[i]);

	} else {

		a = type_int_promotion(a);
		b = type_int_promotion(b);

		if (type_identical_p(a, b))
			return a;

		if (type_signed_p(a) == type_signed_p(b))
			return (type_rank(a) >= type_rank(b)) ? a : b;

		if (type_unsigned_p(a) && (type_rank(a) >= type_rank(b)))
			return a;

		if (type_unsigned_p(b) && (type_rank(b) >= type_rank(a)))
			return b;

		// FIXME: we assume that an signed type with higher rank
		// can represent all values of an unsigned type with
		// lower rank. Bitfields?

		if (type_signed_p(a) && (type_rank(a) > type_rank(b)))
			return a;

		if (type_signed_p(b) && (type_rank(b) > type_rank(a)))
			return b;

		if (type_signed_p(a))
			return type_unsigned(a);

		if (type_signed_p(b))
			return type_unsigned(b);
	}

	assert(0);
}


static bool type_const_recurse_p(type t)
{
	if (type_const_p(t))
		return true;

	if (!type_compound_p(t))
		return false;

	assert(type_complete_p(t));

	int N = type_member_count(t);

	for (int i = 0; i < N; i++)
		if (type_const_recurse_p(type_member_type(t, i)))
			return true;

	return false;
}

// 6.3.2.1
// modifiable lvalue
bool type_modifiable_p(type t)
{
	return !(   type_array_p(t)
		 || (!type_complete_p(t))
	         || type_const_recurse_p(t));
}


