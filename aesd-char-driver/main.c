/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes, Visweshwaran Baskaran
 * @date 2023-11-07
 * @copyright Copyright (c) 2023
 *
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;
char * final_buffptr = NULL;

MODULE_AUTHOR("Visweshwaran Baskaran");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

/**
 * This function opens the AESD character device. It initializes
 * the private data for the file structure and associates it with the AESD device
 * @inode: Pointer to the inode structure
 * @filp: Pointer to the file structure
 * Returns:
 *   0 on success
 *
 * Based on scull_open() in scull/main.c
 */
int aesd_open(struct inode * inode, struct file * filp) {
  PDEBUG("open");
  //Based on scull_open() in scull/main.c   
  struct aesd_dev * dev;
  dev = container_of(inode -> i_cdev, struct aesd_dev, cdev);
  filp -> private_data = dev;
  return 0;
}

/**
 * This function is called when the last file descriptor referring to the
 * device is closed.
 * @param inode  Pointer to the inode structure for the device file.
 * @param filp   Pointer to the file structure for the device file.
 * @return       Returns 0 on success.
 */
int aesd_release(struct inode * inode, struct file * filp) {
  PDEBUG("release");
  // Nothing to add based on scull/main.c
  return 0;
}

/**
 * aesd_read - Read data from the device
 *
 * This function is responsible for reading data from the device. It reads
 * data from the circular buffer and copies it to the user-provided buffer.
 *
 * @param filp     Pointer to the file structure for the device file.
 * @param buf      User-space buffer where the data will be copied.
 * @param count    The maximum number of bytes to read.
 * @param f_pos    Pointer to the file position offset.
 * @return         Returns the number of bytes read on success, or a negative
 *                 error code on failure.
 */
ssize_t aesd_read(struct file * filp, char __user * buf, size_t count,
  loff_t * f_pos) {
  PDEBUG("In aesd_read\n");
  ssize_t retval = 0;
  PDEBUG("read %zu bytes with offset %lld", count, * f_pos);
  ssize_t entry_offset = 0;
  if (!filp) {
    PDEBUG("filp does not exist\n\r");
    return -EFAULT;
  }

  mutex_lock_interruptible( & aesd_device.lock);
  struct aesd_buffer_entry * read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(aesd_device.buffer, (size_t) * f_pos, & entry_offset);

  if (read_entry == NULL) //checking if the buffer entry is found
  {
    PDEBUG("read_entry not found\n\r");
    mutex_unlock( & (aesd_device.lock));
    return retval;
  }

  ssize_t bytes_to_read = read_entry -> size - entry_offset;
  if (bytes_to_read < count)
  ;
  else
    bytes_to_read = count;

  PDEBUG("buf copy: %s\n", read_entry -> buffptr + entry_offset);
  if (copy_to_user(buf, read_entry -> buffptr + entry_offset, bytes_to_read) != 0) {
    printk(KERN_ALERT "copy_to_user failed\n");
    return -EFAULT;
  } else {
    PDEBUG("copy_to_user passed\r\n");
  }

  * f_pos += bytes_to_read;
  retval = bytes_to_read;

  mutex_unlock( & aesd_device.lock);
  return retval;
}

