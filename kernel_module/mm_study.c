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

#define log(a, ...) printk("[ %s : %.3d ] "a"\n", \
				__func__, __LINE__,  ## __VA_ARGS__)


#define __def_gfpflag_names						\
	{(unsigned long)GFP_TRANSHUGE,		"GFP_TRANSHUGE"},	\
	{(unsigned long)GFP_TRANSHUGE_LIGHT,	"GFP_TRANSHUGE_LIGHT"}, \
	{(unsigned long)GFP_HIGHUSER_MOVABLE,	"GFP_HIGHUSER_MOVABLE"},\
	{(unsigned long)GFP_HIGHUSER,		"GFP_HIGHUSER"},	\
	{(unsigned long)GFP_USER,		"GFP_USER"},		\
	{(unsigned long)GFP_KERNEL_ACCOUNT,	"GFP_KERNEL_ACCOUNT"},	\
	{(unsigned long)GFP_KERNEL,		"GFP_KERNEL"},		\
	{(unsigned long)GFP_NOFS,		"GFP_NOFS"},		\
	{(unsigned long)GFP_ATOMIC,		"GFP_ATOMIC"},		\
	{(unsigned long)GFP_NOIO,		"GFP_NOIO"},		\
	{(unsigned long)GFP_NOWAIT,		"GFP_NOWAIT"},		\
	{(unsigned long)GFP_DMA,		"GFP_DMA"},		\
	{(unsigned long)__GFP_HIGHMEM,		"__GFP_HIGHMEM"},	\
	{(unsigned long)GFP_DMA32,		"GFP_DMA32"},		\
	{(unsigned long)__GFP_HIGH,		"__GFP_HIGH"},		\
	{(unsigned long)__GFP_ATOMIC,		"__GFP_ATOMIC"},	\
	{(unsigned long)__GFP_IO,		"__GFP_IO"},		\
	{(unsigned long)__GFP_FS,		"__GFP_FS"},		\
	{(unsigned long)__GFP_NOWARN,		"__GFP_NOWARN"},	\
	{(unsigned long)__GFP_RETRY_MAYFAIL,	"__GFP_RETRY_MAYFAIL"},	\
	{(unsigned long)__GFP_NOFAIL,		"__GFP_NOFAIL"},	\
	{(unsigned long)__GFP_NORETRY,		"__GFP_NORETRY"},	\
	{(unsigned long)__GFP_COMP,		"__GFP_COMP"},		\
	{(unsigned long)__GFP_ZERO,		"__GFP_ZERO"},		\
	{(unsigned long)__GFP_NOMEMALLOC,	"__GFP_NOMEMALLOC"},	\
	{(unsigned long)__GFP_MEMALLOC,		"__GFP_MEMALLOC"},	\
	{(unsigned long)__GFP_HARDWALL,		"__GFP_HARDWALL"},	\
	{(unsigned long)__GFP_THISNODE,		"__GFP_THISNODE"},	\
	{(unsigned long)__GFP_RECLAIMABLE,	"__GFP_RECLAIMABLE"},	\
	{(unsigned long)__GFP_MOVABLE,		"__GFP_MOVABLE"},	\
	{(unsigned long)__GFP_ACCOUNT,		"__GFP_ACCOUNT"},	\
	{(unsigned long)__GFP_WRITE,		"__GFP_WRITE"},		\
	{(unsigned long)__GFP_RECLAIM,		"__GFP_RECLAIM"},	\
	{(unsigned long)__GFP_DIRECT_RECLAIM,	"__GFP_DIRECT_RECLAIM"},\
	{(unsigned long)__GFP_KSWAPD_RECLAIM,	"__GFP_KSWAPD_RECLAIM"}\

#define show_gfp_flags(flags)						\
	(flags) ? __print_flags(flags, "|",				\
	__def_gfpflag_names						\
	) : "none"

#ifdef CONFIG_MMU
#define IF_HAVE_PG_MLOCK(flag,string) ,{1UL << flag, string}
#else
#define IF_HAVE_PG_MLOCK(flag,string)
#endif

#ifdef CONFIG_ARCH_USES_PG_UNCACHED
#define IF_HAVE_PG_UNCACHED(flag,string) ,{1UL << flag, string}
#else
#define IF_HAVE_PG_UNCACHED(flag,string)
#endif

