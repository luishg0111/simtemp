#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "nxp_simtemp.h"

#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"

/* Internal structure */
struct nxp_simtemp_data{
    int temp_mC;        /* Current temperature in mili c */
    int threshold_mC;   /* Alert threshold */
    int sampling_ms;    /* Sampling interval */
    struct miscdevice miscdev;
};

static struct nxp_simtemp_data *g_dev;

/* File operations */
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    /* TODO: just dummy read, to be implemented */
    const char *msg = "SimTemp: dummy date\n";
    return simple_read_from_buffer(buf, count, ppos, msg, strlen(msg));
}

static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .read = simtemp_read,
};

/* Platform dirver probe/remove */
static int nxp_simtemp_probe(struct platform_device *pdev)
{
    int ret;

    pr_info("%s: probing virtual sensor\n", DRIVER_NAME);

    g_dev = devm_kzalloc(&pdev->dev, sizeof(*g_dev), GFP_KERNEL);
    if (!g_dev)
    {
        return -ENOMEM;
    }

    g_dev->temp_mC = 25000;
    g_dev->threshold_mC = 40000;
    g_dev->sampling_ms = 100;

    g_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    g_dev->miscdev.name = DEVICE_NAME;
    g_dev->miscdev.fops = &simtemp_fops;

    ret = misc_register(&g_dev->miscdev);
    if (ret)
    {
        dev_err(&pdev->dev, "failed to register misc device\n");
        return ret;
    }

    pr_info("%s: module loaded successfully\n", DRIVER_NAME);
    return 0;
}

static void nxp_simtemp_remove(struct platform_device *pdev)
{
    pr_info("%s: removing virtual sensor\n", DRIVER_NAME);
    misc_deregister(&g_dev->miscdev);
}


/* Devide tree binding */
static const struct of_device_id nxp_simtemp_dt_ids[] = 
{
    {.compatible = "nxp,simtemp"},
    {/* sentinel */}
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_dt_ids);


/* Platform driver registration */

static struct platform_driver nxp_simtemp_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = nxp_simtemp_dt_ids,
    },
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
};

module_platform_driver(nxp_simtemp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luis Hernández <luishg0111@gmail.com>");
MODULE_DESCRIPTION("Virtual temperature sensor driver");
MODULE_VERSION("0.1");