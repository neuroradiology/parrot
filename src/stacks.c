/* stacks.c
 *  Copyright: (When this is determined...it will go here)
 *  CVS Info
 *     $Id$
 *  Overview:
 *     Stack handling routines for Parrot
 *  Data Structure and Algorithms:
 *     The stack is stored as a circular, doubly-linked list of
 *     chunks, where each chunk has room for STACK_CHUNK_DEPTH
 *     entries. The invariant maintained is that there is always room
 *     for another entry; if a chunk is filled, a new chunk is added
 *     onto the list before returning.
 *  History:
 *  Notes:
 * References: */

#include "parrot/parrot.h"

Stack_Chunk_t *
new_stack(Interp *interpreter)
{
#ifdef TIDY
    int i;
    Stack_Entry_t *entry;
#endif

    Stack_Chunk_t *stack = mem_allocate_aligned(sizeof(Stack_Chunk_t));
    
    stack->used = 0;
    stack->flags = NO_STACK_CHUNK_FLAGS;
    stack->next = stack;
    stack->prev = stack;
    stack->buffer = NULL;
    stack->buffer = new_buffer_header(interpreter);
    Parrot_allocate(interpreter, stack->buffer,
                    sizeof(Stack_Entry_t) * STACK_CHUNK_DEPTH);

#ifdef TIDY
    entry = (Stack_Entry_t *)stack->buffer->bufstart;
    for (i = 0; i < STACK_CHUNK_DEPTH; i++)
        entry[i].flags = NO_STACK_ENTRY_FLAGS;
#endif
    return stack;
}

void
stack_mark_cow(Stack_Chunk_t *stack_base) {
    Stack_Chunk_t * chunk = stack_base;
    chunk->flags |= STACK_CHUNK_COW_FLAG;
    for (chunk = chunk->next; chunk != stack_base; chunk = chunk->next)
        chunk->flags |= STACK_CHUNK_COW_FLAG;
}

/* Returns the height of the stack.  The maximum "depth" is height - 1 */
size_t
stack_height(Interp *interpreter, Stack_Chunk_t *stack_base)
{
    Stack_Chunk_t *chunk;
    size_t height = stack_base->used;

    for (chunk = stack_base->next; chunk != stack_base; chunk = chunk->next)
        height += chunk->used;

    return height;
}

/* Copy a stack (probably with COW flag) to a private writable stack */
Stack_Chunk_t *
stack_copy(struct Parrot_Interp * interp, Stack_Chunk_t *old_base) {
    Stack_Chunk_t *old_chunk = old_base; 
    Stack_Chunk_t *new_chunk;
    Stack_Chunk_t *new_base = NULL;
    do {
        new_chunk = mem_allocate_aligned(sizeof(Stack_Chunk_t));
        if(new_base == NULL) {
            new_base = new_chunk;
            new_base->next = new_base;
            new_base->prev = new_base;
        }
        else {
            new_chunk->next = new_base->next;
            new_chunk->prev = new_base;
            new_base->prev->next = new_chunk;
        }
        new_chunk->used = old_chunk->used;
        new_chunk->flags = old_chunk->flags & ~STACK_CHUNK_COW_FLAG;
        /* Copy stack buffer */
        new_chunk->buffer = new_buffer_header(interp);
        Parrot_allocate(interp, new_chunk->buffer,
                    sizeof(Stack_Entry_t) * STACK_CHUNK_DEPTH);
        memcpy(new_chunk->buffer->bufstart, old_chunk->buffer->bufstart,
                    old_chunk->buffer->buflen);
        old_chunk = old_chunk->next;
    }
    while(old_chunk != old_base);
    return new_base;
}

/* If depth >= 0, return the entry at that depth from the top of the stack,
   with 0 being the top entry.  If depth < 0, then return the entry |depth|
   entries from the bottom of the stack.
   Returns NULL if |depth| > number of entries in stack 
*/
Stack_Entry_t *
stack_entry(Interp *interpreter, Stack_Chunk_t *stack_base, Intval depth)
{
    Stack_Chunk_t *chunk;
    Stack_Entry_t *entry;
    size_t offset = (size_t)depth;
    
    /* For negative depths, look from the bottom of the stack up. */
    if (depth < 0) {
        chunk = stack_base;
        offset = (size_t)-depth;
        while (offset >= chunk->used && chunk != stack_base) {
            offset -= chunk->used;
            chunk  = chunk->next;
        }
        if (offset < chunk->used) {
            entry = (Stack_Entry_t *)chunk->buffer->bufstart + offset - 1;
        }
    }
    else {
        chunk = stack_base->prev;    /* Start at top */
        while (offset >= chunk->used && chunk != stack_base) {
            offset -= chunk->used;
            chunk  = chunk->prev;
        }
        if (offset < chunk->used) {
            entry = (Stack_Entry_t *)chunk->buffer->bufstart +
                                                    chunk->used - offset - 1;
        }
    }
    return entry;
}

