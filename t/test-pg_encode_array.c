#include "../src/strarray.h"
#include "../src/pg-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void check_encode(const struct strarray *input,
                         const char *expected) {
    char *result;

    result = pg_encode_array(input);

    if (result == NULL) {
        if (expected != NULL) {
            fprintf(stderr, "got NULL, expected '%s'\n",
                    expected);
            exit(2);
        }
    } else {
        if (expected == NULL) {
            fprintf(stderr, "got '%s', expected NULL\n",
                    result);
            exit(2);
        } else if (strcmp(result, expected) != 0) {
            fprintf(stderr, "got '%s', expected '%s'\n",
                    result, expected);
            exit(2);
        }

        free(result);
    }
}

int main(int argc, char **argv) {
    struct strarray a;

    (void)argc;
    (void)argv;

    check_encode(NULL, NULL);

    strarray_init(&a);
    check_encode(&a, "{}");

    strarray_append(&a, "foo");
    check_encode(&a, "{\"foo\"}");

    strarray_append(&a, "");
    check_encode(&a, "{\"foo\",\"\"}");

    strarray_append(&a, "\\");
    check_encode(&a, "{\"foo\",\"\",\"\\\\\"}");

    strarray_append(&a, "\"");
    check_encode(&a, "{\"foo\",\"\",\"\\\\\",\"\\\"\"}");

    strarray_free(&a);

    return 0;
}
