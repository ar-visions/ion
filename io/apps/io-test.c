#include <io/io.h>
#include <stdio.h>

typedef struct data_t {
    int  test_a;
    int  test_b;
    cstr lol;
} data;

void data_free(data *d) {
    free(d->lol);
}

int main(int argc, cstr argv[]) {
    data  *d  = io_new(data); /// <- unexpected type name 'data' expected expression
    d->lol    = calloc(1, 2); // allocate 2 bytes, zeroed
    d->lol[0] = 'a';
    
    io_sync(d);
    printf("io-test");
    io_unsync(d);

    io_drop(d);
    return 0;
}