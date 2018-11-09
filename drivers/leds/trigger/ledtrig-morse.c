/*
 * LED morse Trigger
 *
 * Copyright (C) 2006 Atsushi Nemoto <anemo@mba.ocn.ne.jp>
 *
 * Based on Richard Purdie's ledtrig-timer.c and some arch's
 * CONFIG_morse code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/sched/loadavg.h>
#include <linux/leds.h>
#include <linux/reboot.h>
#include "../leds.h"

#define DEBUG 1

static char* msg = "sos";
/* '-' for dash, '.' for dot*/
static char cipher[6][26] = {
	{'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z'},
	{'.',  '-',  '-',  '-',  '.',  '.',  '-',  '.',  '.',  '.',  '-',  '.',  '-',  '-',  '-',  '.',  '-',  '.',  '.',  '-',  '.',  '.',  '.',  '-',  '-',  '-'},
	{'-',  '.',  '.',  '.',  '\0', '.',  '-',  '.',  '.',  '-',  '.',  '-',  '-',  '.',  '-',  '-',  '-',  '-',  '.',  '\0', '.',  '.',  '-',  '.',  '.',  '-'},
	{'\0', '.',  '-',  '.',  '\0', '-',  '.',  '.',  '\0', '-',  '-',  '.',  '\0', '\0', '-',  '-',  '.',  '.',  '.',  '\0', '-',  '.',  '-',  '.',  '-',  '.'},
	{'\0', '.',  '.',  '\0', '\0', '.',  '\0', '.',  '\0', '-',  '\0', '.',  '\0', '\0', '\0', '.',  '-',  '\0', '\0', '\0', '\0', '-',  '\0', '-',  '-',  '.'},
	{'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'}
};
static int idx = 0;
static char char_idx = 0;
static int sym_idx = 1;
/* phase = 0: light should be on
 * phase = 1: light should be off*/
static int phase = 0;
static unsigned long delay = 0;

static int panic_morses;

struct morse_trig_data {
	unsigned int phase;
	unsigned int period;
	struct timer_list timer;
	unsigned int invert;
};

void led_morse_function(unsigned long data) {
#if DEBUG
	printk("entered function\n");
#endif
	struct led_classdev *led_cdev = (struct led_classdev *) data;
	struct morse_trig_data *morse_data = led_cdev->trigger_data;
	unsigned long brightness = LED_OFF;
	//unsigned long delay = 0;

	if (unlikely(panic_morses)) {
		led_set_brightness_nosleep(led_cdev, LED_OFF);
		return;
	}

	if (test_and_clear_bit(LED_BLINK_BRIGHTNESS_CHANGE, &led_cdev->work_flags))
		led_cdev->blink_brightness = led_cdev->new_blink_brightness;

	switch (phase) {
	case 0:	
		printk("case 0\n");
		//printf("idx: %d\tchar_idx: %d\tsym_idx: %d\n", idx, char_idx, sym_idx);
		/* get the next character from the source message */
		char_idx = msg[idx];
#if DEBUG
		printk("current character is: %c\n", char_idx);
#endif

		/* if the character is a null terminator, wrap around to the beginning of the string */
		//if (char_idx == '\0') {
		//	//printf("found end of msg\n");
		//	idx = 0;
		//	char_idx = msg[idx];
		//}
		/* if the character is a space, wait for 7 units of time */
		if (char_idx == ' ' || char_idx == '\0') {
			if(char_idx == '\0') idx = 0;
			else idx++;
#if DEBUG
			printk("found space or nul terminator in msg. idx is: %d\n", idx);
#endif
			//delay = 4;
			delay = msecs_to_jiffies(100);
			phase = 1;
			sym_idx = 1;
			break;
		}
		/* change the uppercase or lower case ascii character to a number from 0-25*/
		else if (char_idx > 90) char_idx -= 97;
		else if (char_idx > 64) char_idx -= 65;

		/* get the current symbol from the cipher*/
		/* if the current symbol to be printed is a null terminator, we have finished printing symbols*/
		if (cipher[sym_idx][char_idx] == '\0') {
			//printf("found a null, end of character\n");
#if DEBUG
			printk("reached the end of the symbols\n");
#endif
			idx++;
			sym_idx = 1;
			//delay = 3;
			delay = msecs_to_jiffies(300);
		} else {
			/* else, set the LED */
			/* set LED brightness on */
			/* set the delay */
			//printf("%c\n", cipher[sym_idx][char_idx]);
			if(cipher[sym_idx][char_idx] == '.') {
				//delay = 1;
#if DEBUG
				printk("found dot, setting delay to 100\n");
#endif
				delay = msecs_to_jiffies(100);
			}
			if(cipher[sym_idx][char_idx] == '-') {
				//delay = 3;
#if DEBUG
				printk("found dash, setting delay to 300\n");
#endif
				delay = msecs_to_jiffies(300);
			}
			
			//brightness = 0;
			if (!morse_data->invert)
				brightness = led_cdev->blink_brightness;
#if DEBUG
			printk("changing phase to 1, incrementing sym_idx\n");
#endif
			sym_idx++;
			phase = 1;
		}
		break;
	case 1:
		/* set LED brightness off*/
#if DEBUG
		printk("case 0, changing brightness to off\n");
#endif
		phase = 0;
		if (morse_data->invert)
			brightness = led_cdev->blink_brightness;
		break;
		/* LED should be off
		 * set brightness and delay for the appropriate amount of time */
	}
	/* call the brightness function */
	//return delay;
	led_set_brightness_nosleep(led_cdev, brightness);
	mod_timer(&morse_data->timer, jiffies + delay);
}

