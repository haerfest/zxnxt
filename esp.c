#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "esp.h"
#include "log.h"


#define MAX_AT_PREFIX_LENGTH  20

#define TX_SIZE  (8 * 1024)
#define RX_SIZE  (8 * 1024)

#define CR       '\r'
#define LF       '\n'
#define CRLF     "\r\n"


typedef void (*tx_handler_t)(void);


typedef struct {
  u8_t*  data;
  size_t size;
  size_t read_index;
  size_t n_elements;
} buffer_t;


static int buffer_read(buffer_t* buffer, u8_t* value) {
  if (buffer->n_elements == 0) {
    return 0;
  }

  if (value) {
    *value = buffer->data[buffer->read_index];
  }
  buffer->n_elements--;
  buffer->read_index = (buffer->read_index + 1) % buffer->size;

  return 1;
}


static int buffer_peek(buffer_t* buffer, size_t index, u8_t* value) {
  if (buffer->n_elements <= index) {
    return 0;
  }

  *value = buffer->data[(buffer->read_index + index) % buffer->size];

  return 1;
}


static int buffer_write(buffer_t* buffer, u8_t value) {
  if (buffer->n_elements == buffer->size) {
    return 0;
  }

  buffer->data[(buffer->read_index + buffer->n_elements) % buffer->size] = value;
  buffer->n_elements++;

  return 1;
}


typedef struct {
  buffer_t     tx;
  buffer_t     rx;
  tx_handler_t tx_handler;

  int          do_echo;
} esp_t;


static esp_t self;


int esp_init(void) {
  self.tx.size = TX_SIZE;
  self.rx.size = RX_SIZE;
  self.tx.data = (u8_t *) malloc(self.tx.size);
  self.rx.data = (u8_t *) malloc(self.rx.size);

  if (self.tx.data == NULL || self.rx.data == NULL) {
    log_wrn("esp: out of memory\n");
    if (self.tx.data != NULL) free(self.tx.data);
    if (self.rx.data != NULL) free(self.rx.data);
    return 1;
  }

  esp_reset(E_RESET_HARD);

  return 0;
}


void esp_finit(void) {
}


static void respond(const char* response) {
  while (*response) {
    (void) buffer_write(&self.rx, *response++);
  }
}


static void ok(void) {
  respond("OK" CRLF);
}


static void error(void) {
  respond("ERROR" CRLF);
}


static void clear_crlf(void) {
  u8_t value;
  
  while (buffer_peek(&self.tx, 0, &value)) {
    if (value == CR) {
      (void) buffer_read(&self.tx, NULL);
      break;
    }
  }

  while (buffer_peek(&self.tx, 0, &value)) {
    if (value == LF) {
      (void) buffer_read(&self.tx, NULL);
      break;
    }
  }
}


typedef void (*at_handler_t)(void);


/**
 * ATE0
 * ATE1
 */
static void at_echo(void) {
  u8_t value;

  if (!buffer_read(&self.tx, &value)) {
    error();
    return;
  }

  if (value == '0' || value == '1') {
    self.do_echo = (value == '1');
    ok();
    return;
  }

  error();
}


static void at(void) {
  const struct {
    char*        prefix;
    at_handler_t handler;
  } handlers[] = {
    { "ATE", at_echo }
  };
  const size_t n_handlers = sizeof(handlers) / sizeof(*handlers);
  char         prefix[MAX_AT_PREFIX_LENGTH + 1];
  size_t       i;
  size_t       prefix_length;
  u8_t         value;

  /* Command syntax must be one of:
   * 1. AT<cmd>?
   * 2. AT<cmd>=<payload>
   * 3. AT<cmd>
   */
  prefix_length = 0;
  while (prefix_length < MAX_AT_PREFIX_LENGTH && buffer_peek(&self.tx, 0, &value)) {
    if (value == '?' || value == '=') {
      break;
    }
    (void) buffer_read(&self.tx, NULL);
    prefix[prefix_length++] = value;
  }
  prefix[prefix_length] = 0;
  log_wrn("esp: at => %s\n", prefix);

  for (i = 0; i < n_handlers; i++) {
    if (strncmp(handlers[i].prefix, prefix, prefix_length) == 0) {
      handlers[i].handler();
      return;
    }
  }

  /* Unknown command. */
  error();
}


static void idle_tx(void) {
  u8_t prefix[2] = { 0, 0 };

  /* Wait for carriage return. */
  if (self.tx.n_elements < 2) {
    return;
  }
  (void) buffer_peek(&self.tx, self.tx.n_elements - 2, &prefix[0]);
  (void) buffer_peek(&self.tx, self.tx.n_elements - 1, &prefix[1]);
  if (strncmp((const char *) prefix, CRLF, 2) != 0) {
    return;
  }

  /* Must be AT-command. */
  (void) buffer_peek(&self.tx, 0, &prefix[0]);
  (void) buffer_peek(&self.tx, 1, &prefix[1]);
  if (strncmp((const char*) prefix, "AT", 2) != 0) {
    error();
    clear_crlf();
    return;
  }

  /* Echo if required. */
  if (self.do_echo) {
    size_t i;
    for (i = 0; i < self.tx.n_elements; i++) {
      u8_t value;
      (void) buffer_peek(&self.tx, i, &value);
      (void) buffer_write(&self.rx, value);
    }
  }

  at();
  clear_crlf();
}


void esp_write(u8_t value) {
  if (!buffer_write(&self.tx, value)) {
    /* Buffer full, signal? */
    error();
    return;
  }

  self.tx_handler();
}


u8_t esp_read(void) {
  u8_t value;

  return buffer_read(&self.rx, &value) ? value : 0x00;
}


void esp_reset(reset_t reset) {
  self.tx.read_index = 0;
  self.tx.n_elements = 0;
  self.rx.read_index = 0;
  self.rx.n_elements = 0;

  self.tx_handler = idle_tx;

  self.do_echo = 1;
}
