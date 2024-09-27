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
#include "linux/slab.h"
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("kjkuhn"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    //dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = &aesd_device;
    
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

void print_bytes(const char *msg, char *buffer, size_t offset, size_t nbytes)
{
    char *out;

    out = kmalloc(nbytes+1, GFP_KERNEL);
    if(out)
    {
        memcpy(out, &buffer[offset], nbytes);
        out[nbytes] = 0;
        PDEBUG("%s %s\n", msg, out);
        kfree(out);
    }
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval;
    /**
     * TODO: handle read
     */
    struct aesd_buffer_entry *entry;
    size_t offset;
    size_t bytes_to_read;
    struct aesd_dev *dev;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    offset = 0;
    retval = 0;
    entry = NULL;

    dev = filp->private_data;

    while(mutex_lock_interruptible(&dev->lock));

    // Find the entry in the circular buffer corresponding to the current file position
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buf, *f_pos, &offset);
    if(entry == NULL) {
        // Reached the end of the buffer. For this assignment, we don't read beyond the buffer
        PDEBUG("Nothing to read, returning 0\n");
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
    PDEBUG("returned %ld bytes to user from addr %p (orig %p, off %lu)\n", retval, &entry->buffptr[offset], entry->buffptr, offset);
    print_bytes("returning: ", entry->buffptr, offset, bytes_to_read);

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval;
    struct aesd_dev *dev;
    int i;
    char *buffer;

    retval = -ENOMEM;

    dev = filp->private_data;
    while(mutex_lock_interruptible(&dev->lock));

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    // TODO: handle write
    // Allocate kernel buffer to copy data from user space
    buffer = kmalloc(dev->entry.size + count, GFP_KERNEL);

    if(buffer == NULL){
        // Memory allocation failed
        goto out;
    }
    if(dev->entry.size > 0)
    {
        memcpy(buffer, dev->entry.buffptr, dev->entry.size);
        kfree(dev->entry.buffptr);
    }

    // Copy data from user space to kernel space
    if(copy_from_user(&buffer[dev->entry.size], buf, count)){
        // Copy failed
        retval = -EFAULT;
        goto out_with_kfree;
    }

    // Prepare the entry for the circular buffer
    dev->entry.buffptr = buffer;
    dev->entry.size += count;

    // Add the entry to the circular buffer
    if(dev->entry.buffptr[dev->entry.size-1] == '\n')
    {
        buffer = (char*) aesd_circular_buffer_add_entry(&dev->circular_buf, &dev->entry);
        memset(&dev->entry, 0, sizeof(struct aesd_buffer_entry));
        if(buffer != 0)
            kfree(buffer);
    }
    // print values
    for(i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        PDEBUG("%d: at %p, length %lu\n", i, dev->circular_buf.entry[i].buffptr, dev->circular_buf.entry[i].size);
        print_bytes("content: ", dev->circular_buf.entry[i].buffptr, 0, dev->circular_buf.entry[i].size);
    }

    retval = count; // Success, all bytes written
    *f_pos = aesd_size(&dev->circular_buf);
    goto out;

out_with_kfree:
    kfree(buffer);
out:
    mutex_unlock(&dev->lock);
    return retval;
}


long aesd_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    int64_t result;
    struct aesd_seekto as;
    struct aesd_dev *dev;
    uint8_t idx;
    uint64_t total_size;
    uint32_t counter;

    result = -EINVAL;
    dev = fp->private_data;
    total_size = 0;
    counter = 0;

    if(cmd == AESDCHAR_IOCSEEKTO && 
        copy_from_user(&as, (const void __user*)arg, sizeof(as)) == 0 &&
        !dev->circular_buf.full && dev->circular_buf.in_offs == dev->circular_buf.out_offs
    )
    {
        PDEBUG("running aesd_ioctl with %u,%u\n", as.write_cmd, as.write_cmd_offset);
        while(mutex_lock_interruptible(&dev->lock));
        idx = dev->circular_buf.out_offs;
        do
        {
            PDEBUG("counter is %u, as.write_cmd is %u\n", counter, as.write_cmd);
            if(counter == as.write_cmd)
            {
                PDEBUG("counter = as.write_cmd = %u\n", counter);
                if(dev->circular_buf.entry[idx].size >= as.write_cmd_offset)
                {
                    fp->f_pos = total_size + as.write_cmd_offset;
                    PDEBUG("f_pos = %lu\n", fp->f_pos);
                    result = 0;
                }
                break;
            }
            total_size += dev->circular_buf.entry[idx].size;
            idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
            counter++;
        } while(idx != dev->circular_buf.in_offs);  
        mutex_unlock(&dev->lock);
    }
    return result;
}

loff_t aesd_seek(struct file *fp, loff_t offset, int whence)
{
    struct aesd_dev *dev;
    loff_t result;

    dev = fp->private_data;

    while(mutex_lock_interruptible(&dev->lock));
    result = fixed_size_llseek(fp, offset, whence, aesd_size(&dev->circular_buf));
    mutex_unlock(&dev->lock);
    
    return result;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_seek,
    .unlocked_ioctl = aesd_ioctl,
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

    //init buffer
    aesd_circular_buffer_init(&aesd_device.circular_buf);
    mutex_init(&aesd_device.lock);

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
