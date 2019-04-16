#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/rmap.h>
#include <linux/kprobes.h>
#include <linux/seq_buf.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/cdev.h>

dev_t cdevno;
struct cdev my_cdev;

static ssize_t gup_test_write(struct file *file, const char __user * buf,
                              size_t count, loff_t * pos)
{
	pr_info("%s -----------\n", __func__);
	if (count == 0)
		return 0;
        return count;
}

static ssize_t gup_test_read(struct file *file, char __user * buf,
                             size_t count, loff_t * pos)
{
	pr_info("%s -----------\n", __func__);
	if (count == 0)
		return 0;
        //return epu_dma_block_rx(file, buf, count, pos);
        return count;
}

static int gup_test_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	pr_info("%s -----------\n", __func__);
	return 0;
}

static int gup_test_close(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	pr_info("%s -----------\n", __func__);
	return 0;
}

static const struct file_operations gup_test_fops = {
	.owner = THIS_MODULE,
	.open = gup_test_open,
	.release = gup_test_close,
	.write = gup_test_write,
	.read = gup_test_read,
};

static struct class *gup_class;
struct device *gupdev;
static int __init gup_test(void)
{
	const char *name = "gup_test";
	gup_class = class_create(THIS_MODULE, "guptest");

	if (alloc_chrdev_region(&cdevno, 0, 1, name) < 0) {
		goto error;
	}
	printk("major :%d, minor: %d\n", MAJOR(cdevno), MINOR(cdevno));

	/* register file operation */
	cdev_init(&my_cdev, &gup_test_fops);

	if (cdev_add(&my_cdev, cdevno, 1) < 0) {
		goto error;
	}

	gupdev = device_create(gup_class, NULL, cdevno, NULL, "%s", "guptest");
	if (IS_ERR(gupdev)) {
		goto error;
	}
	pr_info("we create a file\n");
	return 0;
error:
	return -EINVAL;
}

static void __exit gup_test_exit(void)
{
	cdev_del(&my_cdev);
	unregister_chrdev_region(cdevno, 1);
	device_destroy(gup_class, cdevno);
	class_destroy(gup_class);
	pr_info("we exit now\n");
}

module_init(gup_test);
module_exit(gup_test_exit);

MODULE_AUTHOR("Huang Shijie");
MODULE_LICENSE("GPL v2");
