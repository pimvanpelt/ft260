#pragma once

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <libudev.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/*
 * For the systems that don't have the new version of hidraw.h in userspace.
 */
#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

#define FT260_STATUS_MASTER_BUSY        (0x01) // If this bit set, all other bits in status are invalid.
#define FT260_STATUS_ERROR              (0x02)
#define FT260_STATUS_ERROR_SLAVE_ACK    (0x04)
#define FT260_STATUS_ERROR_DATA_ACK     (0x08)
#define FT260_STATUS_ERROR_LOST         (0x10)
#define FT260_STATUS_IDLE               (0x20)
#define FT260_STATUS_BUS_BUSY           (0x40)

struct ft260_dev {
  int                   fd;
  char *                devpath;
  char                  rawname[256];
  struct hidraw_devinfo info;
  uint16_t              freq_khz;
};

/* Find an FT260 device in the USB Device List. To get the first FT260, use:
 *   `hidpath = ft260_get_hidpath(0x0403, 0x6030, 0);`
 * Return NULL if none were found, and a pointer to a memory allocated string otherwise.
 * Note: Caller should free the returned string.
 */
char *ft260_get_hidpath(const unsigned short vendor_id, const unsigned short product_id, const unsigned short interface_id);

struct ft260_dev *ft260_i2c_create(const char *devpath);
bool ft260_i2c_destroy(struct ft260_dev **d);

/* Set/Get I2C bus speed in kHz, usual values are 100, 400, 3400
 * Returns true if bus speed was set or get successfully, false otherwise.
 * Note: If 'false' is returned from get_speed(), freq_khz is unspecified.
 */
bool ft260_i2c_set_speed(struct ft260_dev *d, const uint16_t freq_khz);
bool ft260_i2c_get_speed(struct ft260_dev *d, uint16_t *freq_khz);

/* Get I2C status from the device.
 * Status bits:
 *   bit0:  controller busy, all other bits are invalid
 *   bit1:  error condition
 *   bit2:  slave address not ack'd during last operation
 *   bit3:  data not ack'd during last operation
 *   bit4:  arbitration lost during last operation
 *   bit5:  controller idle
 *   bit6:  bus busy
 *   bit7:  RESERVED
 * Returns true if the request was successful, false otherwise.
 * Note: if 'false' is returned, status is unspecified
 */
bool ft260_i2c_get_status(struct ft260_dev *d, uint8_t *status);

/* Reset I2C Master controller
 * Returns true if successful, false otherwise.
 */
bool ft260_i2c_reset(struct ft260_dev *d);

/*
 * Read specified number of bytes from the specified address.
 * Address should not include the R/W bit. If addr is -1, START is not
 * performed.
 * If |stop| is true, then at the end of the operation bus will be released.
 */
bool ft260_i2c_read(struct ft260_dev *d, uint16_t addr, void *data, size_t len, bool stop);

/*
 * Write specified number of bytes to the specified address.
 * Address should not include the R/W bit. If addr is -1, START is not
 * performed.
 * If |stop| is true, then at the end of the operation bus will be released.
 */
bool ft260_i2c_write(struct ft260_dev *d, uint16_t addr, const void *data, size_t len, bool stop);

/*
 * Release the bus (when left unreleased after read or write).
 */
void ft260_i2c_stop(struct ft260_dev *d);

/*
 * Helper for reading 1-byte register `reg` from a device at address `addr`.
 * In case of success return a numeric byte value from 0x00 to 0xff; otherwise
 * return -1.
 */
int ft260_i2c_read_reg_b(struct ft260_dev *d, uint16_t addr, uint8_t reg);

/*
 * Helper for reading 2-byte register `reg` from a device at address `addr`.
 * In case of success returns a numeric big-endian value: e.g. if 0x01, 0x02
 * was read from a device, 0x0102 will be returned.
 *
 * In case of error returns -1.
 */
int ft260_i2c_read_reg_w(struct ft260_dev *d, uint16_t addr, uint8_t reg);

/*
 * Helper for reading `n`-byte register value from a device. Returns true on
 * success, false on error. Data is written to `buf`, which should be large
 * enough.
 */
bool ft260_i2c_read_reg_n(struct ft260_dev *d, uint16_t addr, uint8_t reg, size_t n, uint8_t *buf);

/*
 * Helper for writing 1-byte register `reg` to a device at address `addr`.
 * Returns `true` in case of success, `false` otherwise.
 */
bool ft260_i2c_write_reg_b(struct ft260_dev *d, uint16_t addr, uint8_t reg, uint8_t value);

/*
 * Helper for writing 2-byte register `reg` to a device at address `addr`.
 * The value is big-endian: e.g. if `value` is `0x0102`, then `0x01, 0x02`
 * will be written.
 * Returns `true` in case of success, `false` otherwise.
 */
bool ft260_i2c_write_reg_w(struct ft260_dev *d, uint16_t addr, uint8_t reg, uint16_t value);

/*
 * Helper for writing `n`-byte register `reg` to a device at address `addr`.
 * Returns `true` in case of success, `false` otherwise.
 */
bool ft260_i2c_write_reg_n(struct ft260_dev *d, uint16_t addr, uint8_t reg, size_t n, const uint8_t *buf);
