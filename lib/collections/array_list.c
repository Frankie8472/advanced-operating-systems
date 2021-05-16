#include <collections/array_list.h>

#define ARRAY_LIST_DEFAULT_INIT_CAPACITY 8


void array_list_init(struct array_list *list, size_t element_size)
{
    list->element_size = element_size;
    list->data = malloc(ARRAY_LIST_DEFAULT_INIT_CAPACITY * element_size);
    list->capacity = ARRAY_LIST_DEFAULT_INIT_CAPACITY;
    list->length = 0;
}


void array_list_reserve(struct array_list *list, size_t capacity)
{
    list->data = realloc(list->data, capacity * list->element_size);
}


void array_list_append(struct array_list *list, void *element)
{
    if (list->length == list->capacity) {
        array_list_reserve(list, (list->capacity * 3 + 1) / 2);
    }
    memcpy(list->data + list->length * list->element_size, element, list->element_size);
    list->length++;

}


void *array_list_at(struct array_list *list, size_t index)
{
    assert(list);
    assert(index < list->length);

    return list->data + index * list->element_size;
}


void array_list_free(struct array_list *list)
{
    if (list->data != NULL) {
        free(list->data);
    }
    list->length = 0;
    list->capacity = 0;
}