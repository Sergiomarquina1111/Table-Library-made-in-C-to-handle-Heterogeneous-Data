#include "../core/E_LIBFXNS_ARRAY_BASED.cpp"

int main(void) {
    ElementArray arr;
    InitCollection(&arr, 5);

    // User opens a file
    FILE* log_file = fopen("log.txt", "w");
    if (log_file) {
        // Push it into the engine
        PUSH_FILE(&arr, log_file);
            
        // ... later in the program ...

        // Pop it back out
        Element popped = pop_element(&arr);
        FILE* retrieved_file = (FILE*)popped.ptr;

        // Write to it and close it (User manages the FILE lifecycle)
        fprintf(retrieved_file, "Engine execution successful!\n");
        fclose(retrieved_file);
    }

    DestroyCollection(&arr);
    return 0;
}