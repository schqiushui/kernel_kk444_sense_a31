
/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/pinctrl/consumer.h>
#include<linux/delay.h>
#include <linux/slab.h>

#define FLT_DBG_LOG(fmt, ...) \
		printk(KERN_DEBUG "[FLT]SGM " fmt, ##__VA_ARGS__)
#define FLT_INFO_LOG(fmt, ...) \
		printk(KERN_INFO "[FLT]SGM " fmt, ##__VA_ARGS__)
#define FLT_ERR_LOG(fmt, ...) \
		printk(KERN_ERR "[FLT][ERR]SGM " fmt, ##__VA_ARGS__)

#define LED_GPIO_FLASH_DRIVER_NAME	"qcom,leds-gpio-flash"
#define LED_TRIGGER_DEFAULT		"none"

struct delayed_work sgm3780_delayed_work;
static struct workqueue_struct *sgm3780_work_queue;

void led_gpio_flash(void);
void led_gpio_torch(void);
static void flashlight_turn_off(void);


struct led_gpio_flash_data {
	int flash_en;
	int flash_now;
	int brightness;
	int flash_sw_timeout;
	struct led_classdev cdev;
	struct pinctrl_state *gpio_state_default;
};

static struct led_gpio_flash_data *this_sgm3780;

static void led_gpio_brightness_set(struct led_classdev *led_cdev,
				    enum led_brightness value)
{
	struct led_gpio_flash_data *flash_led =
		container_of(led_cdev, struct led_gpio_flash_data, cdev);
	int brightness = value;

	FLT_INFO_LOG("%s, brightness = %d\n", __func__, value);
	if (brightness > LED_HALF) {
		led_gpio_flash();
	} else if (brightness > LED_OFF) {
		led_gpio_torch();
	} else {
		flashlight_turn_off();
	}
	flash_led->cdev.brightness = brightness;
}

void led_gpio_torch()
{
	int rc = 0;
	FLT_INFO_LOG("%s\n", __func__);
	rc = gpio_direction_output(this_sgm3780->flash_en, 1);
	if (rc) {
		pr_err("%s: Failed to set gpio %d\n", __func__,
		       this_sgm3780->flash_en);
		goto err;
	}

	rc = gpio_direction_output(this_sgm3780->flash_now, 0);
	if (rc) {
		pr_err("%s: Failed to set gpio %d\n", __func__,
		       this_sgm3780->flash_now);
		goto err;
	}
err:
	return;
}

void led_gpio_flash()
{
	int rc = 0;
	FLT_INFO_LOG("%s\n", __func__);
	rc = gpio_direction_output(this_sgm3780->flash_en, 0);
	if (rc) {
		pr_err("%s: Failed to set gpio %d\n", __func__,
		       this_sgm3780->flash_en);
		goto err;
	}

	rc = gpio_direction_output(this_sgm3780->flash_now, 1);
	if (rc) {
		pr_err("%s: Failed to set gpio %d\n", __func__,
		       this_sgm3780->flash_now);
		goto err;
	}
	queue_delayed_work(sgm3780_work_queue, &sgm3780_delayed_work,
				   msecs_to_jiffies(this_sgm3780->flash_sw_timeout));
err:
	return;
}

static void flashlight_turn_off_work(struct work_struct *work)
{
	FLT_INFO_LOG("%s\n", __func__);
	flashlight_turn_off();
}

static void flashlight_turn_off()
{
	int rc = 0;
	FLT_INFO_LOG("%s\n", __func__);
	rc = gpio_direction_output(this_sgm3780->flash_en, 0);
	if (rc) {
		pr_err("%s: Failed to set gpio %d\n", __func__,
		       this_sgm3780->flash_en);
		goto err;
	}
	rc = gpio_direction_output(this_sgm3780->flash_now, 0);
	if (rc) {
		pr_err("%s: Failed to set gpio %d\n", __func__,
		       this_sgm3780->flash_now);
		goto err;
	}
err:
	return;

}

static enum led_brightness led_gpio_brightness_get(struct led_classdev
						   *led_cdev)
{
	struct led_gpio_flash_data *flash_led =
	    container_of(led_cdev, struct led_gpio_flash_data, cdev);
	char buf[50];
	FLT_INFO_LOG("%s\n", __func__);

	return snprintf(buf, 50, "%d\n", flash_led->brightness);
}

int led_gpio_flash_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *temp_str;
	struct led_gpio_flash_data *flash_led = NULL;
	struct device_node *node = pdev->dev.of_node;

	printk("[FLT]%s +\n", __func__);
	flash_led = devm_kzalloc(&pdev->dev, sizeof(struct led_gpio_flash_data),
				 GFP_KERNEL);
	if (flash_led == NULL) {
		dev_err(&pdev->dev, "%s:%d Unable to allocate memory\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	flash_led->cdev.default_trigger = LED_TRIGGER_DEFAULT;
	rc = of_property_read_string(node, "linux,default-trigger", &temp_str);
	if (!rc)
		flash_led->cdev.default_trigger = temp_str;

	flash_led->flash_en = of_get_named_gpio(node, "qcom,flash-en", 0);
	if (flash_led->flash_en < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed. rc =  %d\n",
			"flash-en", node->full_name, flash_led->flash_en);
		goto error;
	} else {
		rc = gpio_request(flash_led->flash_en, "FLASH_EN");
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Failed to request gpio %d,rc = %d\n",
				__func__, flash_led->flash_en, rc);

			goto error;
		}
	}

	flash_led->flash_now = of_get_named_gpio(node, "qcom,flash-now", 0);
	if (flash_led->flash_now < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed. rc =  %d\n",
			"flash-now", node->full_name, flash_led->flash_now);
		goto error;
	} else {
		rc = gpio_request(flash_led->flash_now, "FLASH_NOW");
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Failed to request gpio %d,rc = %d\n",
				__func__, flash_led->flash_now, rc);
			goto error;
		}
	}

	rc = of_property_read_string(node, "linux,name", &flash_led->cdev.name);
	if (rc) {
		dev_err(&pdev->dev, "%s: Failed to read linux name. rc = %d\n",
			__func__, rc);
		goto error;
	}

	rc = of_property_read_u32(node, "qcom,flash_duration_ms", &flash_led->flash_sw_timeout);
	if (rc) {
		dev_err(&pdev->dev, "%s: Failed to read flash_duration_ms. rc = %d\n",
			__func__, rc);
		goto error;
	}
	INIT_DELAYED_WORK(&sgm3780_delayed_work, flashlight_turn_off_work);
	sgm3780_work_queue = create_singlethread_workqueue("sgm3780_wq");
	if (!sgm3780_work_queue)
		goto err_create_sgm3780_work_queue;
	platform_set_drvdata(pdev, flash_led);
	flash_led->cdev.max_brightness = LED_FULL;
	flash_led->cdev.brightness_set = led_gpio_brightness_set;
	flash_led->cdev.brightness_get = led_gpio_brightness_get;
	this_sgm3780 = flash_led;
	rc = led_classdev_register(&pdev->dev, &flash_led->cdev);
	if (rc) {
		dev_err(&pdev->dev, "%s: Failed to register led dev. rc = %d\n",
			__func__, rc);
		goto error;
	}
	pr_err("%s:probe successfully!\n", __func__);
	return 0;
