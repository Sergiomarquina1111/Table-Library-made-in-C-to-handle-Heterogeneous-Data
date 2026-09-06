#include "../core/E_LIBFXNS_ARRAY_BASED.cpp"

void ping() {
    printf("Ping executed!\n");
}

int main(void) {
    ElementArray arr;
    InitCollection(&arr, 5);

    // Push the function
    PUSH_FUNC(&arr, &ping);

    PrintCollection(&arr);

    // Pop the function
    Element popped = pop_element(&arr);

    // Cast it back to a function pointer and execute it!
    void (*func)() = (void (*)())popped.ptr;
    func(); // Output: Ping executed!

    PUSH_INT(&arr, 10);

    DestroyCollection(&arr);
    return 0;
}