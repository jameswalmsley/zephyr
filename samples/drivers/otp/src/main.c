/*
 * Copyright (c) 2026 James Walmsley <james@fullfat-fs.co.uk>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/otp.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define OTP_NODE      DT_ALIAS(otp0)
#define OTP_CELL_NODE DT_NODELABEL(otp_sample)

#if !DT_NODE_HAS_STATUS(OTP_NODE, okay)
#error "No otp0 alias found in devicetree"
#endif

#if !DT_NODE_HAS_STATUS(OTP_CELL_NODE, okay)
#error "No otp_sample nvmem cell found in devicetree"
#endif

#define OTP_TEST_OFFSET DT_REG_ADDR(OTP_CELL_NODE)
#define OTP_TEST_LEN    DT_REG_SIZE(OTP_CELL_NODE)

BUILD_ASSERT(OTP_TEST_OFFSET + OTP_TEST_LEN <= DT_REG_SIZE(OTP_NODE),
	     "OTP test range exceeds device size");
BUILD_ASSERT(OTP_TEST_LEN == 16, "Update otp_pattern size to match OTP_TEST_LEN");

#if defined(DT_N_NODELABEL_otp_lock) && DT_NODE_HAS_STATUS(DT_NODELABEL(otp_lock), okay)
#define OTP_LOCK_AVAILABLE 1
#define OTP_LOCK_NODE      DT_NODELABEL(otp_lock)
#define OTP_LOCK_OFFSET    DT_REG_ADDR(OTP_LOCK_NODE)
#define OTP_LOCK_LEN       DT_REG_SIZE(OTP_LOCK_NODE)
BUILD_ASSERT(OTP_LOCK_OFFSET + OTP_LOCK_LEN <= DT_REG_SIZE(OTP_NODE),
	     "OTP lock range exceeds device size");
BUILD_ASSERT(OTP_LOCK_LEN == 16, "Expected 16 OTP lock bytes");
#else
#define OTP_LOCK_AVAILABLE 0
#endif

static const uint8_t otp_pattern[OTP_TEST_LEN] = {
	0x4F, 0x54, 0x50, 0x2D, 0x54, 0x45, 0x53, 0x54,
	0x2D, 0x46, 0x34, 0x00, 0xAA, 0x55, 0xC3, 0x3C,
};

static const struct device *otp_dev;

static bool otp_is_blank(const uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != 0xFF) {
			return false;
		}
	}

	return true;
}

static int cmd_otp_dump(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	const struct device *otp = otp_dev;
	size_t otp_size = DT_REG_SIZE(OTP_NODE);
	uint8_t buf[16];
	size_t offset = 0;
	int ret;

	if (otp == NULL || !device_is_ready(otp)) {
		shell_error(sh, "OTP device not ready");
		return -ENODEV;
	}

	while (offset < otp_size) {
		size_t len = MIN(sizeof(buf), otp_size - offset);

		ret = otp_read(otp, (off_t)offset, buf, len);
		if (ret < 0) {
			shell_error(sh, "OTP read failed at 0x%x: %d", (unsigned int)offset, ret);
			return ret;
		}

		shell_hexdump_line(sh, (unsigned int)offset, buf, len);
		offset += len;
	}

	return 0;
}

static int cmd_otp_program(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	const struct device *otp = otp_dev;
	uint8_t read_buf[OTP_TEST_LEN];
	int ret;

	if (otp == NULL || !device_is_ready(otp)) {
		shell_error(sh, "OTP device not ready");
		return -ENODEV;
	}

	ret = otp_read(otp, OTP_TEST_OFFSET, read_buf, sizeof(read_buf));
	if (ret < 0) {
		shell_error(sh, "OTP read failed: %d", ret);
		return ret;
	}

	if (!otp_is_blank(read_buf, sizeof(read_buf))) {
		shell_warn(sh, "OTP test cell not blank; refusing to program");
		shell_warn(sh, "Read:");
		shell_hexdump(sh, read_buf, sizeof(read_buf));
		return -EALREADY;
	}

	ret = otp_program(otp, OTP_TEST_OFFSET, otp_pattern, sizeof(otp_pattern));
	if (ret < 0) {
		shell_error(sh, "OTP program failed: %d", ret);
		return ret;
	}

	ret = otp_read(otp, OTP_TEST_OFFSET, read_buf, sizeof(read_buf));
	if (ret < 0) {
		shell_error(sh, "OTP readback failed: %d", ret);
		return ret;
	}

	if (memcmp(read_buf, otp_pattern, sizeof(otp_pattern)) == 0) {
		shell_info(sh, "OTP write OK; readback matches pattern");
	} else {
		shell_error(sh, "OTP readback mismatch");
		shell_info(sh, "Read:");
		shell_hexdump(sh, read_buf, sizeof(read_buf));
		shell_info(sh, "Expected:");
		shell_hexdump(sh, otp_pattern, sizeof(otp_pattern));
		return -EIO;
	}

	return 0;
}

static int cmd_otp_verify(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	const struct device *otp = otp_dev;
	uint8_t read_buf[OTP_TEST_LEN];
	int ret;

	if (otp == NULL || !device_is_ready(otp)) {
		shell_error(sh, "OTP device not ready");
		return -ENODEV;
	}

	ret = otp_read(otp, OTP_TEST_OFFSET, read_buf, sizeof(read_buf));
	if (ret < 0) {
		shell_error(sh, "OTP read failed: %d", ret);
		return ret;
	}

	if (memcmp(read_buf, otp_pattern, sizeof(otp_pattern)) == 0) {
		shell_info(sh, "OTP verify OK; pattern matches");
		return 0;
	}

	shell_error(sh, "OTP verify failed; mismatch");
	shell_info(sh, "Read:");
	shell_hexdump(sh, read_buf, sizeof(read_buf));
	shell_info(sh, "Expected:");
	shell_hexdump(sh, otp_pattern, sizeof(otp_pattern));
	return -EIO;
}

static int cmd_otp_write(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *otp = otp_dev;
	unsigned long offset;
	int ret;

	if (otp == NULL || !device_is_ready(otp)) {
		shell_error(sh, "OTP device not ready");
		return -ENODEV;
	}

	if (argc < 3) {
		shell_error(sh, "Usage: otp write <offset> <byte> [byte ...]");
		return -EINVAL;
	}

	offset = strtoul(argv[1], NULL, 0);
	if (offset >= DT_REG_SIZE(OTP_NODE)) {
		shell_error(sh, "Offset out of range");
		return -EINVAL;
	}

	for (size_t i = 2; i < argc; i++) {
		uint8_t value = (uint8_t)strtoul(argv[i], NULL, 0);
		size_t write_offset = offset + (i - 2);

		if (write_offset >= DT_REG_SIZE(OTP_NODE)) {
			shell_error(sh, "Write exceeds OTP size");
			return -EINVAL;
		}

		if (OTP_LOCK_AVAILABLE) {
			if ((write_offset >= OTP_LOCK_OFFSET) &&
			    (write_offset < (OTP_LOCK_OFFSET + OTP_LOCK_LEN))) {
				shell_error(sh, "Refusing to write OTP lock bytes; use 'otp lock'");
				return -EPERM;
			}
		}

		ret = otp_program(otp, (off_t)write_offset, &value, 1);
		if (ret < 0) {
			shell_error(sh, "OTP write failed at 0x%lx: %d",
				    (unsigned long)write_offset, ret);
			return ret;
		}
	}

	shell_info(sh, "OTP write OK (%u byte(s))", (unsigned int)(argc - 2));
	return 0;
}

static int cmd_otp_lock(const struct shell *sh, size_t argc, char **argv)
{
#if !OTP_LOCK_AVAILABLE
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_error(sh, "OTP lock bytes not defined in devicetree");
	return -ENOTSUP;
#else
	const struct device *otp = otp_dev;
	unsigned long index;
	uint8_t lock_byte = 0x00;
	int ret;

	if (otp == NULL || !device_is_ready(otp)) {
		shell_error(sh, "OTP device not ready");
		return -ENODEV;
	}

	if (argc < 2) {
		shell_error(sh, "Usage: otp lock <index 0-15> [value]");
		return -EINVAL;
	}

	index = strtoul(argv[1], NULL, 0);
	if (index >= OTP_LOCK_LEN) {
		shell_error(sh, "Index out of range (0-%u)", (unsigned int)(OTP_LOCK_LEN - 1));
		return -EINVAL;
	}

	if (argc >= 3) {
		lock_byte = (uint8_t)strtoul(argv[2], NULL, 0);
	}

	ret = otp_program(otp, (off_t)(OTP_LOCK_OFFSET + index), &lock_byte, 1);
	if (ret < 0) {
		shell_error(sh, "OTP lock write failed: %d", ret);
		return ret;
	}

	shell_info(sh, "OTP lock byte %lu programmed to 0x%02x", index, lock_byte);
	return 0;
#endif
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	otp_cmds, SHELL_CMD(dump, NULL, "Dump OTP contents in hex", cmd_otp_dump),
	SHELL_CMD(program, NULL, "Write test pattern to otp_sample", cmd_otp_program),
	SHELL_CMD(verify, NULL, "Verify otp_sample matches test pattern", cmd_otp_verify),
	SHELL_CMD(write, NULL, "Write raw byte(s): otp write <offset> <byte> [byte ...]",
		  cmd_otp_write),
	SHELL_CMD(lock, NULL, "Program one OTP lock byte: otp lock <index> [value]", cmd_otp_lock),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(otp, &otp_cmds, "OTP commands", NULL);

int main(void)
{
	const struct device *otp = DEVICE_DT_GET(OTP_NODE);

	if (!device_is_ready(otp)) {
		printk("OTP device %s is not ready\n", otp->name);
		return 0;
	}

	otp_dev = otp;

	printk("Using OTP device: %s (size=%u)\n", otp->name, (unsigned int)DT_REG_SIZE(OTP_NODE));
	printk("Using OTP test cell: offset=0x%x size=%u\n", (unsigned int)OTP_TEST_OFFSET,
	       (unsigned int)OTP_TEST_LEN);
	if (OTP_LOCK_AVAILABLE) {
		printk("Using OTP lock bytes: offset=0x%x size=%u\n", (unsigned int)OTP_LOCK_OFFSET,
		       (unsigned int)OTP_LOCK_LEN);
	}

	return 0;
}
