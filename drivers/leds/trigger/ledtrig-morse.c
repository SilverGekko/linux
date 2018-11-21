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
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/types.h>
//#include "../Char/dummy.c"
#define DEBUG 1

/* DUMMY STUFF HERE */
#include "dummy.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;

/* DUMMY STUFF END HERE */

static char* msg = "sos";
//file int
int fp;
/* '-' for dash, '.' for dot*/
static char cipher[7][36] = {
	{ 'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9' },
	{ '.',  '-',  '-',  '-',  '.',  '.',  '-',  '.',  '.',  '.',  '-',  '.',  '-',  '-',  '-',  '.',  '-',  '.',  '.',  '-',  '.',  '.',  '.',  '-',  '-',  '-',  '-',  '.',  '.',  '.',  '.',  '.',  '-',  '-',  '-',  '-' },
	{ '-',  '.',  '.',  '.',  '\0', '.',  '-',  '.',  '.',  '-',  '.',  '-',  '-',  '.',  '-',  '-',  '-',  '-',  '.',  '\0', '.',  '.',  '-',  '.',  '.',  '-',  '-',  '-',  '.',  '.',  '.',  '.',  '.',  '-',  '-',  '-' },
	{ '\0', '.',  '-',  '.',  '\0', '-',  '.',  '.',  '\0', '-',  '-',  '.',  '\0', '\0', '-',  '-',  '.',  '.',  '.',  '\0', '-',  '.',  '-',  '.',  '-',  '.',  '-',  '-',  '-',  '.',  '.',  '.',  '.',  '.',  '-',  '-' },
	{ '\0', '.',  '.',  '\0', '\0', '.',  '\0', '.',  '\0', '-',  '\0', '.',  '\0', '\0', '\0', '.',  '-',  '\0', '\0', '\0', '\0', '-',  '\0', '-',  '-',  '.',  '-',  '-',  '-',  '-',  '.',  '.',  '.',  '.',  '.',  '-' },
	{ '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '-',  '-',  '-',  '-',  '-',  '.',  '.',  '.',  '.',  '.' },
	{ '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' }
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
	unsigned int slower;
	unsigned int repeat;
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
			if (char_idx == '\0') {
				if(morse_data->repeat == 1)
				{
					//brightness = 0;
					//brightness = led_cdev->blink_brightness;
					delay = msecs_to_jiffies(300);
					phase = 0;
					break;
				}
				else {
					idx = 0;
				}
			}
			else
				idx++;
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
		else if (char_idx >= 97 && char_idx <= 122) char_idx -= 97;
		else if (char_idx >= 65 && char_idx <= 90) char_idx -= 65;
		//then it is a number number 
		else if (char_idx >= 48 && char_idx <= 57)
		{	
			char_idx -= 48 + 26;
		}
		else
		{
			idx++;
			delay = msecs_to_jiffies(100);
			phase = 1;
			sym_idx = 1;
			break;
		}
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
		}
		else {
			/* else, set the LED */
			/* set LED brightness on */
			/* set the delay */
			//printf("%c\n", cipher[sym_idx][char_idx]);
			if (cipher[sym_idx][char_idx] == '.') {
				//delay = 1;
				#if DEBUG
					printk("found dot, setting delay to 100\n");
				#endif
				delay = msecs_to_jiffies(100);
			}
			if (cipher[sym_idx][char_idx] == '-') {
				//delay = 3;
				#if DEBUG
					printk("found dash, setting delay to 300\n");
				#endif
				delay = msecs_to_jiffies(300);
			}

			brightness = 0;
			//if (!morse_data->slower)
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
		//this is important
		//if (morse_data->slower)
		//	brightness = led_cdev->blink_brightness;
		break;
		/* LED should be off
		* set brightness and delay for the appropriate amount of time */
	}
	/* call the brightness function */
	//return delay;
	led_set_brightness_nosleep(led_cdev, brightness);
	if (morse_data->slower)
	{
		mod_timer(&morse_data->timer, jiffies + delay + 200);
	}
	else {
		mod_timer(&morse_data->timer, jiffies + delay);
	}
}

static ssize_t led_slower_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;

	return sprintf(buf, "%u\n", morse_data->slower);
}

static ssize_t led_slower_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;
	unsigned long state;
	int ret;
	//what is kstrtoul
	ret = kstrtoul(buf, 0, &state);
	if (ret)
		return ret;
	//figure !!state out
	morse_data->slower = !!state;

	return size;
}

static DEVICE_ATTR(slower, 0644, led_slower_show, led_slower_store);

static ssize_t led_repeat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;

	return sprintf(buf, "%u\n", morse_data->repeat);
}

static ssize_t led_repeat_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;
	unsigned long state;
	int ret;
	//what is kstrtoul
	ret = kstrtoul(buf, 0, &state);
	if (ret)
		return ret;
	//figure !!state out
	morse_data->repeat = !!state;

	return size;
}

static DEVICE_ATTR(repeat, 0644, led_repeat_show, led_repeat_store);

