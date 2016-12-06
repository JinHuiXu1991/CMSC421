/**
 *  CMSC421 Project 1
 *  Author: 	Jin Hui Xu
 *  E-mail:	ac39537@umbc.edu
 * Description: This project will implement a Linux kernel module that will "encrypt" data using a Caesar Cipher. 
 *		The encryption key is the number of letters to shift. The module will create two miscellaneous devices, 
 *		/dev/rotX and /dev/rotXctl. The user writes to /dev/rotX the encryption key. Next, the user creates 
 *		a memory map to /dev/rotX, via mmap(), to store the data to encrypt. Next, the user writes the magic 
 *		string go to /dev/rotXctl to actually perform the encryption; the resulting ciphertext is fetched from 
 *		the memory mapping. 
 *
 */

/*
 * This file uses kernel-doc style comments, which is similar to
 * Javadoc and Doxygen-style comments.  See
 * ~/linux/Documentation/kernel-doc-nano-HOWTO.txt for details.
 */

/*
 * Getting compilation warnings?  The Linux kernel is written against
 * C89, which means:
 *  - No // comments, and
 *  - All variables must be declared at the top of functions.
 * Read ~/linux/Documentation/CodingStyle to ensure your project
 * compiles without warnings.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#define PREFIX "ROT-X: "

static char *rotX_buffer;
static unsigned int key;
static DEFINE_SPINLOCK(lock);

/**
 * rotX_read() - callback invoked when a process reads from /dev/rotX
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination buffer to store key
 * @count: number of bytes in @ubuf
 * @ppos: file offset (in/out parameter)
 *
 * Write to @ubuf the ASCII string representation of the key (via
 * snprintf()), incrementing @ppos by the number of bytes written. If
 * @ppos already points to the end of the string, then this user has
 * reached the end of file.
 *
 * Return: number of bytes written to @ubuf, 0 on end of file, or
 * negative on error
 */
static ssize_t rotX_read(struct file *filp, char __user * ubuf,
			 size_t count, loff_t * ppos)
{
	int retval;
	char buff[100];
	spin_lock(&lock);
	snprintf(buff, count, "%d", key);
	
	if (*ppos >= sizeof(buff)) {
		spin_unlock(&lock);
		return 0;
	}

	if (count + *ppos > sizeof(buff))
		count = sizeof(buff) - *ppos;

	retval = copy_to_user(ubuf, &buff + *ppos, count);

	if (retval < 0) {
		spin_unlock(&lock);
		return retval;
	}

	*ppos += count;

	spin_unlock(&lock);
	return count;

}

/**
 * rotX_write() - callback invoked when a process writes to /dev/rotX
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer of bytes from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Interpreting the string at @ubuf as an ASCII value, convert it to
 * an unsigned value via kstrtouint_from_user. Store the resulting
 * value as the encryption key.
 *
 * Return: @count, or negative on error
 */
static ssize_t rotX_write(struct file *filp, const char __user * ubuf,
			  size_t count, loff_t * ppos)
{
	int retval;
	spin_lock(&lock);

	retval = kstrtouint_from_user(ubuf, count, 10, &key);

	if (retval < 0) {
		spin_unlock(&lock);
		return retval;
	}

	spin_unlock(&lock);
	return count;
}

/**
 * rotX_mmap() - callback invoked when a process mmap()s to /dev/rotX
 * @filp: process's file object that is writing to this device (ignored)
 * @vma: virtual memory allocation object containing mmap() request
 *
 * Create a mapping from kernel memory (specifically, rotX_buffer)
 * into user space. Code based upon
 * http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace/
 *
 * You do not need to do modify this function.
 *
 * Return: 0 on success, negative on error.
 */
static int rotX_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
	unsigned long page = vmalloc_to_pfn(rotX_buffer);
	if (size > PAGE_SIZE)
		return -EIO;
	vma->vm_pgoff = 0;
	vma->vm_page_prot = PAGE_SHARED;
	if (remap_pfn_range(vma, vma->vm_start, page, size, vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

/**
 * rotXctl_write() - callback invoked when a process writes to
 * /dev/rotXctl
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * If @count is at least 2 and @ubuf begins with the string "go", then
 * encrypt the data buffer with the current encryption key. Otherwise,
 * consume @ubuf and do nothing; this is not an error.
 *
 * Return: @count, or negative on error
 */
static ssize_t rotXctl_write(struct file *filp, const char __user * ubuf,
			     size_t count, loff_t * ppos)
{
	char buff[100];
	unsigned int i;
	int retval;

	spin_lock(&lock);
	retval = copy_from_user(&buff, ubuf, count);

	if (retval < 0) {
		spin_unlock(&lock);
		return retval;
	}

	if (count >= 2 && buff[0] == 'g' && buff[1] == 'o') {
		if (key > 0 && key < 26) {
			for (i = 0; i < PAGE_SIZE; i++) {
				if ('a' <= rotX_buffer[i]
				    && rotX_buffer[i] <= 'z') {
					rotX_buffer[i] =
					    (rotX_buffer[i] - 'a' + key) % 26 +
					    'a';
				} else if ('A' <= rotX_buffer[i]
					   && rotX_buffer[i] <= 'Z') {
					rotX_buffer[i] =
					    (rotX_buffer[i] - 'A' + key) % 26 +
					    'A';
				}

			}
		}
	}

	spin_unlock(&lock);
	return count;
}

static const struct file_operations one_fops = {
	.read = rotX_read,
	.write = rotX_write,
	.mmap = rotX_mmap
};

static const struct file_operations two_fops = {
	.write = rotXctl_write
};

static struct miscdevice one_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.mode = 0666,
	.name = "rotX",
	.fops = &one_fops
};

static struct miscdevice two_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.mode = 0666,
	.name = "rotXctl",
	.fops = &two_fops
};

/**
 * rotX_init() - entry point into the ROT-X kernel module
 * Return: 0 on successful initialization, negative on error
 */
static int __init rotX_init(void)
{
	pr_info(PREFIX "Hello, world!\n");
	rotX_buffer = vmalloc(PAGE_SIZE);
	memset(rotX_buffer, 0, PAGE_SIZE);
	misc_register(&one_device);
	misc_register(&two_device);
	return 0;
}

/**
 * rotX_exit() - called by kernel to clean up resources
 */
static void __exit rotX_exit(void)
{
	pr_info(PREFIX "Goodbye, world!\n");
	misc_deregister(&one_device);
	misc_deregister(&two_device);
	vfree(rotX_buffer);
}

module_init(rotX_init);
module_exit(rotX_exit);

MODULE_DESCRIPTION("CS421 ROT-X driver");
MODULE_LICENSE("GPL");
