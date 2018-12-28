#include "mgos.h"
#include "ft260.h"

enum ft260_feature_direction {
  NONE   = 0,
  INPUT  = 1,
  OUTPUT = 2
};

static const char *ft260_bus_type_str(int bus) {
  switch (bus) {
  case BUS_USB: return "USB";

  case BUS_HIL: return "HIL";

  case BUS_BLUETOOTH: return "Bluetooth";

  case BUS_VIRTUAL: return "Virtual";

  default: return "Other";
  }
}

char *ft260_get_hidpath(const unsigned short vendor_id, const unsigned short product_id, const unsigned short interface_id) {
  struct udev *           udev;
  struct udev_enumerate * enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *    dev;
  struct udev_device *    usb_dev;
  struct udev_device *    intf_dev;
  char *ret_path = NULL;

  /* Create the udev object */
  udev = udev_new();
  if (!udev) {
    LOG(LL_ERROR, ("Can't create udev"));
    return NULL;
  }
  /* Create a list of the devices in the 'hidraw' subsystem. */
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "hidraw");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  /* udev_list_entry_foreach is a macro which expands to a loop. */
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *   path;
    const char *   dev_path;
    const char *   str;
    unsigned short cur_vid;
    unsigned short cur_pid;
    unsigned short cur_interface_id;

    path     = udev_list_entry_get_name(dev_list_entry);
    dev      = udev_device_new_from_syspath(udev, path);
    dev_path = udev_device_get_devnode(dev);

    /* Find the next parent device, with matching
     * subsystem "usb" and devtype value "usb_device" */
    usb_dev = udev_device_get_parent_with_subsystem_devtype(
      dev, "usb", "usb_device");
    if (!usb_dev) {
      LOG(LL_ERROR, ("Unable to find parent usb device"));
      return NULL;
    }
    str      = udev_device_get_sysattr_value(usb_dev, "idVendor");
    cur_vid  = (str) ? strtol(str, NULL, 16) : -1;
    str      = udev_device_get_sysattr_value(usb_dev, "idProduct");
    cur_pid  = (str) ? strtol(str, NULL, 16) : -1;
    intf_dev = udev_device_get_parent_with_subsystem_devtype(
      dev, "usb", "usb_interface");
    if (!intf_dev) {
      LOG(LL_ERROR, ("Unable to find parent usb interface"));
      return NULL;
    }
    str = udev_device_get_sysattr_value(intf_dev, "bInterfaceNumber");
    cur_interface_id = (str) ? strtol(str, NULL, 16) : -1;
//    LOG(LL_DEBUG, ("vid=%x pid=%x interface=%d", cur_vid, cur_pid, cur_interface_id));
    if (cur_vid == vendor_id &&
        cur_pid == product_id &&
        cur_interface_id == interface_id) {
      ret_path = strdup(dev_path);
      udev_device_unref(dev);
      break;
    }
    udev_device_unref(dev);
  }
  /* Free the enumerator object */
  udev_enumerate_unref(enumerate);
  udev_unref(udev);
  return ret_path;
}

/* Send or retrieve a feature to/from the HID.
 *
 * direction:  0 is output, 1 = input
 * buf/buflen: Will write or read 'buflen' bytes from/to 'buf'.
 *
 * Returns true if successful, false otherwise.
 */
static bool ft260_hid_io(struct ft260_dev *d, const enum ft260_feature_direction dir, uint8_t *buf, uint8_t buflen) {
  int res;

  if (!d || d->fd < 0) {
    return false;
  }
  if (dir == OUTPUT) {
    res = ioctl(d->fd, HIDIOCSFEATURE(buflen), buf);
  } else if (dir == INPUT) {
    res = ioctl(d->fd, HIDIOCGFEATURE(buflen), buf);
  } else{
    return false;
  }
  if (res < 0) {
    LOG(LL_ERROR, ("Could not perform feature %s: %s", (dir == OUTPUT ? "output" : "input"), strerror(errno)));
    return false;
  }
  return true;
}

bool ft260_i2c_get_speed(struct ft260_dev *d, uint16_t *freq_khz) {
  uint8_t status = 0;

  if (!ft260_i2c_get_status(d, &status)) {
    return false;
  }
  if (freq_khz) {
    *freq_khz = d->freq_khz;
  }
  return true;
}

bool ft260_i2c_get_status(struct ft260_dev *d, uint8_t *status) {
  uint8_t buf[5];

  memset(buf, 0, sizeof(buf));
  buf[0] = 0xC0; // GET STATUS
  if (!ft260_hid_io(d, INPUT, buf, sizeof(buf))) {
    return false;
  }

  d->freq_khz  = ((uint16_t)buf[2]) << 8;
  d->freq_khz += buf[3];
  if (status) {
    *status = buf[1];
  }
  return true;
}

