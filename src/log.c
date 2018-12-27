/* Some functions mocked from MGOS, so we can run unit tests standalone.
 */

#include "mgos_mock.h"
#include <unistd.h>

int log_print_prefix(enum cs_log_level l, const char *func, const char *file) {
  char   ll_str[6];
  char   ll_file[21];
  char   ll_func[21];
  size_t offset = 0;

  switch (l) {
  case LL_ERROR:
    strncpy(ll_str, "ERROR", sizeof(ll_str));
    break;

  case LL_WARN:
    strncpy(ll_str, "WARN", sizeof(ll_str));
    break;

  case LL_INFO:
    strncpy(ll_str, "INFO", sizeof(ll_str));
    break;

  case LL_DEBUG:
    strncpy(ll_str, "DEBUG", sizeof(ll_str));
    break;

  case LL_VERBOSE_DEBUG:
    strncpy(ll_str, "VERB", sizeof(ll_str));
    break;

  default:                       // LL_NONE
    return 0;
  }

  offset = 0;
  memset(ll_file, 0, sizeof(ll_file));
  if (strlen(file) >= sizeof(ll_file)) {
    offset = strlen(file) - sizeof(ll_file) + 1;
  }
  strncpy(ll_file, file + offset, sizeof(ll_file) - 1);

  offset = 0;
  memset(ll_func, 0, sizeof(ll_func));
  if (strlen(func) >= sizeof(ll_func)) {
    offset = strlen(func) - sizeof(ll_func) + 1;
  }
  strncpy(ll_func, func + offset, sizeof(ll_func) - 1);

  printf("%-5s %-20s %-20s| ", ll_str, ll_file, ll_func);
  return 1;
}

void log_hexdump(const char *prefix, const uint8_t *data, const uint16_t len) {
  uint16_t i;

  LOG(LL_INFO, ("%s of %d bytes follows:", prefix, len));

  for (i = 0; i < len; i++) {
    if (i % 16 == 0) {
      log_print_prefix(LL_INFO, __func__, __FILE__);
    }
    printf("0x%02x ", data[i]);
    if (i % 16 == 15) {
      printf("\r\n");
    }
  }
  if (len % 16 != 0) {
    printf("\r\n");
  }
}
