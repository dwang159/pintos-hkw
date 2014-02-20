#include "vector.h"
#include "../debug.h"
#include "threads/malloc.h"

/* Default initial size of vector. */
#define INIT_SIZE 4

static void copy_elems(void **dst, void **src, unsigned int n_elems);

/*
 * Initializes the vector. We let the initial maximum size
 * be INIT_SIZE and the initial size be 0.
 */
void vector_init(struct vector *v)
{
    ASSERT(v);
    v->data = (void **)malloc(INIT_SIZE * sizeof(void *));
    ASSERT(v->data);
    v->size = 0;
    v->max_size = INIT_SIZE;
}

/*
 * Copies vector src into vector v.
 */
void vector_copy(struct vector *v, struct vector *src)
{
    ASSERT(v && src);
    if (v == src)
        return;
    v->size = 0;
    vector_resize(v, src->max_size);
    copy_elems(v->data, src->data, src->size);
    v->size = src->size;
}

/*
 * Manually change the maximum size of the vector. Is an error if the
 * current size of the vector is larger than the requested new size.
 *
 * This is helpful for quickly initializing large vectors.
 */
void vector_resize(struct vector *v, unsigned int new_size)
{
    void **new_data;

    ASSERT(v);
    ASSERT(new_size > 0 && v->size <= new_size);
    new_data = (void **)malloc(new_size * sizeof(void *));
    ASSERT(new_data);
    copy_elems(new_data, v->data, v->size);
    free(v->data);
    v->data = new_data;
    v->max_size = new_size;
}

/*
 * Insert an element at position pos. All elements after pos are shifted
 * back one index. The element that used to be at pos is now at pos + 1.
 */
void vector_insert(struct vector *v, unsigned int pos, void *value)
{
    unsigned int i;

    ASSERT(v);

    // Pos should be inside the existing elements. To append, the
    // vector_append() function should be used instead.
    ASSERT(pos < v->size);

    // If the vector is full, we need to resize it.
    if (v->size == v->max_size)
        vector_resize(v, v->max_size * 2);

    for (i = v->size - 1; i >= pos; i--)
        v->data[i + 1] = v->data[i];

    v->data[pos] = value;
    v->size++;
}

/*
 * Appends an element to the end of the vector.
 */
void vector_append(struct vector *v, void *value)
{
    ASSERT(v);
    if (v->size == v->max_size)
        vector_resize(v, v->max_size * 2);
    v->data[v->size] = value;
    v->size++;
}

/*
 * Removes the element at position pos from the vector.
 * All other elements after pos shift down one index.
 */
void vector_remove(struct vector *v, unsigned int pos)
{
    unsigned int i;

    ASSERT(v);
    for (i = pos; i < v->size; i++)
        v->data[i] = v->data[i + 1];
    v->size--;
}

/*
 * Cleans up the vector. This is a destructor for the vector.
 */
void vector_destruct(struct vector *v)
{
    ASSERT(v);
    free(v->data);
    v->size = v->max_size = 0;
}

/*
 * Clears vector and creates a new one with initial size (not max_size)
 * of num, filled with zeros.
 */
void vector_zeros(struct vector *v, unsigned int num)
{
    unsigned int i;

    ASSERT(v);
    v->size = 0;
    for (i = 0; i < num; i++)
        vector_append(v, 0);
}

/*
 * Checks if the vector is empty.
 */
bool vector_empty(struct vector *v)
{
    ASSERT(v);
    return v->size == 0;
}

/*
 * Gets the value of the element at position pos.
 */
void *vector_get(struct vector *v, unsigned int pos)
{
    ASSERT(v);
    ASSERT(pos < v->size);
    return v->data[pos];
}

/*
 * Sets the value at position pos.
 */
void vector_set(struct vector *v, unsigned int pos, void *value)
{
    ASSERT(v);
    ASSERT(pos < v->size);
    v->data[pos] = value;
}

/*
 * Copies n elements from src to dst.
 */
void copy_elems(void **dst, void **src, unsigned int n_elems)
{
    unsigned int i;

    ASSERT(src && dst);
    for (i = 0; i < n_elems; i++)
        dst[i] = src[i];
}
