
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uio_driver.h>
#include <linux/isa.h>
#include <asm/io.h>

#define CRD_NAME "PCBird"
#define DEV_NAME "pcbird"

#ifdef CONFIG_ISA

#endif

MODULE_DESCRIPTION(CRD_NAME);
MODULE_AUTHOR("TW");
MODULE_LICENSE("GPL");

#define PCBIRD_CARDS 2

static long port[PCBIRD_CARDS] = { 0, 0};
static long irq[PCBIRD_CARDS] = { 0, 0};
static long enable[PCBIRD_CARDS] = { 1, 0};

module_param_array(port, long, NULL, 0444);
MODULE_PARM_DESC(port, "Port # for " CRD_NAME " driver.");
module_param_array(irq, long, NULL, 0444);
MODULE_PARM_DESC(irq, "IRQ # for " CRD_NAME " driver.");

static irqreturn_t pcbird_handler(int irq, struct uio_info *dev_info)
{
#if 0
	void __iomem *plx_intscr = dev_info->mem[0].internal_addr
					+ PLX9030_INTCSR;

	if ((ioread8(plx_intscr) & INT1_ENABLED_AND_ACTIVE)
	    != INT1_ENABLED_AND_ACTIVE)
		return IRQ_NONE;

	/* Disable interrupt */
	iowrite8(ioread8(plx_intscr) & ~INTSCR_INT1_ENABLE, plx_intscr);
#endif

	return IRQ_HANDLED;
}

static int __devinit isa_pcbird_match(struct device *dev, unsigned int n)
{
	if (!enable[n])
		return 0;

	if (!port[n]) {
		printk(KERN_ERR "%s: please specify port\n", dev->bus_id);
		return 0;
	}

	if (!irq[n]) {
		printk(KERN_ERR "%s: please specify IRQ\n", dev->bus_id);
		return 0;
	}
	return 1;
}

static int __devinit isa_pcbird_probe(struct device *dev, unsigned int n)
{
	struct uio_info *info;

	info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->name = CRD_NAME;
    	info->version = "0.0.1";
	info->irq = irq[n];
	info->irq_flags = IRQF_DISABLED;
	info->handler = pcbird_handler;

	if (uio_register_device(dev, info))
		goto out_unmap;

	printk(KERN_INFO "PCBird: probe done [port = 0x%x, irq = %d].\n", port[n], irq[n]);
	dev_set_drvdata(dev, info);

	disable_irq(irq[n]);
	enable_irq(irq[n]);

	return 0;

out_unmap:
	printk(KERN_ERR "PCBird: probe failed.\n");
	kfree(info);
	return -ENODEV;


}

static int __devexit isa_pcbird_remove(struct device *dev, unsigned int n)
{
	struct uio_info *info = dev_get_drvdata(dev);

	uio_unregister_device(info);
	dev_set_drvdata(dev, NULL);
	kfree(info);
	return 0;
}

static struct isa_driver isa_pcbird_driver = {
	.match		= isa_pcbird_match,
	.probe		= isa_pcbird_probe,
	.remove		= __devexit_p(isa_pcbird_remove),

	.driver		= {
		.name	= DEV_NAME
	}
};

static int __init isa_pcbird_init(void)
{
	return isa_register_driver(&isa_pcbird_driver, PCBIRD_CARDS);
}

static void __exit isa_pcbird_exit(void)
{
	isa_unregister_driver(&isa_pcbird_driver);
}


module_init(isa_pcbird_init);
module_exit(isa_pcbird_exit);
