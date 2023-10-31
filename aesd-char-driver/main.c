/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;
char* final_buffptr = NULL;

MODULE_AUTHOR("Visweshwaran Baskaran"); /** TODO: fill in your name **/
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
int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    //Based on scull_open() in scull/main.c   
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release"); 
    // Nothing to add based on scull/main.c
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
printk("In aesd_read\n");
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    ssize_t entry_offset = 0; 
    if (!filp)
    {
    	printk("filp does not exist\n\r");
        return -EFAULT; 
    }

	mutex_lock_interruptible(&aesd_device.lock);
	struct aesd_buffer_entry *read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(aesd_device.buffer, (size_t)*f_pos, &entry_offset);

	if(read_entry == NULL) //checking if the buffer entry is found
    	{
    		printk("read_entry not found\n\r");
        	mutex_unlock(&(aesd_device.lock));
        	return retval;
    	}


 	ssize_t bytes_to_read = read_entry->size - entry_offset;      
        if (bytes_to_read < count)
        ;
        else
            bytes_to_read = count;          

        if (copy_to_user(buf, read_entry->buffptr + entry_offset, bytes_to_read) != 0) 
        {
             printk(KERN_ALERT "copy_to_user failed\n");
             return -EFAULT;
        } 
        else
        {
        printk("copy_to_user passed\r\n");
        }
        
        *f_pos += bytes_to_read;
        retval = bytes_to_read;

    	mutex_unlock(&aesd_device.lock);
    	return retval;
}

/*
@reference: Code leveraged from Ashwin Ravindra's implementation of aesd_write
*/
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
printk("In aesd_Write\n");
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    static size_t final_count = 0;
    //struct aesd_dev *dev = (struct aesd_dev*) filp->private_data;
   
    char *temp_buffptr = kmalloc(count, GFP_KERNEL);
    if (!temp_buffptr)
    {
    printk("temp_buffptr does not exist\n");
        return retval;
    }
    //static char *final_buffptr = NULL;
    if (!final_buffptr) 
    {
    printk("final_buffptr does not exist\n");
            kfree(temp_buffptr);
            return retval;
    }
    if (copy_from_user(temp_buffptr, buf, count) != 0) 
    {
    	printk("copy_from_user failed\n");
        kfree(temp_buffptr);
        return retval;
    }
     if(strchr(temp_buffptr, '\n') == NULL) {
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
    
    struct aesd_buffer_entry *write_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    if (write_entry == NULL) 
    {
        retval = -ENOMEM;
    }
    else 
    {
        write_entry->size = final_count;
        write_entry->buffptr = kmalloc(final_count, GFP_KERNEL);
        memcpy(write_entry->buffptr, final_buffptr, final_count);

       //mutex_lock(&dev->lock);
       mutex_lock_interruptible(&aesd_device.lock);
        const char* overwritten_buffptr = aesd_circular_buffer_add_entry(aesd_device.buffer, write_entry);

  
    //mutex_unlock(&dev->lock);
    mutex_unlock(&aesd_device.lock);

        /*If overwritten, free the overwritten entry*/
        if(overwritten_buffptr != NULL) {
            kfree(overwritten_buffptr);
        }
        retval = final_count;
        final_count = 0;
    }

 
	    
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
    PDEBUG("Init AESD module");

    struct aesd_circular_buffer* buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    aesd_device.buffer = buffer;
    mutex_init(&aesd_device.lock); 
    aesd_circular_buffer_init(aesd_device.buffer); //initialize the buffer
    final_buffptr = kmalloc(4, GFP_KERNEL); 
    
    /**
     * TODO: initialize the AESD specific portion of the device
     */
     
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);



	struct aesd_buffer_entry* cleanup_entry;
    uint8_t index;
    AESD_CIRCULAR_BUFFER_FOREACH(cleanup_entry, aesd_device.buffer, index) 
    {
    	if(cleanup_entry->buffptr != NULL)
    	{
        kfree(cleanup_entry->buffptr);
        PDEBUG("Entry freed\n");
        }
    }

  
 if (aesd_device.buffer != NULL)
    {
        kfree(aesd_device.buffer);
        PDEBUG("aesd_device.buffer freed\n");
    }

    // Assuming final_buffptr is a pointer, add a null check
    if (final_buffptr != NULL)
    {
        kfree(final_buffptr);
        PDEBUG("final_buffptr freed\n");
    }

    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
