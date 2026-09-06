#include "../core/E_LIBFXNS.cpp"

int main(void)
{
    ElementArray arr;
    InitCollection(&arr, 8); // Start with initial capacity of 8

    printf("\n========================================================================\n");
    printf("   STARTING DYNAMIC CONTAINER TEST SUITE\n");
    printf("========================================================================\n");

 
    int    val_int = 42;
    float  val_float = 3.14159f;
    double val_double = 2.718281828;
    char   val_char = 'X';
    long   val_long = 9876543210L;
    short  val_short = 32000;

    PUSH_INT(&arr, val_int);
    PUSH_FLOAT(&arr, val_float);
    PUSH_DOUBLE(&arr, val_double);
    PUSH_CHAR(&arr, val_char);
    PUSH_LONG(&arr, val_long);
    PUSH_SHORT(&arr, val_short);


    int* ptr_int = &val_int;
    float* ptr_float = &val_float;
    char* ptr_char = &val_char;
    double* ptr_double = &val_double;
    long* ptr_long = &val_long;
    short* ptr_short = &val_short;
    void* ptr_void = (void*)&val_int;

    PUSH_INT_PTR(&arr, ptr_int);
    PUSH_FLOAT_PTR(&arr, ptr_float);
    PUSH_CHAR_PTR(&arr, ptr_char);
    PUSH_DOUBLE_PTR(&arr, ptr_double);
    PUSH_LONG_PTR(&arr, ptr_long);
    PUSH_SHORT_PTR(&arr, ptr_short);
    PUSH_VOID_PTR(&arr, ptr_void);

    int** dptr_int = &ptr_int;
    float** dptr_float = &ptr_float;
    char** dptr_char = &ptr_char;
    double** dptr_double = &ptr_double;
    long** dptr_long = &ptr_long;
    short** dptr_short = &ptr_short;
    void** dptr_void = &ptr_void;

    PUSH_INT_DPTR(&arr, dptr_int);
    PUSH_FLOAT_DPTR(&arr, dptr_float);
    PUSH_CHAR_DPTR(&arr, dptr_char);
    PUSH_DOUBLE_DPTR(&arr, dptr_double);
    PUSH_LONG_DPTR(&arr, dptr_long);
    PUSH_SHORT_DPTR(&arr, dptr_short);
    PUSH_VOID_DPTR(&arr, dptr_void);

    int*** tptr_int = &dptr_int;
    float*** tptr_float = &dptr_float;
    char*** tptr_char = &dptr_char;
    double*** tptr_double = &dptr_double;
    long*** tptr_long = &dptr_long;
    short*** tptr_short = &dptr_short;
    void*** tptr_void = &dptr_void;

    PUSH_INT_TPTR(&arr, tptr_int);
    PUSH_FLOAT_TPTR(&arr, tptr_float);
    PUSH_CHAR_TPTR(&arr, tptr_char);
    PUSH_DOUBLE_TPTR(&arr, tptr_double);
    PUSH_LONG_TPTR(&arr, tptr_long);
    PUSH_SHORT_TPTR(&arr, tptr_short);
    PUSH_VOID_TPTR(&arr, tptr_void);


    int* null_ptr = NULL;
    PUSH_INT_PTR(&arr, null_ptr);

    int x = 500;
    int* px = &x;
    int** dpx = &px;

    PUSH_LIST(&arr, 5,
        TYPE_INT, 42,
        TYPE_FLOAT, 3.14f,
        TYPE_INT_STAR, px,
        TYPE_INT_STAR_DOUBLE, dpx,
        TYPE_CHAR, 'A'
    );


    PrintCollection(&arr);

    DestroyCollection(&arr);
    printf("[Main] Collection destroyed and memory successfully reclaimed.\n\n");

    return 0;
}