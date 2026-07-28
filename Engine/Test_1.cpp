#include"../core/E_LIBFXNS.cpp"

int main(int argc, char* argv[])
{
    ElementArray myList;

    // Initialize collection with a starting capacity of 5 slots
    InitCollection(&myList, 5);

    // Allocate and store values on heap
    int* i_ptr1 = (int*)malloc(sizeof(int));
    int* i_ptr2 = (int*)malloc(sizeof(int));
    float* f_ptr = (float*)malloc(sizeof(float));
    double* d_ptr = (double*)malloc(sizeof(double));
    long* l_ptr = (long*)malloc(sizeof(long));
    short* s_ptr = (short*)malloc(sizeof(short));
    char* c_ptr = (char*)malloc(sizeof(char));

    *i_ptr1 = 10;
    *f_ptr = 25.5f;
    *d_ptr = 10.55687;
    *l_ptr = 104568L;
    *s_ptr = 3265;
    *i_ptr2 = 99;      // Will trigger auto-growth (Element #6)
    *c_ptr = 'A';     // Pushed into newly grown space (Element #7)

    // Push first 5 elements (filling initial capacity)
    push_element(&myList, Element{ i_ptr1, TYPE_INT });
    push_element(&myList, Element{ f_ptr,  TYPE_FLOAT });
    push_element(&myList, Element{ d_ptr,  TYPE_DOUBLE });
    push_element(&myList, Element{ l_ptr,  TYPE_LONG });
    push_element(&myList, Element{ s_ptr,  TYPE_SHORT });

    // Pushing these next 2 elements will trigger realloc (+5 growth)
    push_element(&myList, Element{ i_ptr2, TYPE_INT });
    push_element(&myList, Element{ c_ptr,  TYPE_CHAR });

    printf("\n--- Array Summary ---\n");
    printf("Total Active Count: %d\n", myList.count);
    printf("Total Heap Capacity: %d\n", myList.capacity);

    DestroyCollection(&myList);

    return 0;
}