err_create_sgm3780_work_queue:
	kfree(flash_led);

error:
	devm_kfree(&pdev->dev, flash_led);
	return rc;
}

int led_gpio_flash_remove(struct platform_device *pdev)
{
	struct led_gpio_flash_data *flash_led =
	    (struct led_gpio_flash_data *)platform_get_drvdata(pdev);
	FLT_INFO_LOG("%s\n", __func__);
	led_classdev_unregister(&flash_led->cdev);
	devm_kfree(&pdev->dev, flash_led);
	return 0;
}

static struct of_device_id led_gpio_flash_of_match[] = {
	{.compatible = LED_GPIO_FLASH_DRIVER_NAME,},
	{},
};

static struct platform_driver led_gpio_flash_driver = {
	.probe = led_gpio_flash_probe,
	.remove = led_gpio_flash_remove,
	.driver = {
		   .name = LED_GPIO_FLASH_DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = led_gpio_flash_of_match,
		   }
};

static int __init led_gpio_flash_init(void)
{
	return platform_driver_register(&led_gpio_flash_driver);
}

static void __exit led_gpio_flash_exit(void)
{
	return platform_driver_unregister(&led_gpio_flash_driver);
}

late_initcall(led_gpio_flash_init);
module_exit(led_gpio_flash_exit);

MODULE_DESCRIPTION("QCOM GPIO LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-msm-gpio-flash");
