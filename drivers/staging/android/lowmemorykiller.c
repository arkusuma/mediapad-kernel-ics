/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_adj values will get killed. Specify the
 * minimum oom_adj values in /sys/module/lowmemorykiller/parameters/adj and the
 * number of free pages in /sys/module/lowmemorykiller/parameters/minfree. Both
 * files take a comma separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill processes
 * with a oom_adj value of 8 or higher when the free memory drops below 4096 pages
 * and kill processes with a oom_adj value of 0 or higher when the free memory
 * drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/memory.h>
#include <linux/memory_hotplug.h>

static uint32_t lowmem_debug_level = 2;
static int lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static size_t lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;

static unsigned int offlining;
static struct task_struct *lowmem_deathpending;
//static unsigned long lowmem_deathpending_timeout;
static DEFINE_SPINLOCK(lowmem_deathpending_lock);
#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			printk(x);			\
	} while (0)

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_nb = {
	.notifier_call	= task_notify_func,
};

static void task_free_fn(struct work_struct *work)
{
        unsigned long flags;

        task_free_unregister(&task_nb);
        spin_lock_irqsave(&lowmem_deathpending_lock, flags);
        lowmem_deathpending = NULL;
        spin_unlock_irqrestore(&lowmem_deathpending_lock, flags);
}
static DECLARE_WORK(task_free_work, task_free_fn);

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;
	//if (task == lowmem_deathpending)
	//	lowmem_deathpending = NULL;
	if (task == lowmem_deathpending) {
            schedule_work(&task_free_work);
        }

	return NOTIFY_OK;
}

#ifdef CONFIG_MEMORY_HOTPLUG
static int lmk_hotplug_callback(struct notifier_block *self,
				unsigned long cmd, void *data)
{
	switch (cmd) {
	/* Don't care LMK cases */
	case MEM_ONLINE:
	case MEM_OFFLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
	case MEM_GOING_ONLINE:
		offlining = 0;
		lowmem_print(4, "lmk in normal mode\n");
		break;
	/* LMK should account for movable zone */
	case MEM_GOING_OFFLINE:
		offlining = 1;
		lowmem_print(4, "lmk in hotplug mode\n");
		break;
	}
	return NOTIFY_DONE;
}
#endif



