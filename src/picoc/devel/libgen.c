#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

void replace(char *s, char *pattern1, char *pattern2);
char *picoc_type(char *type);
void remove_newline(char *s);
bool is_blank_line(char *s);

int main(int argc, char **argv)
{
    char  prototype[1000];
    char  prototype_orig[1000];
    char *token[200], *p;
    int   n, idx, i;

    setlinebuf(stdout);
    setlinebuf(stderr);

again:
    // get prototype from stdin
    if (fgets(prototype_orig, sizeof(prototype_orig), stdin) == NULL) {
        return 0;
    }
    remove_newline(prototype_orig);

    // ignore blank lines
    if (is_blank_line(prototype_orig)) {
        goto again;
    }

    //make some adjustments
    p = strstr(prototype_orig, "__attribute__");
    if (p) strcpy(p, ";");

    // make working copy of prototype_orig; which will be changed by strtok
    strcpy(prototype, prototype_orig);

    // handle comment lines
    if (strncmp(prototype, "//", 2) == 0) {
        printf("//\n");
        printf("%s\n", prototype);
        printf("//\n");
        printf("\n");

        fprintf(stderr, "\n");
        fprintf(stderr, "    %s\n", prototype);

        goto again;
    }
    
    // replace certain chars, with the wrapped by spaces
    replace(prototype, "*", " * ");
    replace(prototype, ",", " , ");
    replace(prototype, ";", " ; ");
    replace(prototype, "(", " ( ");
    replace(prototype, ")", " ) ");

    // tokenize
    n = 0;
    while (true) {
        char *x = strtok(n==0?prototype:NULL, " ");
        if (x == NULL) {
            break;
        }
        token[n++] = x;
    }

    // get routine type and name
    char routine_name[100];
    char routine_type[100];
    routine_name[0] = '\0';
    routine_type[0] = '\0';
    for (i = 0; i < n; i++) {
        if (strcmp(token[i+1], "(") == 0) {
            sprintf(routine_name, "%s", token[i]);
            break;
        } else {
            if (routine_type[0] != '\0') {
                strcat(routine_type, " ");
            }
            strcat(routine_type, token[i]);
        }
    }
    if (routine_type[0] == '\0') {
        printf("ERROR routine type not found\n");
        return 1;
    }
    if (routine_name[0] == '\0') {
        printf("ERROR routine name not found\n");
        return 1;
    }
    idx = i + 2;

    // get arg names and types
    char arg_type[100][100];
    char arg_name[100][100];
    int n_args;
    n_args = 0;
    for (i = 0; i < 100; i++) {
        arg_type[i][0] = '\0';
        arg_name[i][0] = '\0';
    }
    while (true) {
        for (i = idx; i < n; i++) {
            if (strcmp(token[i+1], ",") == 0 ||
                strcmp(token[i+1], ")") == 0)
            {
                sprintf(arg_name[n_args], "%s", token[i]);
                n_args++;
                break;
            } else {
                if (arg_type[n_args][0] != '\0') {
                    strcat(arg_type[n_args], " ");
                }
                strcat(arg_type[n_args], token[i]);
            }
        }
        if (strcmp(token[i+1], ")") == 0 || token[i+1][0] == '\0') {
            break;
        }
        idx = i + 2;
    }

    // SdlFunction generator ...

    // print prototype lines
    char new_routine_name[100];
    strcpy(new_routine_name, routine_name);
    new_routine_name[0] = toupper(new_routine_name[0]);
    printf("void %s(struct ParseState *Parser, struct Value *ReturnValue,\n", new_routine_name);
    printf("        struct Value **Param, int NumArgs)\n");
    printf("{\n");

    // print param declarations
    if (strcmp(arg_name[0], "void") != 0) {
        int max_variable_name=0, max_type_name=0, len;

        // - determine longest type and variable names
        for (i = 0; i < n_args; i++) {
            if (strcmp(arg_name[i], "...") == 0) {
                break;
            }
            len = strlen(arg_type[i]);
            if (len > max_type_name) max_type_name = len;
            len = strlen(arg_name[i]);
            if (len > max_variable_name) max_variable_name = len;
        }

        // - print the params
        for (i = 0; i < n_args; i++) {
            if (strcmp(arg_name[i], "...") == 0) {
                break;
            }
            printf("    %-*s %-*s = (%s)Param[%d]->Val->%s;\n",
                max_type_name, arg_type[i], max_variable_name,  arg_name[i], arg_type[i], i, picoc_type(arg_type[i]));
        }

        // - print blank line after args
        printf("\n");
    }

    // print call to real routine
    if (strcmp(routine_type, "void") != 0) {
        printf("    %s retval;\n", routine_type);
        printf("    retval = %s(", routine_name);
    } else {
        printf("    %s(", routine_name);
    }
    for (i = 0; i < n_args; i++) {
        printf("%s", arg_name[i]);
        if (i < n_args-1) printf(", ");
    }
    printf(");\n");

    // set return value
    if (strcmp(routine_type, "void") != 0) {
        printf("    ReturnValue->Val->%s = retval;\n", picoc_type(routine_type));
    }

    // print closing brace
    printf("}\n\n");

    // print SdlFunctions to stderr
    strcat(new_routine_name, ",");
    fprintf(stderr, "    { %-30s \"%s\" },\n", new_routine_name, prototype_orig);

    goto again;

    return 0;
}

void replace(char *s, char *pattern1, char *pattern2)
{
    char *found, work[1000];
    int   len_pattern1 = strlen(pattern1);
    int   len_pattern2 = strlen(pattern2);

    while (true) {
        found = strstr(s, pattern1);
        if (found == NULL) {
            return;
        }

        memcpy(work, s, found-s);
        work[found-s] = '\0';
        strcat(work, pattern2);
        strcat(work, found + len_pattern1);

        strcpy(s, work);
        s = found + len_pattern2;
    }
}

char *picoc_type(char *type)
{
    if (strstr(type, "*") != NULL) {
        return "Pointer";
    } else if (strcmp(type, "int") == 0) {
        return "Integer";
    } else if (strcmp(type, "unsigned int") == 0) {
        return "UnsignedInteger";
    } else if (strcmp(type, "long") == 0) {
        return "LongInteger";
    } else if (strcmp(type, "unsigned long") == 0) {
        return "UnsignedLongInteger";
    } else if (strcmp(type, "double") == 0) {
        return "FP";
    } else if (strcmp(type, "bool") == 0) {
        return "Integer";
    } else if (strcmp(type, "sdlx_color_t") == 0) {
        return "UnsignedInteger";
    } else {
        return "UNKNOWN";
    }
}

void remove_newline(char *s)
{
    int len=strlen(s);

    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

bool is_blank_line(char *s)
{
    for (; *s; s++) {
        if (!isspace(*s)) return false;
    }
    return true;
}