#ifdef CONFIG_MEMORY_FAILURE
#define IF_HAVE_PG_HWPOISON(flag,string) ,{1UL << flag, string}
#else
#define IF_HAVE_PG_HWPOISON(flag,string)
#endif

#if defined(CONFIG_IDLE_PAGE_TRACKING) && defined(CONFIG_64BIT)
#define IF_HAVE_PG_IDLE(flag,string) ,{1UL << flag, string}
#else
#define IF_HAVE_PG_IDLE(flag,string)
#endif

#define __def_pageflag_names						\
	{1UL << PG_locked,		"locked"	},		\
	{1UL << PG_waiters,		"waiters"	},		\
	{1UL << PG_error,		"error"		},		\
	{1UL << PG_referenced,		"referenced"	},		\
	{1UL << PG_uptodate,		"uptodate"	},		\
	{1UL << PG_dirty,		"dirty"		},		\
	{1UL << PG_lru,			"lru"		},		\
	{1UL << PG_active,		"active"	},		\
	{1UL << PG_slab,		"slab"		},		\
	{1UL << PG_owner_priv_1,	"owner_priv_1"	},		\
	{1UL << PG_arch_1,		"arch_1"	},		\
	{1UL << PG_reserved,		"reserved"	},		\
	{1UL << PG_private,		"private"	},		\
	{1UL << PG_private_2,		"private_2"	},		\
	{1UL << PG_writeback,		"writeback"	},		\
	{1UL << PG_head,		"head"		},		\
	{1UL << PG_mappedtodisk,	"mappedtodisk"	},		\
	{1UL << PG_reclaim,		"reclaim"	},		\
	{1UL << PG_swapbacked,		"swapbacked"	},		\
	{1UL << PG_unevictable,		"unevictable"	}		\
IF_HAVE_PG_MLOCK(PG_mlocked,		"mlocked"	)		\
IF_HAVE_PG_UNCACHED(PG_uncached,	"uncached"	)		\
IF_HAVE_PG_HWPOISON(PG_hwpoison,	"hwpoison"	)		\
IF_HAVE_PG_IDLE(PG_young,		"young"		)		\
IF_HAVE_PG_IDLE(PG_idle,		"idle"		)

const struct trace_print_flags pageflag_names[] = {
	__def_pageflag_names,
	{0, NULL}
};

static struct seq_buf sbuf;

static char *show_page_flags(unsigned long flags)
{
	int i;
	const struct trace_print_flags *tp;
	struct seq_buf *s = &sbuf;

	/* clear first */
	seq_buf_clear(s);
	memset(s->buffer, 0, s->size);

	for (i = 0; i < ARRAY_SIZE(pageflag_names); i++) {
		tp = &pageflag_names[i];
		if (tp->mask & flags) {
			//seq_buf_putmem(&sbuf, tp->name, sizeof(tp->name));
			/* Add the delim */
			if (s->len) {
				memcpy(s->buffer + s->len, "|", 1);
				s->len += 1;
			}

			memcpy(s->buffer + s->len, tp->name, strlen(tp->name));
			s->len += strlen(tp->name);
		}
	}
	return sbuf.buffer;
}

#define TEST1_FUNC_NAME	"__isolate_lru_page"
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

	printk("[%s] page : %lx, mode: %d flags: %.16lx(%s), map count: %d, page count: %d, (%d, %d)", TEST1_FUNC_NAME,
		(unsigned long)page, mode, page->flags, show_page_flags(page->flags),
		page_mapcount(page), page_count(page), PG_reclaim, __NR_PAGEFLAGS);
	return 0;
}

static void kp_post_handler(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	//log();
}

static struct kprobe kp = {
	.symbol_name = TEST1_FUNC_NAME,
	.pre_handler = kp_pre_handler,
	.post_handler = kp_post_handler
};

/* -------------------------------------- page_referenced_one() -----------*/
struct page_referenced_arg {
	int mapcount;
	int referenced;
	unsigned long vm_flags;
	struct mem_cgroup *memcg;
};

#define TEST2_FUNC_NAME	"page_referenced_one"

struct my_data {
	void *p;
	struct page *page;
};