bool ft260_i2c_set_speed(struct ft260_dev *d, const uint16_t freq_khz) {
  uint8_t buf[4];

  memset(buf, 0, sizeof(buf));
  buf[0] = 0xA1;            // SYSTEM_SETTING_ID
  buf[1] = 0x22;            // I2C_SPEED
  buf[2] = freq_khz >> 8;   // MSB
  buf[3] = freq_khz & 0xff; // LSB

  if (!ft260_hid_io(d, OUTPUT, buf, sizeof(buf))) {
    return false;
  }
  if (!ft260_i2c_get_status(d, NULL)) {
    return false;
  }

  return freq_khz == d->freq_khz;
}

struct ft260_dev *ft260_i2c_create(const char *devpath) {
  struct ft260_dev *d = calloc(1, sizeof(struct ft260_dev));
  int      res;
  char *   hidpath = (char *)devpath;
  uint8_t  buf[26];
  uint16_t freq;

  if (!hidpath) {
    if (!(hidpath = ft260_get_hidpath(0x0403, 0x6030, 0))) {
      LOG(LL_ERROR, ("Could not autodetect FT260 in udev"));
      return NULL;
    }
  }

  if (!d) {
    return NULL;
  }
  memset(d, 0, sizeof(struct ft260_dev));

  // Open HID device, nonblocking
  if ((d->fd = open(hidpath, O_RDWR | O_NONBLOCK)) < 0) {
    LOG(LL_ERROR, ("Unable to open %s: %s", devpath, strerror(errno)));
    free(d);
    return NULL;
  }
  d->devpath = strdup(hidpath);

  // Get RawName and RawInfo
  if ((res = ioctl(d->fd, HIDIOCGRAWNAME(256), d->rawname)) < 0) {
    LOG(LL_ERROR, ("HIDIOCGRAWNAME: %s", strerror(errno)));
    close(d->fd);
    free(d);
    return NULL;
  }
  if ((res = ioctl(d->fd, HIDIOCGRAWINFO, &d->info)) < 0) {
    LOG(LL_ERROR, ("HIDIOCGRAWINFO: %s", strerror(errno)));
    close(d->fd);
    free(d);
    return NULL;
  }

  // Read the chip ID
  memset(buf, 0, sizeof(buf));
  buf[0] = 0xA0; // CHIP_VERSION_ID
  if (!ft260_hid_io(d, INPUT, buf, 13)) {
    LOG(LL_ERROR, ("Could not read FT260 chip ID"));
    close(d->fd);
    free(d);
    return NULL;
  }
  LOG_HEXDUMP(LL_DEBUG, "Chip ID", buf, 13);

  // Read the System Status
  memset(buf, 0, sizeof(buf));
  buf[0] = 0xA1; // SYSTEM_STATUS_ID
  if (!ft260_hid_io(d, INPUT, buf, 26)) {
    LOG(LL_ERROR, ("Could not read FT260 system status"));
    close(d->fd);
    free(d);
    return NULL;
  }
  LOG_HEXDUMP(LL_DEBUG, "Status", buf, 26);

  // Reset I2C
  if (!ft260_i2c_reset(d)) {
    LOG(LL_ERROR, ("Could not set reset FT260"));
    close(d->fd);
    free(d);
    return NULL;
  }

  // Set the chip to I2C mode
  buf[0] = 0xA1; // SYSTEM_SETTING_ID
  buf[1] = 0x02; // I2C_MODE
  buf[2] = 1;    // Enable
  if (!ft260_hid_io(d, OUTPUT, buf, 3)) {
    LOG(LL_ERROR, ("Could not set mode to I2C"));
    close(d->fd);
    free(d);
    return NULL;
  }

  if (!ft260_i2c_get_speed(d, &freq)) {
    LOG(LL_ERROR, ("Could not get frequency"));
    close(d->fd);
    free(d);
    return NULL;
  }

  LOG(LL_DEBUG, ("FT260 initialized: devpath=%s fd=%d bustype=%s vendor=0x%04x product=0x%04x rawname='%s' i2cfreq=%ukHz", d->devpath, d->fd, ft260_bus_type_str(d->info.bustype), d->info.vendor, d->info.product, d->rawname, freq));

  return d;
}

bool ft260_i2c_reset(struct ft260_dev *d) {
  uint8_t buf[2];

  memset(buf, 0, sizeof(buf));
  buf[0] = 0xA1;            /* SYSTEM_SETTING_ID */
  buf[1] = 0x20;            /* RESET_I2C */
  return ft260_hid_io(d, OUTPUT, buf, sizeof(buf));
}

static bool ft260_i2c_wait(struct ft260_dev *d) {
  uint8_t status = 0;

  // Wait for the controller to be ready
  while (!(status & FT260_STATUS_IDLE)) {
    if (!ft260_i2c_get_status(d, &status)) {
      return false;
    }
    usleep(100);
  }
  if (status & FT260_STATUS_IDLE) {
    LOG(LL_DEBUG, ("Status: I2C Idle"));
    return true;
  }
  return false;
}

