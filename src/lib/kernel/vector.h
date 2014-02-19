#ifndef LIB_KERNEL_VECTOR_H
#define LIB_KERNEL_VECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Implementation of a dynamic array (vector).
 *
 * This is useful for creating arbitrary length arrays. In this vector
 * implementation, we store data as an array of void * pointers. This
 * way, the vector can be used to store all types of data by type casting
 * the pointers to the appropriate type.
 *
 * Examples of using a struct vector v storing data with type char *:
 *
 * char *c;
 * for (i = 0; i < v.size; i++) {
 *   c = (char *) v.data[i];
 *   // Do something with c
 * }
 *
 * Vectors are good for O(1) access and amortized O(1) appends.
 */

struct vector
{
    // Array of void * pointers.
    void **data;

    // Number of elements currently in the vector.
    unsigned int size;

    // Maximum number of elements the vector can currently hold.
    unsigned int max_size;
};

void vector_init(struct vector *v);
void vector_copy(struct vector *v, struct vector *src);
void vector_resize(struct vector *v, unsigned int new_size);
void vector_insert(struct vector *v, unsigned int pos, void *value);
void vector_append(struct vector *v, void *value);
void vector_remove(struct vector *v, unsigned int pos);
void vector_destruct(struct vector *v);
void vector_zeros(struct vector *v, unsigned int num);

bool vector_empty(struct vector *v);

/*
 * Optional functions to get/set data. You can also directly access
 * the data if you're careful.
 */
void *vector_get(struct vector *v, unsigned int pos);
void vector_set(struct vector *v, unsigned int pos, void *value);


#endif /* lib/kernel/vector.h */