static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct page *page;
	struct vm_area_struct *vma;
	unsigned long address;
	struct page_referenced_arg *arg;
	unsigned long pfn;
	struct my_data *data;

	data = (struct my_data *)ri->data;
	/*
	 * @di is the first parameter, @si is the second.
	 *
	 * log("%lx, 2:%lx", regs->di, regs->si);
	 */
	//dump_stack();
	page = (struct page*)(regs->di);
	vma = (struct vm_area_struct *)regs->si;
	address = regs->dx;
	arg = (struct page_referenced_arg *)regs->cx;

	pfn = (unsigned long)page_to_pfn(page);

	if (PageAnon(page))
	printk("[%s] pfn:%#.16lx(%d,%d), vma: %lx(%lx), address:%.16lx, mapcount:%.2d, referenced:%d\n",
		TEST2_FUNC_NAME, pfn, PageAnon(page), PageKsm(page),
		(unsigned long)vma, vma->vm_flags, address, arg->mapcount, arg->referenced);

	/* keep it here */
	data->p = arg;
	data->page = page;
	return 0;
}

static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct my_data *data = (struct my_data *)ri->data;
	struct page_referenced_arg *arg = data->p;
	struct page *page = data->page;

	if (PageAnon(page))
	printk("[%s post] mapcount:%.2d, referenced:%d\n",
		TEST2_FUNC_NAME, arg->mapcount, arg->referenced);
	return 0;
}

static struct kretprobe my_kretprobe = {
	.handler		= ret_handler,
	.entry_handler		= entry_handler,
	.data_size		= sizeof(struct my_data),
	/* Probe up to 20 instances concurrently. */
	.maxactive		= 20
};

/* -------------------------------------- check_pte() -----------*/
#define TEST3_FUNC_NAME	"check_pte"

static int kp_pre_handler3(struct kprobe *p, struct pt_regs *regs)
{
	/*
	 * @di is the first parameter, @si is the second.
	 *
	 * log("%lx, 2:%lx", regs->di, regs->si);
	 */
	struct page_vma_mapped_walk *pvmw = (struct page_vma_mapped_walk *)(regs->di);
	pte_t pte;

	if (PageAnon(pvmw->page)) {
		pte = *pvmw->pte;
		printk("[%s] pfn: %#.16lx , PTE: %#lx\n", p->symbol_name, page_to_pfn(pvmw->page),
				*((unsigned long *)&pte));
	}

	return 0;
}


static struct kprobe kp3 = {
	.symbol_name = TEST3_FUNC_NAME,
	.pre_handler = kp_pre_handler3,
};

/* -------------------------------------- end here  -----------*/

static int test_isolate_lru_page;
static int test_page_referenced_one = 1;
static int test_check_pte = 1;

static int __init mm_study(void)
{
	int ret = 0;

	seq_buf_init(&sbuf, kmalloc(PAGE_SIZE, GFP_KERNEL), PAGE_SIZE);
	if (!sbuf.buffer)
		return -ENOMEM;

	if (test_isolate_lru_page) {
		ret = register_kprobe(&kp);
		if (ret < 0) {
			pr_err("register_kprobe returned %d\n", ret);
			return ret;
		}
	}

	if (test_page_referenced_one) {
		/*
		ret = register_kprobe(&kp2);
		if (ret < 0) {
			pr_err("register_kprobe returned %d\n", ret);
			return ret;
		}
		*/
		my_kretprobe.kp.symbol_name = TEST2_FUNC_NAME;
		ret = register_kretprobe(&my_kretprobe);
	}

	if (test_check_pte) {
		ret = register_kprobe(&kp3);
		if (ret < 0) {
			pr_err("register_kprobe returned %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static void __exit mm_study_exit(void)
{
	log("---------------- bye world ---------------------");
	kfree(sbuf.buffer);
	sbuf.buffer = NULL;
	if (test_isolate_lru_page)
		unregister_kprobe(&kp);
	if (test_page_referenced_one) {
		//unregister_kprobe(&kp2);
		unregister_kretprobe(&my_kretprobe);
	}
	if (test_check_pte)
		unregister_kprobe(&kp3);
}

module_init(mm_study);
module_exit(mm_study_exit);

MODULE_AUTHOR("Huang Shijie");
MODULE_LICENSE("GPL v2");
