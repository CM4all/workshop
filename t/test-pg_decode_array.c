#include "../src/strarray.h"
#include "../src/pg-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void check_decode(const char *input, const char *const* expected) {
    int ret;
    struct strarray a;
    unsigned i;

    strarray_init(&a);

    ret = pg_decode_array(input, &a);
    if (ret != 0) {
        fprintf(stderr, "decode '%s' failed\n", input);
        exit(2);
    }

    for (i = 0; i < a.num; ++i) {
        if (expected[i] == NULL) {
            fprintf(stderr, "decode '%s': too many elements in result ('%s')\n",
                    input, a.values[i]);
            exit(2);
        }

        if (strcmp(a.values[i], expected[i]) != 0) {
            fprintf(stderr, "decode '%s': element %u differs: '%s', but '%s' expected\n",
                    input, i, a.values[i], expected[i]);
            exit(2);
        }
    }

    if (expected[a.num] != NULL) {
        fprintf(stderr, "decode '%s': not enough elements in result ('%s')\n",
                input, expected[a.num]);
        exit(2);
    }

    strarray_free(&a);
}

int main(int argc, char **argv) {
    const char *zero[] = {NULL};
    const char *empty[] = {"", NULL};
    const char *one[] = {"foo", NULL};
    const char *two[] = {"foo", "bar", NULL};
    const char *three[] = {"foo", "", "bar", NULL};
    const char *special[] = {"foo", "\"\\", NULL};

    (void)argc;
    (void)argv;

    check_decode("{}", zero);
    check_decode("{\"\"}", empty);
    check_decode("{foo}", one);
    check_decode("{\"foo\"}", one);
    check_decode("{foo,bar}", two);
    check_decode("{foo,\"bar\"}", two);
    check_decode("{foo,,bar}", three);
    check_decode("{foo,\"\\\"\\\\\"}", special);

    return 0;
}
