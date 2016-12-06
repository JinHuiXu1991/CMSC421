/**
 *  I am performing the extra credit part for this project
 *  CMSC421 Project 2
 *  Author: 	Jin Hui Xu
 *  E-mail:	ac39537@umbc.edu
 * Description:  In this project is to implement a Linux driver that operates upon a virtual Internet filter. 
 *		The user stores an arbitrary list of dirty words in the driver. The driver then scans incoming 
 *		network packets for those dirty words. For each found word, censor that word by overwriting it 
 *		with asterisks. 
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

#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#define PREFIX "SCRUBBER: "

struct node {
	char *word;
	struct list_head list;
};
struct node *temp;
LIST_HEAD(mylist);

static DEFINE_SPINLOCK(lock);

/*
 * Read xt_filter.c to learn how to use these two functions.
 */
#define FILTER_IRQ 6
extern void filter_enable(void);
extern void filter_disable(void);
extern void filter_resume(void);
extern char *filter_get_payload(size_t *);

/**
 * scrubber_open() - callback invoked when a process tries to open
 * /dev/scrubber
 * @inode: inode of character device (ignored)
 * @filp: process's file object (ignored)
 *
 * Return: always 0
 */
static int scrubber_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/**
 *scrubber_read function, not required, debug purpose only.
 */
static ssize_t scrubber_read(struct file *filp, char __user * ubuf,
			     size_t count, loff_t * ppos)
{
	int retval;
	char *buff;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	list_for_each_entry(temp, &mylist, list) {
		buff = temp->word;
		retval = copy_to_user(ubuf, buff, count);

		if (retval < 0) {
			spin_unlock_irqrestore(&lock, flags);
			return retval;
		}
	}
	spin_unlock_irqrestore(&lock, flags);
	return count;
}

/**
 * scrubber_write() - callback invoked when a process writes to
 * /dev/scrubber
 * @filp: process's file object (ignored)
 * @ubuf: source buffer
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Copy from @ubuf into a local buffer, the lesser of @count or 80
 * bytes. Tokenize the buffer, splitting on the newline ('\n')
 * character. For each token, add it to an internal list of "dirty
 * words".
 *
 * WARNING: @ubuf is not null-terminated.
 *
 * HINT: strsep() and kstrdup() are useful here.
 *
 * Return: @count on success, negative on error
 */
static ssize_t scrubber_write(struct file *filp, const char __user * ubuf,
			      size_t count, loff_t * ppos)
{
	/* YOUR CODE HERE */
	char buff[80];
	int retval, i;
	char delimiters[] = " .,;:!-0123456789\n";
	char *token;
	char *running;
	unsigned long flags;
	int bool = 1;

	spin_lock_irqsave(&lock, flags);

	/*check if the input is greater than packet length 80 */
	if (count <= 80)
		retval = copy_from_user(&buff, ubuf, count);
	else {
		spin_unlock_irqrestore(&lock, flags);
		return count;
	}

	if (retval < 0) {
		spin_unlock_irqrestore(&lock, flags);
		return retval;
	}

	/*check if the input is empty word */
	for (i = 0; i < count; i++) {
		if (buff[i] == ' ')
			bool = 1;
		else
			bool = 0;
	}

	/*if it is empty word, don't store it into list */
	if (bool == 1) {
		spin_unlock_irqrestore(&lock, flags);
		return count;
	}

	buff[count] = '\0';
	running = kstrdup(buff, GFP_KERNEL);
	while (running != NULL) {
		token = strsep(&running, delimiters);
		temp = kmalloc(sizeof(*temp), GFP_KERNEL);
		if (!temp) {
			spin_unlock_irqrestore(&lock, flags);
			return -ENOMEM;
		}
		temp->word = token;
		list_add_tail(&temp->list, &mylist);
	}

	spin_unlock_irqrestore(&lock, flags);
	return count;
}

/**
 * extra credit character device:
 * scrubber_del_write: scan over linked list and delete the input word
 */
static ssize_t scrubber_del_write(struct file *filp, const char __user * ubuf,
				  size_t count, loff_t * ppos)
{
	char buff[80];
	int retval, i;
	char delimiters[] = " .,;:!-0123456789\n";
	char *token;
	char *running;
	unsigned long flags;
	int bool = 1;

	spin_lock_irqsave(&lock, flags);

	/*check if the input is greater than packet length 80 */
	if (count <= 80)
		retval = copy_from_user(&buff, ubuf, count);
	else {
		spin_unlock_irqrestore(&lock, flags);
		return count;
	}

	if (retval < 0) {
		spin_unlock_irqrestore(&lock, flags);
		return retval;
	}

	/*check if the input is empty word */
	for (i = 0; i < count; i++) {
		if (buff[i] == ' ')
			bool = 1;
		else
			bool = 0;
	}

	/*if it is empty word, do nothing */
	if (bool == 1) {
		spin_unlock_irqrestore(&lock, flags);
		return count;
	}

	buff[count] = '\0';
	running = kstrdup(buff, GFP_KERNEL);
	while (running != NULL) {
		token = strsep(&running, delimiters);
		list_for_each_entry(temp, &mylist, list) {
			if (token == temp->word)
				list_del(&temp->list);

		}
	}

	spin_unlock_irqrestore(&lock, flags);
	return count;
}

