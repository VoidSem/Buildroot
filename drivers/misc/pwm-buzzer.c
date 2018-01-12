/*
 * leelin
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/fs.h>  
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

//#define BUZZER_DEBUG
#ifdef BUZZER_DEBUG
	#define debug(fmt, args...) printk(fmt, ##args)
#else
	#define debug(fmt, args...) 
#endif

#define BUZZER_ENABLE	182
#define BUZZER_FREQENCY 183
#define BUZZER_DISABLE	184

#define DEV_NAME    "pwm-buzzer"

struct pwm_buzzer {
	struct pwm_device *pwm;
	unsigned long period;
};

//#define HZ_TO_NANOSECONDS(x) (1000000000UL/(x))
struct pwm_buzzer *privpwmRef=NULL;

static int buzzer_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int buzzer_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long buzzer_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
    if(privpwmRef == NULL)
    {
		debug("privpwmRef == NULL.\r\n");
        return -EINVAL;
    }

    if(privpwmRef->pwm == NULL)
    {
		debug("privpwmRef->pwm == NULL.\r\n");
        return -EINVAL;
    }
	if(arg > 8000 || arg < 0)
    {
		debug("out of range.\r\n");
        return -EINVAL;
    }
	debug("cmd: %d,", cmd);
	debug("   arg: %d,\r\n", arg);

    switch (cmd) {
	case BUZZER_ENABLE:
        pwm_enable(privpwmRef->pwm);
		debug("enable buzzer.\r\n");
		break;
    case BUZZER_FREQENCY:
		if(arg!=0)
		{
        	pwm_config(privpwmRef->pwm, 1000000000/arg/2, 1000000000/arg);
			debug("set buzzer pwm freqency.\r\n");
        	break;
		}
	case BUZZER_DISABLE:
        pwm_disable(privpwmRef->pwm);
		debug("disable buzzer.\r\n");
		break;
    default:
		debug("Unknown command(%d).\r\n",cmd);
        break;
    }

    return 0;
}

static struct file_operations buzzer_fops = {
    .owner              = THIS_MODULE,
    .unlocked_ioctl     = buzzer_ioctl,
    .open               = buzzer_open,
    .release            = buzzer_release,
};

static struct miscdevice buzzer_miscdev =
{
     .minor  = MISC_DYNAMIC_MINOR,
     .name   = DEV_NAME,
     .fops   = &buzzer_fops,
};

static int pwm_buzzer_probe(struct platform_device *pdev)
{
	unsigned long pwm_id = (unsigned long)dev_get_platdata(&pdev->dev);
	struct pwm_buzzer *buzzer;
	int error;
	debug("pwm_buzzer_probe:pwm_id: %d.\r\n", pwm_id);

	buzzer = kzalloc(sizeof(*buzzer), GFP_KERNEL);
	if (!buzzer)
		return -ENOMEM;

	buzzer->pwm = pwm_get(&pdev->dev, NULL);
	if (IS_ERR(buzzer->pwm)) {
		//debug("IS_ERR(buzzer->pwm):pwm_id: %d,\r\n", pwm_id);
		dev_dbg(&pdev->dev, "unable to request PWM, trying legacy API\n");
		buzzer->pwm = pwm_request(pwm_id, "pwm buzzer");
	}

	if (IS_ERR(buzzer->pwm)) {
		error = PTR_ERR(buzzer->pwm);
		//debug("PTR_ERR(buzzer->pwm):error: %d,\r\n", error);
		dev_err(&pdev->dev, "Failed to request pwm device: %d\n", error);
		goto err_free;
	}
	privpwmRef=buzzer;
	if(misc_register(&buzzer_miscdev)<0)
		goto err_pwm_free;
	printk("pwm_buzzer_probe pass.\n");
	return 0;

err_pwm_free:
	pwm_free(buzzer->pwm);
err_free:
	kfree(buzzer);

	return error;
}

static int pwm_buzzer_remove(struct platform_device *pdev)
{
	struct pwm_buzzer *buzzer = platform_get_drvdata(pdev);

	pwm_disable(buzzer->pwm);
	pwm_free(buzzer->pwm);
   	misc_deregister(&buzzer_miscdev);

	kfree(buzzer);

	return 0;
}

static int __maybe_unused pwm_buzzer_suspend(struct device *dev)
{
	struct pwm_buzzer *buzzer = dev_get_drvdata(dev);

	if (buzzer->period)
		pwm_disable(buzzer->pwm);

	return 0;
}

static int __maybe_unused pwm_buzzer_resume(struct device *dev)
{
	struct pwm_buzzer *buzzer = dev_get_drvdata(dev);

	if (buzzer->period) {
		pwm_config(buzzer->pwm, buzzer->period / 2, buzzer->period);
		pwm_enable(buzzer->pwm);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(pwm_buzzer_pm_ops,
			 pwm_buzzer_suspend, pwm_buzzer_resume);

#ifdef CONFIG_OF
static const struct of_device_id pwm_buzzer_match[] = {
	{ .compatible = "pwm-buzzer", },
	{ },
};
#endif

static struct platform_driver pwm_buzzer_driver = {
	.probe	= pwm_buzzer_probe,
	.remove = pwm_buzzer_remove,
	.driver = {
		.name	= "pwm-buzzer",
		.pm	= &pwm_buzzer_pm_ops,
		.of_match_table = of_match_ptr(pwm_buzzer_match),
	},
};
module_platform_driver(pwm_buzzer_driver);

MODULE_AUTHOR("Lee Lin <wensilin@gmail.com>");
MODULE_DESCRIPTION("PWM buzzer driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-buzzer");

