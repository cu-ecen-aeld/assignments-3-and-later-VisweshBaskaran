/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry * aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer * buffer,
  size_t char_offset, size_t * entry_offset_byte_rtn) {
  //Checking if the pointer buffer and the pointer entry_offset_byte_rtn are null pointers  
     if (!buffer || !entry_offset_byte_rtn) {
        return NULL;
    }
  size_t cumulative_offset = 0;
  int current_entry_index = buffer -> out_offs;
  int i = 0;

  while (i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
    struct aesd_buffer_entry * current_entry = & buffer -> entry[current_entry_index];

    // Check if the character offset falls within the current buffer entry's range.
    if (char_offset < cumulative_offset + current_entry -> size) {
      if (entry_offset_byte_rtn) {
        * entry_offset_byte_rtn = char_offset - cumulative_offset; // Calculate the byte offset within the entry.
      }
      return current_entry;
    }

    cumulative_offset += current_entry -> size;
    current_entry_index = (current_entry_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    i++;
  }

  return NULL;

}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 */
const char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer * buffer,
  const struct aesd_buffer_entry * add_entry) {
  //Checking if the pointer buffer and the pointer add_entry are null pointers    
  if (!buffer || !add_entry) {
    return NULL;
  }
  const char* buffptr_removed = NULL;
  if(buffer -> full) 
  {
  buffptr_removed = buffer->entry[buffer->out_offs].buffptr; //Pointer to be returned with the buffer that was removed. When adding to cb when its already full
  }
  // Adding the new entry to the buffer at in_offs
  buffer -> entry[buffer -> in_offs] = * add_entry;

  // Updating in_offs to point to the next location
  buffer -> in_offs = (buffer -> in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

  if (buffer -> full) {
    // Buffer is full, overwriting the oldest entry and advancing buffer->out_ffs to new location
    buffer -> out_offs = (buffer -> out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
  }

  // Checking if the buffer is full
  if (buffer -> in_offs == buffer -> out_offs) {
    buffer -> full = true;
  }
  
  return  buffptr_removed;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer * buffer) {
  memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
