

struct type;

extern size_t type_sizeof(const struct type* t);
extern size_t type_alignof(const struct type* t);
extern size_t type_offsetof(const struct type* t, const char* name);
extern size_t type_offsetof_n(const struct type* t, int n);
extern size_t type_widthof(const struct type* t);

