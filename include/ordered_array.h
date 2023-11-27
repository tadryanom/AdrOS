/* 
 * Interface for creating, inserting and deleting
 * from ordered arrays.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __ORDERED_ARRAY_H
#define __ORDERED_ARRAY_H 1

#include <typedefs.h>

/*
 * This array is insertion sorted - it always remains in a sorted state (between calls).
 * It can store anything that can be cast to a void* -- so a u32int, or any pointer.
*/
typedef void* type_t;

/*
 * A predicate should return nonzero if the first argument is less than the second. Else 
 * it should return zero.
 */
typedef s8int (*lessthan_predicate_t) (type_t, type_t);

typedef struct
{
    type_t *array;
    u32int size;
    u32int max_size;
    lessthan_predicate_t less_than;
} ordered_array_t;

// A standard less than predicate.
s8int standard_lessthan_predicate (type_t, type_t);

// Create an ordered array.
ordered_array_t create_ordered_array (u32int, lessthan_predicate_t);
ordered_array_t place_ordered_array (void *, u32int, lessthan_predicate_t);

// Destroy an ordered array.
void destroy_ordered_array (ordered_array_t *);

// Add an item into the array.
void insert_ordered_array (type_t, ordered_array_t *);

// Lookup the item at index i.
type_t lookup_ordered_array (u32int, ordered_array_t *);

// Deletes the item at location i from the array.
void remove_ordered_array (u32int, ordered_array_t *);

#endif