/* Rotate the top N entries by one.  If N > 0, the rotation is bubble up,
   so the top most element becomes the Nth element.  If N < 0, the rotation
   is bubble down, so that the Nth element becomes the top most element.
*/
void
rotate_entries(Interp *interpreter, Stack_Chunk_t *stack_base, 
               Intval num_entries)
{
    Stack_Entry_t temp;
    Intval i;
    Intval depth = num_entries - 1;
    
    if (num_entries >= -1 && num_entries <= 1) {
        return;
    }

    if (num_entries < 0) {
        num_entries = -num_entries;
        depth = num_entries - 1;
            
        if (stack_height(interpreter, stack_base) < (size_t)num_entries) {
            internal_exception(ERROR_STACK_SHALLOW, "Stack too shallow!\n");
        }
        
        temp = *stack_entry(interpreter, stack_base, depth);
        for (i = depth; i > 0; i--) {
            *stack_entry(interpreter, stack_base, i) =
                *stack_entry(interpreter, stack_base, i - 1);
        }

        *stack_entry(interpreter, stack_base, 0) = temp;
    } 
    else {
        
        if (stack_height(interpreter, stack_base) < (size_t)num_entries) {
            internal_exception(ERROR_STACK_SHALLOW, "Stack too shallow!\n");
        }
        
        temp = *stack_entry(interpreter, stack_base, 0);
        for (i = 0; i < depth; i++) {
            *stack_entry(interpreter, stack_base, i) =
                *stack_entry(interpreter, stack_base, i + 1);
        }

        *stack_entry(interpreter, stack_base, depth) = temp;
    }
}

/* Push something on the generic stack.

   Note that the cleanup pointer, if non-NULL, points to a routine
   that'll be called when the entry is removed from the stack. This is
   handy for those cases where you need some sort of activity to take
   place when an entry is removed, such as when you push a lexical
   lock onto the call stack, or localize (or tempify, or whatever
   we're calling it) variable or something
 */
void
stack_push(Interp *interpreter, Stack_Chunk_t **stack_base_p,
           void *thing, Stack_entry_type type, Stack_cleanup_method cleanup)
{
    Stack_Chunk_t *stack_base = *stack_base_p;
    Stack_Chunk_t *chunk;
    Stack_Entry_t *entry;

    /* If stack is copy-on-write, copy it before we can execute on it */
    if (stack_base->flags & STACK_CHUNK_COW_FLAG) {
        *stack_base_p = stack_base = stack_copy(interpreter, stack_base);
    }

    chunk = stack_base->prev;

    /* Do we need a new chunk? */
    if (chunk->used == STACK_CHUNK_DEPTH) {
        /* Need to add a new chunk */
        Stack_Chunk_t *new_chunk = mem_allocate_aligned(sizeof(Stack_Chunk_t));

        new_chunk->used = 0;
        new_chunk->next = stack_base;
        new_chunk->prev = chunk;
        chunk->next = new_chunk;
        stack_base->prev = new_chunk;
        chunk = new_chunk;

        /* Need to initialize this pointer before the collector sees it */
        chunk->buffer = NULL;
        chunk->buffer = new_buffer_header(interpreter);

        Parrot_allocate(interpreter, chunk->buffer,
                        sizeof(Stack_Entry_t) * STACK_CHUNK_DEPTH);
    }

    entry = (Stack_Entry_t *)(chunk->buffer->bufstart) + chunk->used;

    /* Remember the type */
    entry->entry_type = type;
    /* If we were passed a cleanup function, mark the flag entry
     * for this as needing cleanup */
    entry->flags = (cleanup ? STACK_ENTRY_CLEANUP_FLAG 
                            : NO_STACK_ENTRY_FLAGS);
    /* Remember the cleanup function */
    entry->cleanup = cleanup;
    /* Store our thing */
    switch (type) {
        case STACK_ENTRY_INT:
            entry->entry.int_val = *(Intval *)thing;
            break;
        case STACK_ENTRY_FLOAT:
            entry->entry.num_val = *(Floatval *)thing;
            break;
        case STACK_ENTRY_PMC:
            entry->entry.pmc_val = (PMC *)thing;
            break;
        case STACK_ENTRY_STRING:
            entry->entry.string_val = (String *)thing;
            break;
        case STACK_ENTRY_POINTER:
            entry->entry.generic_pointer = thing;
            break;
        case STACK_ENTRY_DESTINATION:
            entry->entry.generic_pointer = thing;
            break;
        default:
            internal_exception(ERROR_BAD_STACK_TYPE, 
                               "Invalid Stack_Entry_type!\n");
            break;
    }

    chunk->used++;
}

