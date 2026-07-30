#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils.h>

char *json_text = "\
{ \n\
    \"type\": \"Feature\", \n\
    \"properties\": { \n\
        \"units\": \"miles\", \n\
        \"periods\": [ \n\
            { \n\
                \"number\": 0, \n\
                \"string\": \"string0\" \n\
            }, \n\
            { \n\
                \"number\": 1, \n\
                \"string\": \"string1\" \n\
            } \n\
        ] \n\
    } \n\
} \n\
{ \n\
    \"another\": \"yes\", \n\
} \
";

void print_json_value(json_value_t *x);

int main()
{
    void *json_root;
    char *end_ptr;

    // print the json_text
    printf("-------- JSON --------\n");
    printf("%s\n", json_text);
    printf("------ JSON END ------\n\n");

    // parse the json_text
    json_root = util_json_parse(json_text, &end_ptr);
    if (json_root == NULL) {
        printf("ERROR: failed to parse json\n");
        return 1;
    }
    if (strlen(end_ptr) > 0) {
        printf("end_ptr = ...");
        printf("%s\n\n", end_ptr);
    }

    // print fields, always provide NULL as the last arg to util_json_get_value
    print_json_value(util_json_get_value(json_root, "type", NULL));
    print_json_value(util_json_get_value(json_root, "properties", "units", NULL));

    // print fields from periods array [0]
    print_json_value(util_json_get_value(json_root, "properties", "periods", "0", "number", NULL));
    print_json_value(util_json_get_value(json_root, "properties", "periods", "0", "string", NULL));

    // print fields from periods array [1], illustrates a different approach than above
    json_value_t *jv = util_json_get_value(json_root, "properties", "periods", "1", NULL);
    void *period1 = jv->u.object;
    print_json_value(util_json_get_value(period1, "number", NULL));
    print_json_value(util_json_get_value(period1, "string", NULL));

    // cleanup
    util_json_free(json_root);
    return 0;
}

void print_json_value(json_value_t *jv)
{
    if (jv == NULL) {
        printf("ERROR: json_value is NULL\n");
        exit(1);
    }

    switch (jv->type) {
    case JSON_TYPE_FLAG:   printf("FLAG   %d\n", jv->u.flag);   break;
    case JSON_TYPE_NUMBER: printf("NUMBER %g\n", jv->u.number); break;
    case JSON_TYPE_STRING: printf("STRING %s\n", jv->u.string); break;
    case JSON_TYPE_ARRAY:  printf("ARRAY  %p\n", jv->u.array);  break;
    case JSON_TYPE_OBJECT: printf("OBJECT %p\n", jv->u.object); break;
    default:
        printf("ERROR: bad json_value type %d\n", jv->type);
        exit(1);
    }
}
