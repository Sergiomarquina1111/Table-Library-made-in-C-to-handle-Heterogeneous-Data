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

typedef struct
{
	void* ptr;
	DATA_TYPE type;
}Element;

typedef struct
{
	Element* slots;
	int capacity;
	int count;
}ElementArray;



extern void InitCollection(ElementArray* obj, int total);
extern bool push_element(ElementArray* obj, Element cobj);
extern void DestroyCollection(ElementArray* obj);
extern bool push_int_impl(ElementArray* obj, int val);
extern bool push_float_impl(ElementArray* obj, float val);
extern bool push_double_impl(ElementArray* obj, double val);
extern bool push_long_impl(ElementArray* obj, long val);
extern bool push_short_impl(ElementArray* obj, short val);
extern bool push_char_impl(ElementArray* obj, char val);



#define PUSH_INT(obj_ptr, val)    push_int_impl((obj_ptr), (val))
#define PUSH_FLOAT(obj_ptr, val)  push_float_impl((obj_ptr), (val))
#define PUSH_DOUBLE(obj_ptr, val) push_double_impl((obj_ptr), (val))
#define PUSH_LONG(obj_ptr, val)   push_long_impl((obj_ptr), (val))
#define PUSH_SHORT(obj_ptr, val)  push_short_impl((obj_ptr), (val))
#define PUSH_CHAR(obj_ptr, val)   push_char_impl((obj_ptr), (val))

#endif