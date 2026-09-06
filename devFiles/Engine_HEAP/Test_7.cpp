#include "../core/E_LIBFXNS_ARRAY_BASED.cpp"

typedef union SensorData {
    int raw_temperature;
    float calculated_temp;
    char sensor_id[4];
} SensorData;

int main(void) {
    ElementArray arr;
    InitCollection(&arr, 5);

    SensorData data;
    data.calculated_temp = 98.6f;

    // Push the union
    PUSH_UNION(&arr, &data);

    // Retrieve the active member!
    float temp = GET_UNION_MEMBER(&arr.slots[0], float);
    printf("Extracted Union Temp: %.2f\n", temp);

    DestroyCollection(&arr);
    return 0;
}