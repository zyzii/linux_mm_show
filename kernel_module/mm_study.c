#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kprobes.h>

#define log(a, ...) printk("[ %s: %s : %.3d ] "a"\n", \
				__FILE__, __FUNCTION__, __LINE__,  ## __VA_ARGS__)

static int kp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	/*
	 * @di is the first parameter, @si is the second.
	 *
	 * log("%lx, 2:%lx", regs->di, regs->si);
	 */
	//dump_stack();
	return 0;
}

static void kp_post_handler(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	log();
}

static struct kprobe kp = {
	.symbol_name = "__isolate_lru_page",
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
