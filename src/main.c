#include "mgos.h"
#include "ft260.h"

int main(int argc, char **argv) {
  char *            hidpath = NULL;
  struct ft260_dev *d;
  uint16_t          freq;
  uint8_t           buf[64];
  int res;

  if (argc > 1) {
    hidpath = strdup(argv[1]);
  }

  if (!(d = ft260_i2c_create(hidpath))) {
    LOG(LL_ERROR, ("Could not create FT260 driver"));
    return -1;
  }
  if (!ft260_i2c_set_speed(d, 100)) {
    LOG(LL_ERROR, ("Could not set I2C speed"));
    return -1;
  }
  if (!ft260_i2c_get_speed(d, &freq)) {
    LOG(LL_ERROR, ("Could not get I2C speed"));
  } else {
    LOG(LL_INFO, ("I2C Bus Speed is %u kHz", freq));
  }


  /*
   * // Send a Report to the Device
   * memset(buf, 0, sizeof(buf));
   * buf[0] = 0xD0; // I2C write
   * buf[1] = 0x22; // Slave address
   * buf[2] = 0x06; // Start and Stop
   * buf[3] = 0x03; // data len
   * buf[4] = 'a';
   * buf[5] = 'b';
   * buf[6] = 'c';
   * res    = write(d->fd, buf, 7);
   * if (res > 0) {
   * LOG_HEXDUMP(LL_DEBUG, "Write Buffer", buf, 7);
   * }
   * LOG(LL_INFO, ("Write %d bytes to i2caddr=0x%02x: %s", res, 0x22, strerror(errno)));
   */


  // Scan the bus
  for (int i = 1; i <= 127; i++) {
    memset(buf, 0, sizeof(buf));
    buf[0] = 0xC2;
    buf[1] = i;
    buf[2] = 0x06;
    buf[3] = 0;
    buf[4] = 0;

    res = write(d->fd, buf, 5);
    LOG(LL_INFO, ("write()=%d bytes to i2caddr=0x%02x: %s", res, i, res < 0 ? strerror(errno) : "OK"));
    if (res > 0) {
      LOG_HEXDUMP(LL_DEBUG, "Write Buffer", buf, res);
    } else {
      ft260_i2c_reset(d);
    }

    memset(buf, 0, sizeof(buf));
    res = read(d->fd, buf, 64);
    LOG(LL_INFO, ("read()=%d bytes from i2caddr=0x%02x: %s", res, i, res < 0 ? strerror(errno) : "OK"));
    if (res > 0) {
      LOG_HEXDUMP(LL_DEBUG, "Read Buffer", buf, res);
    } else {
      ft260_i2c_reset(d);
    }
  }

  if (!ft260_i2c_destroy(&d)) {
    LOG(LL_ERROR, ("Could not destroy FT260 drivier"));
    return -1;
  }
  return 0;
}