/* BEGIN DUMMY FUNCTIONS HERE */

static int my_open(struct inode *i, struct file *f)
{
    printk("CS444 Dummy driver open\r\n");
    return 0;
}

static int my_close(struct inode *i, struct file *f)
{
    printk("CS444 Dummy driver close\r\n");
    return 0;
}

static ssize_t dummy_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    printk("CS444 Dummy driver read\r\n");
    snprintf(buf, size, "Hey there, I'm a dummy!\r\n");
    return strlen(buf);
}

static ssize_t dummy_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
  	//if msg not null:
  	//	free msg
  	//	kalloc new message size
  	//	set global msg point to point at the kalloc space
  	//	reset idx and sym_idx to default
  	if (msg) {
      	kfree(msg);
      	msg = kmalloc(size + 1, 0);
      	//memset(msg, 0, size + 1);
		msg[size] = '\0';
      	idx = 0;
      	sym_idx = 1;


    	printk("Before the copy from user function");

		if (copy_from_user(msg, buf, size))
        {
            return -EACCES;
        }
    }
  
    //char lcl_buf[64];

    //memset(lcl_buf, 0, sizeof(lcl_buf));

    //if (copy_from_user(lcl_buf, buf, min(size, sizeof(lcl_buf))))


    printk("CS444 Dummy driver write %ld bytes: %s\r\n", size, msg);

    return size;
}

static struct file_operations dummy_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .read = dummy_read,
    .write = dummy_write,
    .release = my_close
};

/* END DUMMY FUNCTIONS HERE */

static void morse_trig_activate(struct led_classdev *led_cdev)
{
	/*
	FILE* fd = open("/dev/dummy", O_RDWR);
	//do something with fd
    if (fd < 0) {
		// handle error
	}
	if (read(fd, buffer, size) < 0) {
	// handle error
	}
	//read(fd, buffersize, fileoffset);
	close(fd);
	*/
	struct morse_trig_data *morse_data;
	int rc;

	morse_data = kzalloc(sizeof(*morse_data), GFP_KERNEL);
	if (!morse_data)
		return;

	led_cdev->trigger_data = morse_data;
	rc = device_create_file(led_cdev->dev, &dev_attr_slower);
	if (rc) {
		kfree(led_cdev->trigger_data);
		return;
	}
	rc = device_create_file(led_cdev->dev, &dev_attr_repeat);
	if (rc) {
		kfree(led_cdev->trigger_data);
		return;
	}

	setup_timer(&morse_data->timer,
		led_morse_function, (unsigned long)led_cdev);
	morse_data->phase = 0;
	if (!led_cdev->blink_brightness)
		led_cdev->blink_brightness = led_cdev->max_brightness;
	led_morse_function(morse_data->timer.data);
	set_bit(LED_BLINK_SW, &led_cdev->work_flags);
	led_cdev->activated = true;

	/* BEGIN DUMMY CODE HERE*/

	/* code from dummy_init */

    int ret;
    struct device *dev_ret;
 
    // Allocate the device
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "morse")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &dummy_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
     
    // Allocate the /dev device (/dev/cs444_dummy)
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "cs444_dummy")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

    printk("CS444 Dummy Driver has been loaded!\r\n");

	/* END DUMMY CODE HERE */
}

static void morse_trig_deactivate(struct led_classdev *led_cdev)
{
	
	//fd = close("/dev/dummy", O_RDWR);
	struct morse_trig_data *morse_data = led_cdev->trigger_data;

	if (led_cdev->activated) {
		del_timer_sync(&morse_data->timer);
		device_remove_file(led_cdev->dev, &dev_attr_slower);
		device_remove_file(led_cdev->dev, &dev_attr_repeat);
		kfree(morse_data);
		clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
		led_cdev->activated = false;
	}

	/* BEGIN DUMM CODE HERE */

	/* Code from dummy_exit */

    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);

	/* END DUMMY CODE HERE */
}

static struct led_trigger morse_led_trigger = {
	.name = "morse",
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
	msg = kmalloc(4,0);
	msg[0] = 's';
	msg[1] = 'o';
	msg[2] = 's';
	msg[3] = '\0';

	if (!rc) {
		atomic_notifier_chain_register(&panic_notifier_list,
			&morse_panic_nb);
		register_reboot_notifier(&morse_reboot_nb);
	}
	return rc;
}

static void __exit morse_trig_exit(void)
{
	kfree(msg);
	unregister_reboot_notifier(&morse_reboot_nb);
	atomic_notifier_chain_unregister(&panic_notifier_list,
		&morse_panic_nb);
	led_trigger_unregister(&morse_led_trigger);
}

module_init(morse_trig_init);
module_exit(morse_trig_exit);

MODULE_AUTHOR("Nick Pugliese <nicholas.d.pugliese@gmail.com>");
MODULE_DESCRIPTION("morse LED trigger v0.2");
MODULE_LICENSE("GPL");
