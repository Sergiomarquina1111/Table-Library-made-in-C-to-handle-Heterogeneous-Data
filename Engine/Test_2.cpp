#include"../core/E_LIBFXNS.cpp"

int main(void) 
{
    ElementArray myList;
    InitCollection(&myList, 5);

    PUSH_INT(&myList, 10);
    PUSH_FLOAT(&myList, 25.5f);
    PUSH_DOUBLE(&myList, 10.55687);
    PUSH_CHAR(&myList, 'A');

    DestroyCollection(&myList);
    return 0;
}