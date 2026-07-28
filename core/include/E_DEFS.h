#ifndef E_DEFS_H
#define E_DEFS_H

typedef enum DATA_TYPE
{
	TYPE_EMPTY,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_DOUBLE,
	TYPE_CHAR,
	TYPE_LONG,
	TYPE_SHORT
}DATA_TYPE;

typedef enum POINTERS
{
	TYPE_NULL = 10,
	TYPE_VOID_STAR,
	TYPE_INT_STAR,
	TYPE_FLOAT_STAR,
	TYPE_CHAR_STAR,
	TYPE_DOUBLE_STAR,
	TYPE_LONG_STAR,
	TYPE_SHORT_STAR,
}POINTERS;

typedef enum DOUBLE_POINTERS
{
	TYPE_NULL_DOUBLE = 100,
	TYPE_VOID_STAR_DOUBLE,
	TYPE_INT_STAR_DOUBLE,
	TYPE_FLOAT_STAR_DOUBLE,
	TYPE_CHAR_STAR_DOUBLE,
	TYPE_DOUBLE_STAR_DOUBLE,
	TYPE_LONG_STAR_DOUBLE,
	TYPE_SHORT_STAR_DOUBLE,
}DOUBLE_POINTERS;

typedef enum TRIPLE_POINTERS
{
	TYPE_NULL_TRIPLE = 1000,
	TYPE_VOID_STAR_TRIPLE,
	TYPE_INT_STAR_TRIPLE,
	TYPE_FLOAT_STAR_TRIPLE,
	TYPE_CHAR_STAR_TRIPLE,
	TYPE_DOUBLE_STAR_TRIPLE,
	TYPE_LONG_STAR_TRIPLE,
	TYPE_SHORT_STAR_TRIPLE,
}TRIPLE_POINTERS;

typedef struct
{
	void* ptr;
   short type;
}Element;

typedef struct
{
	Element* slots;
	int capacity;
	int count;
}ElementArray;



extern void InitCollection(ElementArray* obj, int total);
extern bool push_element(ElementArray* obj, Element cobj);
extern Element pop_element(ElementArray* obj);
extern void DestroyCollection(ElementArray* obj);
extern bool push_int_impl(ElementArray* obj, int val);
extern bool push_float_impl(ElementArray* obj, float val);
extern bool push_double_impl(ElementArray* obj, double val);
extern bool push_long_impl(ElementArray* obj, long val);
extern bool push_short_impl(ElementArray* obj, short val);
extern bool push_char_impl(ElementArray* obj, char val);
extern bool push_ptr_impl(ElementArray* obj, void* val, short pointer_type);
extern bool push_dptr_impl(ElementArray* obj, void** val, short double_pointer_type);
extern bool push_tptr_impl(ElementArray* obj, void*** val, short triple_pointer_type);
extern void PrintCollection(const ElementArray* obj);
extern bool PUSH_LIST(ElementArray* obj, int count, ...);

#define PUSH_INT(obj_ptr, val)     push_int_impl((obj_ptr), (val))
#define PUSH_FLOAT(obj_ptr, val)   push_float_impl((obj_ptr), (val))
#define PUSH_DOUBLE(obj_ptr, val)  push_double_impl((obj_ptr), (val))
#define PUSH_LONG(obj_ptr, val)    push_long_impl((obj_ptr), (val))
#define PUSH_SHORT(obj_ptr, val)   push_short_impl((obj_ptr), (val))
#define PUSH_CHAR(obj_ptr, val)    push_char_impl((obj_ptr), (val))
#define PUSH_INT_PTR(obj, val)     push_ptr_impl((obj), (void*)(val), TYPE_INT_STAR)
#define PUSH_FLOAT_PTR(obj, val)   push_ptr_impl((obj), (void*)(val), TYPE_FLOAT_STAR)
#define PUSH_CHAR_PTR(obj, val)    push_ptr_impl((obj), (void*)(val), TYPE_CHAR_STAR)
#define PUSH_DOUBLE_PTR(obj, val)  push_ptr_impl((obj), (void*)(val), TYPE_DOUBLE_STAR)
#define PUSH_LONG_PTR(obj, val)    push_ptr_impl((obj), (void*)(val), TYPE_LONG_STAR)
#define PUSH_SHORT_PTR(obj, val)   push_ptr_impl((obj), (void*)(val), TYPE_SHORT_STAR)
#define PUSH_VOID_PTR(obj, val)    push_ptr_impl((obj), (void*)(val), TYPE_VOID_STAR)
#define PUSH_INT_DPTR(obj, val)    push_dptr_impl((obj), (void**)(val), TYPE_INT_STAR_DOUBLE)
#define PUSH_FLOAT_DPTR(obj, val)  push_dptr_impl((obj), (void**)(val), TYPE_FLOAT_STAR_DOUBLE)
#define PUSH_CHAR_DPTR(obj, val)   push_dptr_impl((obj), (void**)(val), TYPE_CHAR_STAR_DOUBLE)
#define PUSH_DOUBLE_DPTR(obj, val) push_dptr_impl((obj), (void**)(val), TYPE_DOUBLE_STAR_DOUBLE)
#define PUSH_LONG_DPTR(obj, val)   push_dptr_impl((obj), (void**)(val), TYPE_LONG_STAR_DOUBLE)
#define PUSH_SHORT_DPTR(obj, val)  push_dptr_impl((obj), (void**)(val), TYPE_SHORT_STAR_DOUBLE)
#define PUSH_VOID_DPTR(obj, val)   push_dptr_impl((obj), (void**)(val), TYPE_VOID_STAR_DOUBLE)
#define PUSH_INT_TPTR(obj, val)    push_tptr_impl((obj), (void***)(val), TYPE_INT_STAR_TRIPLE)
#define PUSH_FLOAT_TPTR(obj, val)  push_tptr_impl((obj), (void***)(val), TYPE_FLOAT_STAR_TRIPLE)
#define PUSH_CHAR_TPTR(obj, val)   push_tptr_impl((obj), (void***)(val), TYPE_CHAR_STAR_TRIPLE)
#define PUSH_DOUBLE_TPTR(obj, val) push_tptr_impl((obj), (void***)(val), TYPE_DOUBLE_STAR_TRIPLE)
#define PUSH_LONG_TPTR(obj, val)   push_tptr_impl((obj), (void***)(val), TYPE_LONG_STAR_TRIPLE)
#define PUSH_SHORT_TPTR(obj, val)  push_tptr_impl((obj), (void***)(val), TYPE_SHORT_STAR_TRIPLE)
#define PUSH_VOID_TPTR(obj, val)   push_tptr_impl((obj), (void***)(val), TYPE_VOID_STAR_TRIPLE)


#endif