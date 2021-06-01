#include "variables.h"
#include <hashtable/hashtable.h>

static struct hashtable *shell_variables = NULL;

char *get_variable(const char *name)
{
    if (shell_variables == NULL) {
        return NULL;
    }

    char *variable;
    shell_variables->d.get(&shell_variables->d, name, strlen(name), (void **) &variable);
    return variable;
}

void set_variable(const char *name, const char *value)
{
    if (shell_variables == NULL) {
        shell_variables = create_hashtable();
    }

    char *existing = get_variable(name);
    if (existing != NULL) {
        free(existing);
        existing = NULL;
        shell_variables->d.remove(&shell_variables->d, name, strlen(name));
    }

    // copy string to take ownership kinda complicated
    // 
    // as I did not find a way to get the key back from the hashmap,
    // we allocate the value and the key in one single malloc.
    // this way, it is enough to just free the value
    size_t valuelen = strlen(value);
    size_t namelen = strlen(name);

    char *value_and_name = malloc(valuelen + namelen + 2);

    char *newval = value_and_name;
    char *newname = value_and_name + valuelen + 1;

    strncpy(newval, value, valuelen + 1);
    strncpy(newname, name, namelen + 1);

    shell_variables->d.put_word(&shell_variables->d, newname, strlen(name), (uintptr_t) newval);
}