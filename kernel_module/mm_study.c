#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kprobes.h>

#define log(a, ...) printk("[ %s : %.3d ] "a"\n", \
				__func__, __LINE__,  ## __VA_ARGS__)

#define func_name	"__isolate_lru_page"

static int kp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct page *page;
	int mode;
	/*
	 * @di is the first parameter, @si is the second.
	 *
	 * log("%lx, 2:%lx", regs->di, regs->si);
	 */
	//dump_stack();
	page = (struct page*)(regs->di);
	mode = regs->si;

	printk("[%s] page : %lx, mode: %d flags: %.16lx, map count: %d, page count: %d, (%d, %d)", func_name,
		(unsigned long)page, mode, page->flags, page_mapcount(page), page_count(page), PageLRU(page), PG_lru);
	return 0;
}

static void kp_post_handler(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	//log();
}

static struct kprobe kp = {
	.symbol_name = func_name,
	.pre_handler = kp_pre_handler,
	.post_handler = kp_post_handler
};

static int __init mm_study(void)
{
	int ret = 0;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register_kprobe returned %d\n", ret);
		return ret;
	}

	return ret;
}

static void __exit mm_study_exit(void)
{
	log("---------------- bye world ---------------------");
	unregister_kprobe(&kp);
}

module_init(mm_study);
module_exit(mm_study_exit);

MODULE_AUTHOR("Huang Shijie");
MODULE_LICENSE("GPL v2");
