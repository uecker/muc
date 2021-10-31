
#include <stdbool.h>
#include <stdlib.h>

struct type;
typedef const struct type* type;

enum type_kind { TYPE_VOID, TYPE_UNION, TYPE_STRUCT, TYPE_ARRAY, TYPE_POINTER,
		TYPE_FUNCTION, TYPE_BOOL, TYPE_CHAR, TYPE_ENUM, TYPE_ARGLIST,
		TYPE_SCHAR, TYPE_SHORT, TYPE_INT, TYPE_LONG, TYPE_LONGLONG,
		TYPE_FLOAT, TYPE_DOUBLE, TYPE_LONGDOUBLE,
		TYPE_MODIFIED, TYPE_NR_KINDS };

enum type_category { TC_ARRAY, TC_POINTER, TC_FUNCTION, TC_UNION, TC_STRUCT, TC_ATOMIC, TC_SELF };

extern type type_basic(enum type_kind kind);
extern type type_void(void);

extern void type_free(type x);
extern type type_ref(type x);

struct type_element {

	const char* name;
	type typ;
};

struct type_enum {

	const char* name;
	int value;
};


// variations
extern type type_signed(type t);
extern type type_unsigned(type t);
extern type type_complex(type t);
extern type type_real(type t);

// type derivation
extern type type_pointer(type t);
extern type type_function(type ret, int N, type args[static N]);
extern type type_function2(type ret, int N, type args[static N], const char* argnames[static N]);
extern type type_struct(const char* tag, int N, struct type_element e[static N]);
extern type type_struct_inc(const char* tag);
extern type type_union(const char* tag, int N, struct type_element e[static N]);
extern type type_union_inc(const char* tag);
extern type type_array(int N, type t);
extern type type_incomplete_array(type t);
extern type type_variable_array(type t, void* targ);
extern type type_atomic(type t);
extern type type_enum(const char* tag, int N, struct type_enum list[static N]);
extern type type_enum_inc(const char* tag);

// qualifier
extern type type_unqualified(type t);
extern type type_const(type t);
extern type type_volatile(type t);
extern type type_restrict(type t);
extern type type_wide(type t);

extern type type_bitfield(type t, int bits);


// inspection
extern type type_base(type t);
extern enum type_kind type_classify(type t);
extern bool type_has_class_p(type t, enum type_kind k);
extern bool type_function_p(type t);
extern bool type_pointer_p(type t);
extern bool type_struct_p(type t);
extern bool type_union_p(type t);
extern bool type_array_p(type t);
extern bool type_enum_p(type t);
extern bool type_arglist_p(type t);
extern int type_dependencies(type t);
extern void* type_get_dependency(type t, int n);
extern int type_rank(type t);

// pointer
extern type type_pointer_referenced(type t);

// array
extern type type_array_element(type t);
extern int type_array_length(type t);

// functions
extern type type_function_return(type t);
extern type type_function_arguments(type t);

// struct or unions or arglists
extern int type_member_count(type t);
extern type type_member_type(type t, int n);
extern const char* type_member_name(type t, int n);
extern const char* type_compound_tag(type t);
extern bool type_struct_has_fam_p(type t);

extern int type_enum_value(type t, int n);
extern int type_bitfield_bits(type t);

extern type type_composite(type a, type b);

extern bool type_bitfield_p(type t);

extern bool type_complex_p(type t);
extern bool type_real_p(type t);
extern bool type_float_p(type t);
extern bool type_unsigned_p(type t);
extern bool type_signed_p(type t);
extern bool type_integer_p(type t);
extern bool type_basic_p(type t);
extern bool type_atomic_p(type t);
extern bool type_arithmetic_p(type t);

extern bool type_const_p(type t);
extern bool type_volatile_p(type t);
extern bool type_restrict_p(type t);
extern bool type_wide_p(type t);
extern bool type_complete_p(type a);

extern bool type_unqualified_p(type t);
extern bool type_character_p(type t);
extern bool type_scalar_p(type t);
extern bool type_aggregate_p(type t);
extern bool type_array_vla_p(type t);
extern bool type_known_const_size_p(type t);
extern bool type_derived_decl_p(type t);
extern bool type_compound_p(type t); // this is not std terminology

extern bool type_has_category_p(enum type_category, type t);
extern enum type_category type_category(type t);

extern bool type_compatible_p(type a, type b);
extern bool type_identical_p(type a, type b);
extern bool type_variably_modified_p(type a);

extern type type_usual_conversion(type a, type b);
extern type type_int_promotion(type x);

extern bool type_modifiable_p(type x);


