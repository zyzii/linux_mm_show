#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/module.h>
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

	printk("[%s] page : %lx, mode: %d flags: %.16lx(%s), map count: %d, page count: %d, (%d, %d)", func_name,
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
	.symbol_name = func_name,
	.pre_handler = kp_pre_handler,
	.post_handler = kp_post_handler
};

static int __init mm_study(void)
{
	int ret = 0;

	seq_buf_init(&sbuf, kmalloc(PAGE_SIZE, GFP_KERNEL), PAGE_SIZE);
	if (!sbuf.buffer)
		return -ENOMEM;

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
	kfree(sbuf.buffer);
	sbuf.buffer = NULL;
	unregister_kprobe(&kp);
}

module_init(mm_study);
module_exit(mm_study_exit);

MODULE_AUTHOR("Huang Shijie");
MODULE_LICENSE("GPL v2");