/* Pop off an entry and return a pointer to the contents */
void *
stack_pop(Interp *interpreter, Stack_Chunk_t **stack_base_p,
          void *where, Stack_entry_type type)
{
    Stack_Chunk_t *stack_base = *stack_base_p;
    Stack_Entry_t *entry;
    Stack_Chunk_t *chunk;

    /* If stack is copy-on-write, copy it before we can execute on it */
    if (stack_base->flags & STACK_CHUNK_COW_FLAG) {
        *stack_base_p = stack_base = stack_copy(interpreter, stack_base);
    }

    chunk = stack_base->prev;

    /* We may have an empty chunk at the end of the list */
    if (chunk->used == 0 && chunk != stack_base) {
        /* That chunk != stack check is just to allow the empty stack case
         * to fall through to the following exception throwing code. */

        /* Need to pop off the last entry */
        stack_base->prev = chunk->prev;
        stack_base->prev->next = stack_base;
        /* Relying on GC feels dirty... */
        chunk = stack_base->prev;
    }

    /* Quick sanity check */
    if (chunk->used == 0) {
        internal_exception(ERROR_STACK_EMPTY, "No entries on stack!\n");
    }

    /* Now decrement the SP */
    chunk->used--;

    entry = (Stack_Entry_t *)(chunk->buffer->bufstart) + chunk->used;

    /* Types of 0 mean we don't care */
    if (type && entry->entry_type != type) {
        internal_exception(ERROR_BAD_STACK_TYPE,
                           "Wrong type on top of stack!\n");
    }

    /* Cleanup routine? */
    if (entry->flags & STACK_ENTRY_CLEANUP_FLAG) {
        (*entry->cleanup)(entry);
    }

    /* Sometimes the caller doesn't care what the value was */
    if (where == NULL) {
        return NULL;
    }

    /* Snag the value */
    switch (type) {
        case STACK_ENTRY_INT:
            *(Intval *)where = entry->entry.int_val;
            break;
        case STACK_ENTRY_FLOAT:
            *(Floatval *)where = entry->entry.num_val;
            break;
        case STACK_ENTRY_PMC:
            *(PMC **)where = entry->entry.pmc_val;
            break;
        case STACK_ENTRY_STRING:
            *(String **)where = entry->entry.string_val;
            break;
        case STACK_ENTRY_POINTER:
            *(void **)where = entry->entry.generic_pointer;
            break;
        case STACK_ENTRY_DESTINATION:
            *(void **)where = entry->entry.generic_pointer;
            break;
        default:
        internal_exception(ERROR_BAD_STACK_TYPE,
                           "Wrong type on top of stack!\n");
            break;                
    }

    return where;
}

/* Pop off a destination entry and return a pointer to the contents*/
void *
pop_dest(Interp *interpreter)
{
    /* We don't mind the extra call, so we do this: (previous comment
     * said we *do* mind, but I say let the compiler decide) */
    void *dest;
    (void)stack_pop(interpreter, &interpreter->control_stack,
                    &dest, STACK_ENTRY_DESTINATION);
    return dest;
}

void *
stack_peek(Interp *interpreter, Stack_Chunk_t *stack_base, 
           Stack_entry_type *type)
{
    Stack_Entry_t *entry = stack_entry(interpreter, stack_base, 0);
    if (entry == NULL) {
        return NULL;
    }
    
    if (type != NULL) {
        *type = entry->entry_type;
    }
    
    return entry->entry.generic_pointer;
}

Stack_entry_type
get_entry_type(Interp *interpreter, Stack_Entry_t *entry)
{
    return entry->entry_type;
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil 
 * End:
 *
 * vim: expandtab shiftwidth=4:
*/
