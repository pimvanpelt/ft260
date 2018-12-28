#include "mgos.h"
#include "ft260.h"

int ft260_i2c_read_reg_b(struct ft260_dev *d, uint16_t addr, uint8_t reg) {
  uint8_t value;

  if (!ft260_i2c_read_reg_n(d, addr, reg, 1, &value)) {
    return -1;
  }
  return value;
}

int ft260_i2c_read_reg_w(struct ft260_dev *d, uint16_t addr, uint8_t reg) {
  uint8_t tmp[2];

  if (!ft260_i2c_read_reg_n(d, addr, reg, 2, tmp)) {
    return -1;
  }
  return (((uint16_t)tmp[0]) << 8) | tmp[1];
}

bool ft260_i2c_read_reg_n(struct ft260_dev *d, uint16_t addr, uint8_t reg, size_t n, uint8_t *buf) {
  return ft260_i2c_write(d, addr, &reg, 1, false /* stop */) &&
         ft260_i2c_read(d, addr, buf, n, true /* stop */);
}

bool ft260_i2c_write_reg_b(struct ft260_dev *d, uint16_t addr, uint8_t reg, uint8_t value) {
  uint8_t tmp[2] = { reg, value };

  return ft260_i2c_write(d, addr, tmp, sizeof(tmp), true /* stop */);
}

bool ft260_i2c_write_reg_w(struct ft260_dev *d, uint16_t addr, uint8_t reg, uint16_t value) {
  uint8_t tmp[3] = { reg, (uint8_t)(value >> 8), (uint8_t)value };

  return ft260_i2c_write(d, addr, tmp, sizeof(tmp), true /* stop */);
}

bool ft260_i2c_write_reg_n(struct ft260_dev *d, uint16_t addr, uint8_t reg, size_t n, const uint8_t *buf) {
  bool     res = false;
  uint8_t *tmp = calloc(n + 1, 1);

  if (tmp) {
    *tmp = reg;
    memcpy(tmp + 1, buf, n);
    res = ft260_i2c_write(d, addr, tmp, n + 1, true /* stop */);
    free(tmp);
  }
  return res;
}