/**
 * scrubber_check() - top-half of scrubber ISR
 * @irq: IRQ that was invoked
 * @cookie: Pointer to data that was passed into
 * register_threader_irq() (ignored)
 *
 * If @irq is FILTER_IRQ, then wake up the bottom-half. Otherwise,
 * return IRQ_NONE.
 */
static irqreturn_t scrubber_check(int irq, void *cookie)
{
	unsigned long flags;
	spin_lock_irqsave(&lock, flags);

	if (irq == FILTER_IRQ) {
		spin_unlock_irqrestore(&lock, flags);
		return IRQ_WAKE_THREAD;
	} else {
		spin_unlock_irqrestore(&lock, flags);
		return IRQ_NONE;
	}

}

/**
 * scrubber_handler() - bottom-half to scrubber ISR
 * @irq: IRQ that was invoked
 * @cookie: Pointer that was passed into register_threaded_irq()
 * (ignored)
 *
 * For each word in the dirty word list, scan the payload for that
 * word. For each instance found, case-sensitive matching, overwrite
 * that part of the payload with asterisks. Afterwords, resume the
 * Internet filter.
 *
 * HINT: use list_for_each_entry() to iterate over the dirty word list
 * HINT: strnstr()/memcmp() and memset() are useful here.
 *
 * WARNING: The payload is not null-terminated.
 *
 * Return: always IRQ_HANDLED
 */
static irqreturn_t scrubber_handler(int irq, void *cookie)
{
	char *word;
	char *payload;
	size_t len;
	char *ptr;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	list_for_each_entry(temp, &mylist, list) {
		word = temp->word;

		payload = filter_get_payload(&len);
		payload[len - 1] = '\0';

		ptr = strnstr(payload, word, len);
		if (ptr != NULL && ptr != payload)
			memset(ptr, '*', strlen(word));

	}
	spin_unlock_irqrestore(&lock, flags);
	filter_resume();
	return IRQ_HANDLED;
}

static const struct file_operations scrubber_fops = {
	.owner = THIS_MODULE,
	.open = scrubber_open,
	.read = scrubber_read,
	.write = scrubber_write
};

static struct miscdevice scrubber_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scrubber",
	.fops = &scrubber_fops,
	.mode = 0666,
};

/**
 * extra credit: scrubber_del device
 */
static const struct file_operations scrubber_del_fops = {
	.owner = THIS_MODULE,
	.write = scrubber_del_write
};

static struct miscdevice scrubber_del_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scrubber_del",
	.fops = &scrubber_del_fops,
	.mode = 0666,
};

/**
 * scrubber_init() - entry point into the scubber kernel module
 * Return: 0 on successful initialization, negative on error
 */
static int __init scrubber_init(void)
{
	int retval;

	retval = misc_register(&scrubber_dev);
	if (retval) {
		pr_err(PREFIX "Could not register device\n");
		return -ENODEV;
	}

	retval = misc_register(&scrubber_del_dev);
	if (retval) {
		pr_err(PREFIX "Could not register device\n");
		return -ENODEV;
	}

	retval =
	    request_threaded_irq(FILTER_IRQ, scrubber_check, scrubber_handler,
				 0, "scrubber", &scrubber_dev);
	if (retval) {
		misc_deregister(&scrubber_dev);
		misc_deregister(&scrubber_del_dev);
	} else
		filter_enable();

	return 0;
}

/**
 * scrubber_exit() - called by kernel to clean up resources
 */
static void __exit scrubber_exit(void)
{
	struct node *tmp;
	list_for_each_entry_safe(temp, tmp, &mylist, list) {
		kfree(temp);
	}
	INIT_LIST_HEAD(&mylist);
	free_irq(FILTER_IRQ, &scrubber_dev);
	filter_disable();
	misc_deregister(&scrubber_dev);
	misc_deregister(&scrubber_del_dev);

}

module_init(scrubber_init);
module_exit(scrubber_exit);

MODULE_DESCRIPTION("CS421 scrubber driver");
MODULE_LICENSE("GPL");