/**
 * aesd_write - Write data to the device
 *
 * This function is responsible for writing data to the device. It handles the
 * buffering of data and copying it into the circular buffer for later reading.
 *
 * @param filp     Pointer to the file structure for the device file.
 * @param buf      User-space buffer containing the data to be written.
 * @param count    The number of bytes to write.
 * @param f_pos    Pointer to the file position offset.
 * @return         Returns the number of bytes written on success, or a negative
 *                 error code on failure.
 *@reference: Code leveraged from Ashwin Ravindra's implementation of aesd_write
*/
ssize_t aesd_write(struct file * filp,
  const char __user * buf, size_t count,
    loff_t * f_pos) {
  PDEBUG("In aesd_write\n");
  ssize_t retval = -ENOMEM;
  PDEBUG("write %zu bytes with offset %lld", count, * f_pos);
  static size_t final_count = 0;
  char * temp_buffptr = kmalloc(count, GFP_KERNEL);
  if (!temp_buffptr) {
    PDEBUG("temp_buffptr does not exist\n");
    return retval;
  }
  //static char *final_buffptr = NULL;
  if (!final_buffptr) {
    PDEBUG("final_buffptr does not exist\n");
    kfree(temp_buffptr);
    return retval;
  }
  if (copy_from_user(temp_buffptr, buf, count) != 0) {
    PDEBUG("copy_from_user failed\n");
    kfree(temp_buffptr);
    return retval;
  }
  if (strchr(temp_buffptr, '\n') == NULL) {
    final_buffptr = krealloc(final_buffptr, final_count + count, GFP_KERNEL); //realloc based on new size
    memcpy(final_buffptr + final_count, temp_buffptr, count);
    PDEBUG("final_buffptr: %s", final_buffptr);
    final_count += count;
    retval = count;
    kfree(temp_buffptr); //this is not needed anymore
    temp_buffptr = NULL; // Dont leave it dangling
    return retval;
  }

  final_buffptr = krealloc(final_buffptr, final_count + count, GFP_KERNEL);
  memcpy(final_buffptr + final_count, temp_buffptr, count);
  PDEBUG("final_buffptr: %s", final_buffptr);
  final_count += count;

  struct aesd_buffer_entry * write_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
  if (write_entry == NULL) {
    retval = -ENOMEM;
  } else {
    write_entry -> size = final_count;
    write_entry -> buffptr = kmalloc(final_count, GFP_KERNEL);
    memcpy(write_entry -> buffptr, final_buffptr, final_count);

    //mutex_lock(&dev->lock);
    mutex_lock_interruptible( & aesd_device.lock);
    const char * overwritten_buffptr = aesd_circular_buffer_add_entry(aesd_device.buffer, write_entry);
    mutex_unlock( & aesd_device.lock);

    /*If overwritten, free the overwritten entry*/
    if (overwritten_buffptr != NULL) {
      kfree(overwritten_buffptr);
    }
    retval = final_count;
    final_count = 0;
  }

  return retval;
}

/**
 * aesd_llseek - Change the file position
 *
 * This function is responsible for changing the file position (seek) within
 * the device file. It calculates the updated offset based on the given offset
 * and whence parameters.
 *
 * @param filp   Pointer to the file structure for the device file.
 * @param offset The new file position offset.
 * @param whence The reference point for the offset (SEEK_SET, SEEK_CUR, SEEK_END).
 * @return       Returns the updated file position offset on success, or a
 *               negative error code on failure.
 */
loff_t aesd_llseek(struct file * filp, loff_t offset, int whence) {
  PDEBUG("llseek");
  if (filp == NULL) {
    return -EFAULT;
  }
  struct aesd_dev * dev = filp -> private_data;
  struct aesd_buffer_entry * iter_entry;
  loff_t updated_offset = 0, size = 0;
  int index = 0;
  if (mutex_lock_interruptible( & dev -> lock) != 0) {
    return -ERESTARTSYS;
  }
  AESD_CIRCULAR_BUFFER_FOREACH(iter_entry, aesd_device.buffer, index) {
    size += iter_entry -> size;
  }
  updated_offset = fixed_size_llseek(filp, offset, whence, size);
  PDEBUG("llseek to %lld", updated_offset);
  mutex_unlock( & dev -> lock);
  return updated_offset;
}

/**
 * Adjust the file offset (f_pos) parameter of @param filp based on the location specified by
 * @param write cmd (the zero referenced command to locate)
 * and @param write_cmd_offset (the zero referenced offset into the command)
 * @return 0 if successful, negative if error occurred:
 * -ERESTARTSYS if mutex could not be obtained
 * -EINVAL if write command or write_cmd_offset was out of range
 */
