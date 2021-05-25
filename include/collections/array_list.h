#ifndef COLLECTIONS_ARRAY_LIST_H
#define COLLECTIONS_ARRAY_LIST_H

#include <stdint.h>
#include <aos/aos.h>

struct array_list
{
    size_t length;
    size_t capacity;
    size_t element_size;
    char *data;
};


void array_list_init(struct array_list *list, size_t element_size);
void array_list_reserve(struct array_list *list, size_t capacity);
void array_list_append(struct array_list *list, void *element);
void *array_list_at(struct array_list *list, size_t index);

void array_list_free(struct array_list *list);


#endif // COLLECTIONS_ARRAY_LIST_H
