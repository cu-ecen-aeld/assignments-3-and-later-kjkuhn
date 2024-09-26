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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    //nothing todo
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    //nothing todo
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_buffer_entry *entry;
    size_t offset;
    size_t bytes_to_read;

    offset = 0;
    retval = 0;
    entry = NULL

    // Find the entry in the circular buffer corresponding to the current file position
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.circular_buf, *f_pos, &offset);
    if(entry == NULL) {
        // Reached the end of the buffer. For this assignment, we don't read beyond the buffer
        goto out;
    }

    // Calculate how many bytes can be read from the current entry
    bytes_to_read = min(count, entry->size - offset);

    // Copy data from the kernel buffer to the user buffer
    if (copy_to_user(buf, entry->buffptr + offset, bytes_to_read) != 0) {
        retval = -EFAULT;
        goto out;
    }

    retval = bytes_to_read;
    *f_pos += bytes_to_read;

out:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval;
    struct aesd_buffer_entry entry;
    char *buffer;

    retval = -ENOMEM;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    // TODO: handle write
    // Allocate kernel buffer to copy data from user space
    buffer = kmalloc(count, GFP_KERNEL);
    if(buffer == NULL){
        // Memory allocation failed
        goto out;
    }

    // Copy data from user space to kernel space
    if(copy_from_user(buffer, buf, count)){
        // Copy failed
        retval = -EFAULT;
        goto out_with_kfree;
    }

    // Prepare the entry for the circular buffer
    entry.buffptr = buffer;
    entry.size = count;

    // Add the entry to the circular buffer
    aesd_circular_buffer_add_entry(&aesd_device.circular_buf, &entry);

    retval = count; // Success, all bytes written

out_with_kfree:
    kfree(buffer);
out:
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
    //init buffer
    aesd_circular_buffer_init(&aesd_device.circular_buf);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev;
    int result;

    dev = 0;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

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
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    

    // Free the memory allocated for each buffer entry
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buf, index)
    {
        if (entry->buffptr) {
            kfree((void *)entry->buffptr); // Cast to non-const void*
        }
    }
    

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
