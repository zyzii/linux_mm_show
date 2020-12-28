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
#include <linux/mm_inline.h>

static int __init mm_study(void)
{
	int ret = 0;

	printk("%s -- start\n", __func__);

	return ret;
}

static void __exit mm_study_exit(void)
{
	printk("%s -- exit\n", __func__);
}

module_init(mm_study);
module_exit(mm_study_exit);

MODULE_AUTHOR("Huang Shijie");
MODULE_LICENSE("GPL v2");