bool ft260_i2c_read(struct ft260_dev *d, uint16_t addr, void *data, size_t len, bool stop) {
  uint8_t buf[64];
  int     res;
  uint8_t status;

  if (!d || d->fd == -1) {
    return false;
  }

  if (!data && len > 0) {
    return false;
  }

  // Read from device
  buf[0] = 0xC2; // I2C read
  buf[1] = (uint8_t)addr;
  buf[2] = 0;
  if (addr == (uint16_t)-1) {
    buf[1] = 0;
  } else {
    buf[2] |= 0x02;
  }
  if (stop) {
    buf[2] |= 0x04;
  }
  len   &= 0xffff;
  buf[3] = len >> 8;
  buf[4] = len & 0xff;

  printf("Write: ");
  for (int i = 0; i < 5; i++) {
    printf("0x%02x ", buf[i]);
  }
  printf("\r\n");

  res = write(d->fd, buf, 5);
  if (res < 0) {
    LOG(LL_ERROR, ("write error: %s", strerror(errno)));
  } else {
    LOG(LL_INFO, ("write %d bytes", res));
  }

  res = read(d->fd, buf, 64);
  if (res < 0) {
    LOG(LL_ERROR, ("read error: %s", strerror(errno)));
  } else {
    LOG(LL_INFO, ("read %d bytes", res));
    for (int i = 0; i < res; i++) {
      printf("0x%02x ", buf[i]);
    }
    printf("\r\n");
  }

  ft260_i2c_get_status(d, &status);
  LOG(LL_DEBUG, ("I2C Status: 0x%02x", status));
  return false;
}

/*
 * Write specified number of bytes to the specified address.
 * Address should not include the R/W bit. If addr is -1, START is not
 * performed.
 * If |stop| is true, then at the end of the operation bus will be released.
 */

bool ft260_i2c_write(struct ft260_dev *d, uint16_t addr, const void *data, size_t len, bool stop) {
  uint8_t buf[64];
  uint8_t status;
  ssize_t written = 0;

  if (!d || d->fd == -1) {
    return false;
  }
  if (!data && len > 0) {
    return false;
  }
  if (len > 60) {
    LOG(LL_ERROR, ("Writing packets larger than 60 bytes is not (yet) supported"));
    return false;
  }

  buf[0] = 0xD0 + (len <= 4 ? 0 : (len - 1) / 4); // Report ID 0xD0=4 bytes, 0xD1=8 bytes, .. 0xDE=60 bytes.
  buf[1] = 0;
  buf[2] = 0x00;                                  // Flags; 2=|start|, 4=|stop|
  if (addr != (uint16_t)-1) {
    buf[1]  = (uint8_t)addr;
    buf[2] |= 0x02;          // Set start bit.
  }
  if (stop) {
    buf[2] |= 0x04;          // Set stop bit.
  }
  buf[3] = (uint8_t)len;
  memcpy(buf + 4, data, len);

  // Wait for the controller to be ready
  if (!ft260_i2c_wait(d)) {
    LOG(LL_ERROR, ("Timeout waiting before write"));
    return false;
  }

  written = write(d->fd, buf, len + 4);
  LOG(LL_DEBUG, ("Wrote %ld bytes to 0x%02x %sstart %sstop: %s", written, buf[1], buf[2] & 0x02 ? "" : "!", buf[2] & 0x04 ? "" : "!", (written == (ssize_t)len + 4) ? "OK" : "FAIL"));
  if (written != (ssize_t)len + 4) {
    return false;
  }

  if (!ft260_i2c_wait(d)) {
    LOG(LL_ERROR, ("Timeout waiting after write"));
    return false;
  }

  if (!ft260_i2c_get_status(d, &status)) {
    return false;
  }
  LOG(LL_INFO, ("Status: 0x%02x", status));

  if (status & FT260_STATUS_MASTER_BUSY) {
    LOG(LL_ERROR, ("Error: controller busy"));
    return false;
  }
  if (status & FT260_STATUS_ERROR) {
    LOG(LL_ERROR, ("Error: Error condition: %s %s %s",
                   (status & FT260_STATUS_ERROR_SLAVE_ACK ? "(slave_ack)" : ""),
                   (status & FT260_STATUS_ERROR_DATA_ACK ? "(data_ack)" : ""),
                   (status & FT260_STATUS_ERROR_LOST ? "(lost)" : "")));
    return false;
  }
  if (status & FT260_STATUS_BUS_BUSY) {
    LOG(LL_DEBUG, ("Status: I2C Bus Busy"));
  }
  return true;
}

bool ft260_i2c_destroy(struct ft260_dev **d) {
  if (!(*d)) {
    return false;
  }
  if ((*d)->fd != -1) {
    close((*d)->fd);
  }
  if ((*d)->devpath) {
    free((*d)->devpath);
  }
  free(*d);
  *d = NULL;
  return true;
}
