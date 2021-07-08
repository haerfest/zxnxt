#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "esp.h"
#include "log.h"


#define MAX_AT_PREFIX_LENGTH    20
#define MAX_PACKET_LENGTH     2048

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


static size_t buffer_read(buffer_t* buffer, u8_t* value) {
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


static size_t buffer_read_n(buffer_t* buffer, size_t n, u8_t* values) {
  size_t i;

  for (i = 0; buffer->n_elements && (i < n); i++) {
    if (values) {
      values[i] = buffer->data[buffer->read_index];
    }
    buffer->n_elements--;
    buffer->read_index = (buffer->read_index + 1) % buffer->size;
  }

  return i;
}


static size_t buffer_peek(buffer_t* buffer, size_t index, u8_t* value) {
  if (buffer->n_elements <= index) {
    return 0;
  }

  *value = buffer->data[(buffer->read_index + index) % buffer->size];

  return 1;
}


static size_t buffer_peek_n(buffer_t* buffer, size_t index, size_t n, u8_t* values) {
  size_t i;

  for (i = 0; (index + i < buffer->n_elements) && (i < n); i++) {
    values[i] = buffer->data[(buffer->read_index + index + i) % buffer->size];
  }

  return i;
}


static size_t buffer_write(buffer_t* buffer, u8_t value) {
  if (buffer->n_elements == buffer->size) {
    return 0;
  }

  buffer->data[(buffer->read_index + buffer->n_elements) % buffer->size] = value;
  buffer->n_elements++;

  return 1;
}


/* Forward declarations. */
static void idle_tx(void);
static void send_tx(void);


typedef struct {
  buffer_t     tx;
  buffer_t     rx;
  tx_handler_t tx_handler;

  u32_t        baudrate;
  int          bits_per_frame;
  int          use_parity_check;
  int          use_odd_parity;
  int          use_two_stop_bits;
  int          do_echo;

  TCPsocket    tcp_socket;
  size_t       length;
  
} esp_t;


static esp_t self;


int esp_init(void) {
  self.tcp_socket = NULL;
  self.tx.size    = TX_SIZE;
  self.rx.size    = RX_SIZE;
  self.tx.data    = (u8_t *) malloc(self.tx.size);
  self.rx.data    = (u8_t *) malloc(self.rx.size);

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
  if (self.tcp_socket) {
    SDLNet_TCP_Close(self.tcp_socket);
    self.tcp_socket = NULL;
  }
}


static void respond(const char* response) {
  while (*response) {
    (void) buffer_write(&self.rx, *response++);
  }
}


static void ok(void) {
  log_wrn("esp: OK\n");
  respond("OK" CRLF);
}


static void error(void) {
  log_wrn("esp: ERROR\n");
  respond("ERROR" CRLF);
}


static void skip_crlf(void) {
  u8_t value;

  /* Precondition: there is a CRLF in the TX buffer. */

  while (1) {
    /* Hunt for CR. */
    while (buffer_read(&self.tx, &value) && value != CR);

    /* Could be sole CR, not followed by LF. */
    if (!buffer_read(&self.tx, &value)) {
      return;
    }
    if (value == CR) {
      break;
    }
  }
}


typedef void (*at_handler_t)(void);


/**
 * ATE0
 */
static void at_echo_on(void) {
  self.do_echo = 1;
  ok();
}


/**
 * ATE1
 */
static void at_echo_off(void) {
  self.do_echo = 0;
  ok();
}


/**
 * AT+UART_CUR?
 */
static void at_uart_cur(void) {
  char response[32 + 1];
  u8_t value;

  if (!buffer_read(&self.tx, &value)) {
    error();
    return;
  }

  if (value != '?') {
    error();
    return;
  }
  
  snprintf(response, sizeof(response), "+UART_CUR:%u,%d,%d,%d,0" CRLF,
           self.baudrate,
           self.bits_per_frame,
           self.use_two_stop_bits ? 2 : 1,             
           self.use_parity_check ? (self.use_odd_parity ? 1 : 2) : 0);
  respond(response);
  ok();
}


/**
 * AT+CIPSTART="TCP","<host>",<port>[<keepalive>]
 */
static void at_cipstart(void) {
  u8_t      host[80 + 1];
  u8_t      port[5 + 1];
  u8_t      s[8];
  size_t    i;
  IPaddress ip;

  if (buffer_read_n(&self.tx, sizeof(s), s) != sizeof(s)) {
    error();
    return;
  }

  if (strncmp((const char *) s, "=\"TCP\",\"", sizeof(s)) != 0) {
    error();
    return;
  }

  /* Read hostname or IP address until speech marks. */
  memset(host, 0, sizeof(host));
  for (i = 0; (i < sizeof(host)) && buffer_read(&self.tx, &host[i]) && host[i] != '"'; i++);
  if (i == sizeof(host)) {
    error();
    return;
  }
  if (host[i] != '"') {
    error();
    return;
  }
  host[i] = 0;

  /* Skip ",". */
  if (!buffer_read(&self.tx, s)) {
    error();
    return;
  }
  if (s[0] != ',') {
    error();
    return;
  }

  /* Read numeric port. */
  memset(port, 0, sizeof(port));
  for (i = 0; (i < sizeof(port) - 1) && buffer_read(&self.tx, &port[i]) && isdigit(port[i]); i++);
  if (i == sizeof(port) - 1) {
    error();
    return;
  }
  port[i] = 0;

  if (self.tcp_socket) {
    respond("ALREADY CONNECTED" CRLF);
    return;
  }

  log_wrn("esp: host='%s' port='%s'\n", host, port);
  if (SDLNet_ResolveHost(&ip, (const char *) host, atoi((const char *) port)) != 0) {
    log_wrn("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
    error();
    return;
  }

  self.tcp_socket = SDLNet_TCP_Open(&ip);
  if (!self.tcp_socket) {
    log_wrn("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
    error();
    return;
  }

  ok();
}


static void clear_tx(void) {
  while (buffer_read(&self.tx, NULL));
}


static void send_tx(void) {
  u8_t packet[MAX_PACKET_LENGTH];
  u8_t value;

  /* Wait until it starts with ">". */
  if (!buffer_peek(&self.tx, 0, &value)) {
    return;
  }
  log_wrn("esp: tx value=%c (%d)\n", value, value);
  if (value != '>') {
    error();
    clear_tx();
    self.tx_handler = idle_tx;
    return;
  }

  /* Wait for the packet. */
  if (buffer_peek_n(&self.tx, 1, self.length, packet) != self.length) {
    return;
  }

  {
    size_t i;
    log_wrn("esp: sending ");
    for (i = 0; i < self.length; i++) {
      log_wrn("%c", isprint(packet[i]) ? packet[i] : '.');
    }
    log_wrn("\n");
  }
  if (SDLNet_TCP_Send(self.tcp_socket, packet, self.length) < self.length) {
    log_wrn("SDLNet_TCP_Send: %s\n", SDLNet_GetError());
    respond("SEND FAIL" CRLF);
  } else {
    respond("SEND OK" CRLF);
  }

  self.tx_handler = idle_tx;
}


/**
 * AT+CIPSEND
 * AT+CIPSEND=<length>
 */
static void at_cipsend(void) {
  u8_t   length[4 + 1];  /* Max 2048. */
  u8_t   value;
  size_t i;

  (void) buffer_peek(&self.tx, 0, &value);
  switch (value) {
    case '=':
      /* Normal transmission mode. */
      (void) buffer_read(&self.tx, NULL);
 
      /* Read length. */
      memset(length, 0, sizeof(length));
      while (buffer_read(&self.tx, &length[0]) && length[0] == '0');
      for (i = 1; (i < sizeof(length) - 1) && buffer_read(&self.tx, &length[i]) && isdigit(length[i]); i++);
      if (i == sizeof(length) - 1) {
        error();
        return;
      }
      length[i] = 0;
      self.length = atoi((const char *) length);
      log_wrn("esp: length=%lu\n", self.length);
      if (self.length > MAX_PACKET_LENGTH) {
        error();
        return;
      }
      break;

    case CR:
      /* Transparent transmission mode. */
      error();
      return;

    default:
      error();
      return;
  }

  self.tx_handler = send_tx;
}


static void at(void) {
  /* Order common prefixes from longest to shortest. */
  const struct {
    char*        prefix;
    at_handler_t handler;
  } handlers[] = {
    { "E0",        at_echo_off },
    { "E1",        at_echo_on  },
    { "+UART_CUR", at_uart_cur },
    { "+CIPCLOSE", ok          },
    { "+CIPMUX",   ok          },
    { "+CIPSTART", at_cipstart },
    { "+CIPSEND",  at_cipsend  },
    { "",          ok          }
  };
  const size_t n_handlers = sizeof(handlers) / sizeof(*handlers);
  char         prefix[MAX_AT_PREFIX_LENGTH + 1];
  size_t       i, j;

  {
    u8_t value;
    log_wrn("esp: command '");
    for (j = 0; buffer_peek(&self.tx, j, &value); j++) {
      log_wrn("%c", isprint(value) ? value : '.');
    }
    log_wrn("'\n");
  }

  /* Command syntax must be one of:
   * 1. AT<cmd>?
   * 2. AT<cmd>=<payload>
   * 3. AT<cmd>
   */
  i = buffer_peek_n(&self.tx, 0, MAX_AT_PREFIX_LENGTH, (u8_t *) prefix);
  for (j = 0; (j < i) && (prefix[j] != '=' && prefix[j] != '?' && prefix[j] != CR); j++);
  prefix[j] = 0;

  for (i = 0; i < n_handlers; i++) {
    const size_t n = strlen(handlers[i].prefix);
    if (strncmp(prefix, handlers[i].prefix, n) == 0) {
      (void) buffer_read_n(&self.tx, n, NULL);
      handlers[i].handler();
      return;
    }
  }

  /* Unknown command, just fake an OK. */
  log_wrn("esp: unknown AT-command: '%s'\n", prefix);
  ok();
}


static void idle_tx(void) {
  u8_t prefix[2] = { 0, 0 };

  /* Wait for carriage return. */
  if (self.tx.n_elements < 2) {
    return;
  }
  (void) buffer_peek_n(&self.tx, self.tx.n_elements - 2, 2, prefix);
  if (strncmp((const char *) prefix, CRLF, 2) != 0) {
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

  /* Must be AT-command. */
  (void) buffer_read_n(&self.tx, 2, prefix);
  if (strncmp((const char*) prefix, "AT", 2) != 0) {
    error();
    skip_crlf();
    return;
  }

  /* Handle AT-command. */
  at();
  skip_crlf();
}


void esp_tx_write(u8_t value) {
  if (!buffer_write(&self.tx, value)) {
    /* Buffer full, signal? */
    error();
    return;
  }

  self.tx_handler();
}


u8_t esp_tx_read(void) {
  return (self.tx.n_elements == 0)                    << 4 /* Tx empty      */
       | (self.rx.n_elements >= self.rx.size * 3 / 4) << 3 /* Rx near full  */
       | (self.tx.n_elements == self.tx.size)         << 1 /* Tx full       */
       | (self.rx.n_elements > 0);                         /* Rx not empty  */
}


u8_t esp_rx_read(void) {
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

  if (self.tcp_socket) {
    SDLNet_TCP_Close(self.tcp_socket);
    self.tcp_socket = NULL;
  }
}


void esp_baudrate_set(u32_t baud) {
  self.baudrate = baud;
}


void esp_dataformat_set(int bits_per_frame, int use_parity_check, int use_odd_parity, int use_two_stop_bits) {
  self.bits_per_frame    = bits_per_frame;
  self.use_parity_check  = use_parity_check;
  self.use_odd_parity    = use_odd_parity;
  self.use_two_stop_bits = use_two_stop_bits;
}
