#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

/*
 * Step 1 — probe/remove skeleton.
 *
 * This is the minimum needed to match the DT node and confirm
 * the driver is loading. No GPIO yet — just a dmesg message.
 *
 * When the overlay is applied, the kernel sees:
 *   compatible = "myvendor,mydev"
 * and calls mydev_led_probe() with the matched device.
 */

static int mydev_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const char *label;

	/* read the "label" string property from the DT node */
	if (of_property_read_string(dev->of_node, "label", &label))
		label = "unknown";

	dev_info(dev, "mydev-led probed! label = %s\n", label);
	return 0;
}

static int mydev_led_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "mydev-led removed\n");
	return 0;
}

/* must match compatible string in 01-dt-overlay.dts exactly */
static const struct of_device_id mydev_led_of_match[] = {
	{ .compatible = "myvendor,mydev" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mydev_led_of_match);

static struct platform_driver mydev_led_driver = {
	.probe  = mydev_led_probe,
	.remove = mydev_led_remove,
	.driver = {
		.name           = "mydev-led",
		.of_match_table = mydev_led_of_match,
	},
};
module_platform_driver(mydev_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hau Nguyen");
MODULE_DESCRIPTION("Custom DT-probed LED driver for Raspberry Pi 4");
