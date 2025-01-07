#include <stdio.h>
#include <stdlib.h>

int main() {
    int* p = (int *)malloc(10 * sizeof(int));
    for (int i = 0; i < 10; ++i) {
        p[i] = i;
    }

    printf("Pointer p: %p\n", (void*)p);

    // int d;
    // scanf("%d", &d);
    getchar();

    for (int i = 0; i < 10; ++i) {
        printf("%d ", p[i]);
    }
    printf("\n");

    free(p);

    return 0;
}

