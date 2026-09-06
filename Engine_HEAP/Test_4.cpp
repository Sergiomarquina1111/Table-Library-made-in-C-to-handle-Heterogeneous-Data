#include "../core/E_LIBFXNS_ARRAY_BASED.cpp"

typedef struct Node
{
    int val;
    struct Node* next;
}Node;

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

    Node obj;
    obj.val = 10000;

    PUSH_STRUCT(&arr, &obj);
    int node_val = GET_STRUCT_MEMBER(&arr.slots[15], offsetof(Node, val), int);

    printf("%d", node_val);

    PrintCollection(&arr);

    DestroyCollection(&arr);
    return 0;
}