#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>	/* devm_gpiod_get, gpiod_set_value */
#include <linux/sysfs.h>
#include <linux/device.h>

/*
 * Step 2 — GPIO control + sysfs toggle.
 *
 * New in this step:
 *   - Request the GPIO pin declared as "ctrl-gpios" in the DT node
 *   - Create a sysfs file "led" under the device's sysfs directory
 *   - Writing "1" turns the LED on, writing "0" turns it off
 *
 * After loading the module on the Pi:
 *   echo 1 > /sys/bus/platform/devices/mydev@0/led   # LED on
 *   echo 0 > /sys/bus/platform/devices/mydev@0/led   # LED off
 */

/* per-device state — one instance allocated per probed device */
struct mydev_led {
	struct gpio_desc *gpio;		/* handle to the GPIO pin */
	const char       *label;	/* label string from DT   */
};

/* ------------------------------------------------------------------ */
/* sysfs attribute                                                      */
/* ------------------------------------------------------------------ */

/*
 * show — called when userspace reads the sysfs file.
 * Returns the current GPIO value ("0\n" or "1\n").
 */
static ssize_t led_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mydev_led *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->gpio));
}

/*
 * store — called when userspace writes to the sysfs file.
 * Accepts "0" or "1".
 */
static ssize_t led_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mydev_led *priv = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	gpiod_set_value(priv->gpio, val);
	return count;
}

/* ties led_show and led_store to a file named "led", rw for owner+group */
static DEVICE_ATTR_RW(led);

/* ------------------------------------------------------------------ */
/* probe / remove                                                       */
/* ------------------------------------------------------------------ */

static int mydev_led_probe(struct platform_device *pdev)
{
	struct device    *dev = &pdev->dev;
	struct mydev_led *priv;
	int ret;

	/* allocate private data — devm frees it automatically on remove */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* read the "label" property from the DT node */
	if (of_property_read_string(dev->of_node, "label", &priv->label))
		priv->label = "unknown";

	/*
	 * Request the GPIO declared as "ctrl-gpios" in the DT node.
	 *   "ctrl"         — matches "ctrl-gpios" property name
	 *   GPIOD_OUT_LOW  — configure as output, initial state low (LED off)
	 */
	priv->gpio = devm_gpiod_get(dev, "ctrl", GPIOD_OUT_LOW);
	if (IS_ERR(priv->gpio)) {
		dev_err(dev, "failed to get ctrl-gpios: %ld\n",
			PTR_ERR(priv->gpio));
		return PTR_ERR(priv->gpio);
	}

	/* store priv so sysfs callbacks can retrieve it via dev_get_drvdata */
	dev_set_drvdata(dev, priv);

	/* create the "led" sysfs file under this device's directory */
	ret = device_create_file(dev, &dev_attr_led);
	if (ret) {
		dev_err(dev, "failed to create sysfs file: %d\n", ret);
		return ret;
	}

	dev_info(dev, "mydev-led probed! label=%s gpio=%d\n",
		 priv->label, desc_to_gpio(priv->gpio));
	return 0;
}

static int mydev_led_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_led);
	dev_info(&pdev->dev, "mydev-led removed\n");
	return 0;
}

/* ------------------------------------------------------------------ */
/* driver registration                                                  */
/* ------------------------------------------------------------------ */

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
