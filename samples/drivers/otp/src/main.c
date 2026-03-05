/*
 * Copyright (c) 2026 James Walmsley <james@fullfat-fs.co.uk>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "otp_sample.h"

int main(void)
{
	const struct device *otp = DEVICE_DT_GET(OTP_NODE);

	if (!device_is_ready(otp)) {
		printk("OTP device %s is not ready\n", otp->name);
		return 0;
	}

	printk("Using OTP device: %s (size=%u)\n", otp->name, (unsigned int)DT_REG_SIZE(OTP_NODE));
	printk("Using OTP test cell: offset=0x%x size=%u\n", (unsigned int)OTP_TEST_OFFSET,
	       (unsigned int)OTP_TEST_LEN);
	if (OTP_LOCK_AVAILABLE) {
		printk("Using OTP lock bytes: offset=0x%x size=%u\n", (unsigned int)OTP_LOCK_OFFSET,
		       (unsigned int)OTP_LOCK_LEN);
	}

	return 0;
}