static ssize_t led_invert_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;

	return sprintf(buf, "%u\n", morse_data->invert);
}

static ssize_t led_invert_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;
	unsigned long state;
	int ret;

	ret = kstrtoul(buf, 0, &state);
	if (ret)
		return ret;

	morse_data->invert = !!state;

	return size;
}

static DEVICE_ATTR(invert, 0644, led_invert_show, led_invert_store);

static void morse_trig_activate(struct led_classdev *led_cdev)
{
	struct morse_trig_data *morse_data;
	int rc;

	morse_data = kzalloc(sizeof(*morse_data), GFP_KERNEL);
	if (!morse_data)
		return;

	led_cdev->trigger_data = morse_data;
	rc = device_create_file(led_cdev->dev, &dev_attr_invert);
	if (rc) {
		kfree(led_cdev->trigger_data);
		return;
	}

	setup_timer(&morse_data->timer,
		    led_morse_function, (unsigned long) led_cdev);
	morse_data->phase = 0;
	if (!led_cdev->blink_brightness)
		led_cdev->blink_brightness = led_cdev->max_brightness;
	led_morse_function(morse_data->timer.data);
	set_bit(LED_BLINK_SW, &led_cdev->work_flags);
	led_cdev->activated = true;
}

static void morse_trig_deactivate(struct led_classdev *led_cdev)
{
	struct morse_trig_data *morse_data = led_cdev->trigger_data;

	if (led_cdev->activated) {
		del_timer_sync(&morse_data->timer);
		device_remove_file(led_cdev->dev, &dev_attr_invert);
		kfree(morse_data);
		clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
		led_cdev->activated = false;
	}
}

static struct led_trigger morse_led_trigger = {
	.name     = "morse",
	.activate = morse_trig_activate,
	.deactivate = morse_trig_deactivate,
};

static int morse_reboot_notifier(struct notifier_block *nb,
				     unsigned long code, void *unused)
{
	led_trigger_unregister(&morse_led_trigger);
	return NOTIFY_DONE;
}

static int morse_panic_notifier(struct notifier_block *nb,
				     unsigned long code, void *unused)
{
	panic_morses = 1;
	return NOTIFY_DONE;
}

static struct notifier_block morse_reboot_nb = {
	.notifier_call = morse_reboot_notifier,
};

static struct notifier_block morse_panic_nb = {
	.notifier_call = morse_panic_notifier,
};

static int __init morse_trig_init(void)
{
	int rc = led_trigger_register(&morse_led_trigger);

	if (!rc) {
		atomic_notifier_chain_register(&panic_notifier_list,
					       &morse_panic_nb);
		register_reboot_notifier(&morse_reboot_nb);
	}
	return rc;
}

static void __exit morse_trig_exit(void)
{
	unregister_reboot_notifier(&morse_reboot_nb);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &morse_panic_nb);
	led_trigger_unregister(&morse_led_trigger);
}

module_init(morse_trig_init);
module_exit(morse_trig_exit);

MODULE_AUTHOR("Nick Pugliese <nicholas.d.pugliese@gmail.com>");
MODULE_DESCRIPTION("morse LED trigger");
MODULE_LICENSE("GPL");
