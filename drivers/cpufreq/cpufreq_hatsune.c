#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernel.h>  // Tambahkan header ini

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static DEFINE_PER_CPU(unsigned int, cpu_is_managed);
static DEFINE_MUTEX(userspace_mutex);

static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq)
{
    int ret = -EINVAL;
    unsigned int *setspeed = policy->governor_data;

    pr_debug("cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);

    mutex_lock(&userspace_mutex);
    if (!per_cpu(cpu_is_managed, policy->cpu))
        goto err;

    *setspeed = freq;

    pr_info("Clearing RAM and cache...\n");
    sync();
    drop_caches();
    pr_info("RAM and cache cleared\n");

    ret = __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

err:
    mutex_unlock(&userspace_mutex);
    return ret;
}

static ssize_t show_speed(struct cpufreq_policy *policy, char *buf)
{
    return sprintf(buf, "%u\n", policy->cur);
}

static int cpufreq_userspace_policy_init(struct cpufreq_policy *policy)
{
    unsigned int *setspeed;

    setspeed = kzalloc(sizeof(*setspeed), GFP_KERNEL);
    if (!setspeed)
        return -ENOMEM;

    policy->governor_data = setspeed;
    return 0;
}

static void cpufreq_userspace_policy_exit(struct cpufreq_policy *policy)
{
    mutex_lock(&userspace_mutex);
    kfree(policy->governor_data);
    policy->governor_data = NULL;
    mutex_unlock(&userspace_mutex);
}

static int cpufreq_userspace_policy_start(struct cpufreq_policy *policy)
{
    unsigned int *setspeed = policy->governor_data;

    BUG_ON(!policy->cur);
    pr_debug("started managing cpu %u\n", policy->cpu);

    mutex_lock(&userspace_mutex);
    per_cpu(cpu_is_managed, policy->cpu) = 1;
    *setspeed = policy->cur;
    mutex_unlock(&userspace_mutex);
    return 0;
}

static void cpufreq_userspace_policy_stop(struct cpufreq_policy *policy)
{
    unsigned int *setspeed = policy->governor_data;

    pr_debug("managing cpu %u stopped\n", policy->cpu);

    mutex_lock(&userspace_mutex);
    per_cpu(cpu_is_managed, policy->cpu) = 0;
    *setspeed = 0;
    mutex_unlock(&userspace_mutex);
}

static int my_init_function(void)
{
    return cpufreq_register_governor(&cpufreq_gov_hatsune_init);
}

static void my_exit_function(void)
{
    cpufreq_unregister_governor(&cpufreq_gov_hatsune_init);
}

MODULE_AUTHOR("Frostleaft07 <zx7unknow@gmail.com>");
MODULE_DESCRIPTION("CPUfreq policy governor 'hatsune' (Clear RAM and Cache)");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_HATSUNE
struct cpufreq_governor *cpufreq_default_governor(void)
{
    return &cpufreq_gov_hatsune;
}
fs_initcall(my_init_function);
#else
module_init(my_init_function);
module_exit(my_exit_function);
#endif