static long aesd_adjust_file_offset(struct file * filp, unsigned int write_cmd, unsigned int write_cmd_offset) {
  long retval = 0;
  int i;
  if (filp == NULL) {
    return -EFAULT;
  }
  struct aesd_dev * dev = filp -> private_data;
  if (mutex_lock_interruptible( & dev -> lock) != 0) {
    return -ERESTARTSYS;
  }
  if (write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) //valid write_cmd value check
  {
    retval = -EINVAL;
    mutex_unlock( & dev -> lock);
    return retval;
  } else if (write_cmd_offset > dev -> buffer -> entry[write_cmd].size) //write_cmd_offset is >= size of command
  {
    retval = -EINVAL;
    mutex_unlock( & dev -> lock);
    return retval;
  } else if (dev -> buffer -> entry[write_cmd].buffptr == NULL) {
    retval = -EINVAL;
    mutex_unlock( & dev -> lock);
    return retval;
  } else {
    for (i = 0; i < write_cmd; i++) {
      filp -> f_pos += dev -> buffer -> entry[i].size;
    }
    filp -> f_pos += write_cmd_offset;
    retval = filp -> f_pos;
  }
  mutex_unlock( & dev -> lock);
  return retval;
}

/**
 * aesd_ioctl - Perform device-specific control operations
 *
 * This function is responsible for handling device-specific control operations
 * (ioctl) for the device. It supports the `AESDCHAR_IOCSEEKTO` command for
 * adjusting the file offset.
 *
 * @param filp Pointer to the file structure for the device file.
 * @param cmd  The IOCTL command to perform.
 * @param arg  An argument for the IOCTL command.
 * @return     Returns 0 on success or a negative error code on failure.
 */
long aesd_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
  PDEBUG("ioctl");
  long retval = 0;
  if (filp == NULL) {
    return -EFAULT;
  }
  if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) //bounds checking
  {
    return -ENOTTY;
  }
  switch (cmd) {
  case AESDCHAR_IOCSEEKTO: {
    struct aesd_seekto seekto;
    if (copy_from_user( & seekto, (const void * ) arg, sizeof(seekto))) {
      retval = EFAULT;
    } else {
      retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
    }
    break;
  }
  }
  return retval;
}
struct file_operations aesd_fops = {
  .owner = THIS_MODULE,
  .read = aesd_read,
  .write = aesd_write,
  .open = aesd_open,
  .release = aesd_release,
  .llseek = aesd_llseek,
  .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev * dev) {
  int err, devno = MKDEV(aesd_major, aesd_minor);

  cdev_init( & dev -> cdev, & aesd_fops);
  dev -> cdev.owner = THIS_MODULE;
  dev -> cdev.ops = & aesd_fops;
  err = cdev_add( & dev -> cdev, devno, 1);
  if (err) {
    printk(KERN_ERR "Error %d adding aesd cdev", err);
  }
  return err;
}

int aesd_init_module(void) {
  dev_t dev = 0;
  int result;
  result = alloc_chrdev_region( & dev, aesd_minor, 1,
    "aesdchar");
  aesd_major = MAJOR(dev);
  if (result < 0) {
    printk(KERN_WARNING "Can't get major %d\n", aesd_major);
    return result;
  }
  memset( & aesd_device, 0, sizeof(struct aesd_dev));
  PDEBUG("Init AESD module");

  struct aesd_circular_buffer * buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
  aesd_device.buffer = buffer;
  mutex_init( & aesd_device.lock);
  aesd_circular_buffer_init(aesd_device.buffer); //initialize the buffer
  final_buffptr = kmalloc(4, GFP_KERNEL);

  /**
   * TODO: initialize the AESD specific portion of the device
   */

  result = aesd_setup_cdev( & aesd_device);

  if (result) {
    unregister_chrdev_region(dev, 1);
  }
  return result;
}

void aesd_cleanup_module(void) {
  dev_t devno = MKDEV(aesd_major, aesd_minor);

  cdev_del( & aesd_device.cdev);

  struct aesd_buffer_entry * cleanup_entry;
  uint8_t index;
  AESD_CIRCULAR_BUFFER_FOREACH(cleanup_entry, aesd_device.buffer, index) {
    if (cleanup_entry -> buffptr != NULL) {
      kfree(cleanup_entry -> buffptr);
      PDEBUG("Entry freed\n");
    }
  }

  if (aesd_device.buffer != NULL) {
    kfree(aesd_device.buffer);
    PDEBUG("aesd_device.buffer freed\n");
  }

  // Assuming final_buffptr is a pointer, add a null check
  if (final_buffptr != NULL) {
    kfree(final_buffptr);
    PDEBUG("final_buffptr freed\n");
  }

  mutex_destroy( & aesd_device.lock);
  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
