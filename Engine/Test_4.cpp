#include "../core/E_LIBFXNS.cpp"

int main(void)
{
    ElementArray arr;
    InitCollection(&arr, 10);

    int x = 500;
    int* px = &x;
    int** dpx = &px;
    int*** tpx = &dpx;

    // YOUR EXACT SYNTAX WORKS PERFECTLY:
    PUSH_LIST(&arr, 5, TYPE_INT, 42, TYPE_FLOAT, 3.14f, TYPE_INT_STAR, px,TYPE_INT_STAR_DOUBLE, dpx, TYPE_INT_STAR_TRIPLE, tpx, TYPE_CHAR, 'A');
    PUSH_LIST(&arr, 5, TYPE_INT, 42, TYPE_FLOAT, 3.14f, TYPE_INT_STAR, px,TYPE_INT_STAR_DOUBLE, dpx, TYPE_INT_STAR_TRIPLE, tpx, TYPE_CHAR, 'A');
    PUSH_LIST(&arr, 5, TYPE_INT, 42, TYPE_FLOAT, 3.14f, TYPE_INT_STAR, px,TYPE_INT_STAR_DOUBLE, dpx, TYPE_INT_STAR_TRIPLE, tpx, TYPE_CHAR, 'A');

    PrintCollection(&arr);

    DestroyCollection(&arr);
    return 0;
}