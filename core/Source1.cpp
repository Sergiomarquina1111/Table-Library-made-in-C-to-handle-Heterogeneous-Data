#include "include/E_DEFS.h"

int main(int argc, char* argv[])
{
	Element arr[6];

	int* i_ptr = (int*)malloc(sizeof(int));
	float* f_ptr = (float*)malloc(sizeof(float));
	double* d_ptr = (double*)malloc(sizeof(double));
	long* l_ptr = (long*)malloc(sizeof(long));
	short* s_ptr = (short*)malloc(sizeof(short));
	char* c_ptr = (char*)malloc(sizeof(char));

	*i_ptr = 10;
	*f_ptr = 25.5f;
	*d_ptr = 10.55687;
	*l_ptr = 104568L;
	*s_ptr = 3265;
	*c_ptr = 'A';

	arr[0] = Element{ i_ptr, TYPE_INT };
	arr[1] = Element{ f_ptr, TYPE_FLOAT };
	arr[2] = Element{ d_ptr, TYPE_DOUBLE };
	arr[3] = Element{ l_ptr, TYPE_LONG };
	arr[4] = Element{ s_ptr, TYPE_SHORT };
	arr[5] = Element{ c_ptr, TYPE_CHAR };

	for (int i = 0; i < 6; i++)
	{
		switch (arr[i].type)
		{
			case TYPE_INT:
				printf("Element 'int' is found in array which is %d \n", *((int*)(arr[i].ptr)));
				printf("Element is at %d index with address %p on heap\n\n", i, (int*)arr[i].ptr);
				break;
			case TYPE_FLOAT:
				printf("Element 'float' is found in array which is %f\n", *((float*)(arr[i].ptr)));
				printf("Element is at %d index with address %p on heap\n\n", i, (float*)arr[i].ptr);
				break;
			case TYPE_DOUBLE:
				printf("Element 'double' is found in array which is %lf\n", *((double*)(arr[i].ptr)));
				printf("Element is at %d index with address %p on heap\n\n", i, (double*)arr[i].ptr);
				break;
			case TYPE_LONG:
				printf("Element 'long' is found in array which is %ld\n", *((long*)(arr[i].ptr)));
				printf("Element is at %d index with address %p on heap\n\n", i, (long*)arr[i].ptr);
				break;
			case TYPE_CHAR:
				printf("Element 'char' is found in array which is %c\n", *((char*)(arr[i].ptr)));
				printf("Element is at %d index with address %p on heap\n\n", i, (char*)arr[i].ptr);
				break;
			case TYPE_SHORT:
				printf("Element 'short' is found in array which is %d\n", *((short*)(arr[i].ptr)));
				printf("Element is at %d index with address %p on heap\n\n", i, (short*)arr[i].ptr);
				break;
			default:
				printf("Element present of Unknow Identifier");
				printf("Element is at %d index with address %p on heap\n\n", i, arr[i].ptr);
				break;
		}
	}

	return(0);
}