static int lowmem_shrink(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *p;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int i;
	int min_adj = OOM_ADJUST_MAX + 1;
	int selected_tasksize = 0;
	int selected_oom_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES);
	int other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM);
	struct zone *zone;
	unsigned long flags;

	if (offlining) {
		/* Discount all free space in the section being offlined */
		for_each_zone(zone) {
			 if (zone_idx(zone) == ZONE_MOVABLE) {
				other_free -= zone_page_state(zone,
						NR_FREE_PAGES);
				lowmem_print(4, "lowmem_shrink discounted "
					"%lu pages in movable zone\n",
					zone_page_state(zone, NR_FREE_PAGES));
			}
		}
	}
	/*
	 * If we already have a death outstanding, then
	 * bail out right away; indicating to vmscan
	 * that we have nothing further to offer on
	 * this pass.
	 *
	 */
	//if (lowmem_deathpending &&
	//    time_before_eq(jiffies, lowmem_deathpending_timeout))
	if (lowmem_deathpending)
		return 0;

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		if (other_free < lowmem_minfree[i] &&
		    other_file < lowmem_minfree[i]) {
			min_adj = lowmem_adj[i];
			break;
		}
	}
	if (sc->nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %lu, %x, ofree %d %d, ma %d\n",
			     sc->nr_to_scan, sc->gfp_mask, other_free, other_file,
			     min_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (sc->nr_to_scan <= 0 || min_adj == OOM_ADJUST_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %lu, %x, return %d\n",
			     sc->nr_to_scan, sc->gfp_mask, rem);
		return rem;
	}
	selected_oom_adj = min_adj;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		struct mm_struct *mm;
		struct signal_struct *sig;
		int oom_adj;

		task_lock(p);
		mm = p->mm;
		sig = p->signal;
		if (!mm || !sig) {
			task_unlock(p);
			continue;
		}
		oom_adj = sig->oom_adj;
		if (oom_adj < min_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_adj < selected_oom_adj)
				continue;
			if (oom_adj == selected_oom_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_adj = oom_adj;
		lowmem_print(2, "select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_adj, tasksize);
	}
	if (selected) {
		spin_lock_irqsave(&lowmem_deathpending_lock, flags);
             if (!lowmem_deathpending) {
               lowmem_print(1, "send sigkill to %d (%s), adj %d, size %d\n",
               selected->pid, selected->comm,
               selected_oom_adj, selected_tasksize);

		lowmem_deathpending = selected;
		//lowmem_deathpending_timeout = jiffies + HZ;
		task_free_register(&task_nb);
		force_sig(SIGKILL, selected);
		rem -= selected_tasksize;
		}
		spin_unlock_irqrestore(&lowmem_deathpending_lock, flags);
	}
	lowmem_print(4, "lowmem_shrink %lu, %x, return %d\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	read_unlock(&tasklist_lock);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

#if CONFIG_MSM_KGSL_VM_THRESHOLD > 0

static void lowmem_vm_shrinker(int largest, int rss_threshold)
{
       struct task_struct *p;
       struct task_struct *selected = NULL;
       int vmsize, rssize;
       int min_adj, min_large_adj;
       int selected_vmsize = 0;
       int selected_oom_adj;
       int array_size = ARRAY_SIZE(lowmem_adj);
       unsigned long flags;

       /*
        * If we already have a death outstanding, then
        * bail out right away; indicating to vmscan
        * that we have nothing further to offer on
        * this pass.
        *
        */
       if (lowmem_deathpending)
               return;

       if (lowmem_adj_size < array_size)
               array_size = lowmem_adj_size;
       if (lowmem_minfree_size < array_size)
               array_size = lowmem_minfree_size;

       min_adj = lowmem_adj[array_size - 2];  /* lock onto cached processes only */
       min_large_adj = lowmem_adj[array_size - 3];  /* Minimum priority for large processes */

       lowmem_print(3, "lowmem_vm_shrink ma %d, large ma %d, largest %d, rss_threshold=%d\n",
                     min_adj, min_large_adj, largest, rss_threshold);

       selected_oom_adj = min_adj;
       read_lock(&tasklist_lock);
       for_each_process(p) {
               struct mm_struct *mm;
               struct signal_struct *sig;
               int oom_adj;

               task_lock(p);
               mm = p->mm;
               sig = p->signal;
               if (!mm || !sig) {
                       task_unlock(p);
                       continue;
               }
               oom_adj = sig->oom_adj;
               vmsize = get_mm_hiwater_vm(mm);
                rssize = get_mm_rss(mm) * PAGE_SIZE;
               task_unlock(p);

               if (vmsize <= 0)
                       continue;

                /* Only look at cached processes */
                if (oom_adj < min_adj) {
                    /* Is this a very large home process in the background? */
                    if ((oom_adj > min_large_adj) && (rssize >= rss_threshold)) {
                        selected = p;
                        selected_vmsize = vmsize;
                       selected_oom_adj = oom_adj;
                        lowmem_print(2, "lowmem_shrink override %d (%s), adj %d, vm size %d, rs size %d to kill\n" ,p->pid, p->comm, oom_adj, vmsize, rssize);
                        break;
                    }

                    continue;
                }

                /* Is this process a better fit than last selected? */
               if (selected) {
                       if (oom_adj < selected_oom_adj)
                               continue;

                        /* If looking for largest, ignore priority */
                       if ((largest || (oom_adj == selected_oom_adj)) &&
                           (vmsize <= selected_vmsize))
                               continue;
               }

               selected = p;
               selected_vmsize = vmsize;

               if (largest == 0)  /* Do not filter by priority if searching for largest */
                       selected_oom_adj = oom_adj;

               lowmem_print(2, "lowmem_shrink select %d (%s), adj %d, vm size %d, rs size %d to kill\n",
                            p->pid, p->comm, oom_adj, vmsize, rssize);
       }
       if (selected) {
               spin_lock_irqsave(&lowmem_deathpending_lock, flags);
               if (!lowmem_deathpending) {
                       lowmem_print(1,
                               "lowmem_shrink send sigkill to %d (%s), adj %d, vm size %d\n",
                               selected->pid, selected->comm,
                               selected_oom_adj, selected_vmsize);
                       lowmem_deathpending = selected;
                       task_free_register(&task_nb);
                       force_sig(SIGKILL, selected);
               }
               spin_unlock_irqrestore(&lowmem_deathpending_lock, flags);
       }
       lowmem_print(4, "lowmem_vm_shrink, saved %d\n", selected_vmsize);
       read_unlock(&tasklist_lock);
       return;
}
#endif

static int __init lowmem_init(void)
{
	//task_free_register(&task_nb);
#if CONFIG_MSM_KGSL_VM_THRESHOLD > 0
       extern void kgsl_register_shrinker(void (*shrink)(int largest, int threshold));

       kgsl_register_shrinker(lowmem_vm_shrinker);
#endif
	register_shrinker(&lowmem_shrinker);
#ifdef CONFIG_MEMORY_HOTPLUG
	hotplug_memory_notifier(lmk_hotplug_callback, 0);
#endif
	return 0;
}

static void __exit lowmem_exit(void)
{
#if CONFIG_MSM_KGSL_VM_THRESHOLD > 0
       extern void kgsl_unregister_shrinker(void);

       kgsl_unregister_shrinker();
#endif
	unregister_shrinker(&lowmem_shrinker);
	//task_free_unregister(&task_nb);
}

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
module_param_array_named(adj, lowmem_adj, int, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

