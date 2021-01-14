/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <charDeviceDriver.h>

MODULE_LICENSE("GPL");
DEFINE_MUTEX(device_lock);
LIST_HEAD(messages_list);

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
    mutex_lock(&device_lock);
    is_opened++;
    mutex_unlock(&device_lock);
    try_module_get(THIS_MODULE);
    return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock(&device_lock);
    is_opened--;
    mutex_unlock(&device_lock);
    module_put(THIS_MODULE);
    return SUCCESS;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 * This is just an example about how to copy a message in the user space
 * You will need to modify this function
 */
static ssize_t device_read(struct file *filp, /* see include/linux/fs.h   */
                           char *buffer,      /* buffer to fill with data */
                           size_t length,     /* length of the buffer     */
                           loff_t *offset)
{
    struct message *entry;
    size_t msg_length;

    mutex_lock(&device_lock);
    if (list_empty(&messages_list))
    {
        mutex_unlock(&device_lock);
        return -EAGAIN;
    }

    entry = list_first_entry(&messages_list, struct message, list);
    msg_length = entry->length;

    if (copy_to_user(buffer, entry->data, msg_length + 1))
    {
        mutex_unlock(&device_lock);
        return -EFAULT;
    }

    messages_length -= msg_length;
    list_del(&entry->list);
    kfree(entry->data);
    kfree(entry);
    mutex_unlock(&device_lock);
    return msg_length + 1;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello -ENOMEM */
static ssize_t device_write(struct file *filp,
                            const char *buffer,
                            size_t length,
                            loff_t *off)
{
    struct message *msg;

    mutex_lock(&device_lock);
    if (length - 1 > MESSAGE_SIZE)
    {
        mutex_unlock(&device_lock);
        return -EINVAL;
    }

    if ((length - 1 + messages_length) > MESSAGES_SIZE)
    {
        mutex_unlock(&device_lock);
        return -EAGAIN;
    }

    msg = kmalloc(sizeof(struct message), GFP_KERNEL);
    msg->data = kmalloc(length, GFP_KERNEL);

    if (!msg->data)
    {
        kfree(msg->data);
        kfree(msg);
        mutex_unlock(&device_lock);
        return -ENOMEM;
    }

    if (copy_from_user(msg->data, buffer, length))
    {
        kfree(msg->data);
        kfree(msg);
        mutex_unlock(&device_lock);
        return -EFAULT;
    }

    INIT_LIST_HEAD(&msg->list);
    list_add(&msg->list, &messages_list);
    msg->length = length - 1;
    messages_length += length - 1;
    mutex_unlock(&device_lock);
    return length;
}

/*
 * This function is called when the module is loaded
 * You will not need to modify it for this assignment
 */
int init_module(void)
{
    major_number = register_chrdev(0, DEVICE_NAME, &fops);

    if (major_number < 0)
    {
        printk(KERN_ALERT "Registering char device failed with %d\n", major_number);
        return major_number;
    }

    printk(KERN_INFO "I was assigned major_number number %d. To talk to\n", major_number);
    printk(KERN_INFO "The driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, major_number);
    printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
    printk(KERN_INFO "The device file.\n");
    printk(KERN_INFO "Remove the device file and module when done.\n");

    return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    struct message *entry;

    while (!list_empty(&messages_list))
    {
        entry = list_first_entry_or_null(&messages_list, struct message, list);
        list_del(&entry->list);
        kfree(entry->data);
        kfree(entry);
    }

    unregister_chrdev(major_number, DEVICE_NAME);
}
