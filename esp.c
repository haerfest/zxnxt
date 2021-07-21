#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "defs.h"
#include "esp.h"
#include "log.h"


#define MAX_AT_PREFIX_LENGTH    20
#define MAX_PACKET_LENGTH     2048

#define TX_SIZE  MAX_PACKET_LENGTH
#define RX_SIZE  MAX_PACKET_LENGTH

#define CR       '\r'
#define LF       '\n'
#define CRLF     "\r\n"


typedef void (*tx_handler_t)(void);


/* Forward declarations. */
static void idle_tx(void);
static void send_tx(void);


typedef struct esp_t {
  buffer_t     tx;
  buffer_t     rx;
  u8_t*        rx_temp;
  size_t       rx_temp_index;
  tx_handler_t tx_handler;

  u32_t        baudrate;
  int          bits_per_frame;
  int          use_parity_check;
  int          use_odd_parity;
  int          use_two_stop_bits;
  int          do_echo;

  SDLNet_SocketSet socket_set;

  TCPsocket    socket;
  SDL_mutex*   socket_mutex;
  SDL_cond*    socket_changed;

  size_t       length;
  SDL_Thread*  rx_thread;
  int          do_finit;
} esp_t;


static esp_t self;


#define HEADER       "+IPD,0000:"
#define HEADER_SIZE  10


static void respond_n(const u8_t* response, size_t length) {
  size_t i;

  for (i = 0; (i < length) && !self.do_finit; i++) {
    SDL_LockMutex(self.rx.mutex);
    while (self.rx.n_elements == self.rx.size && !self.do_finit) {
      SDL_CondWaitTimeout(self.rx.element_removed, self.rx.mutex, 1000);
    }
    if (!self.do_finit) {
      buffer_write(&self.rx, response[i]);
    }
    SDL_UnlockMutex(self.rx.mutex);
  }
}


static void respond(const char* response) {
  respond_n((const u8_t *) response, strlen(response));
}


static void ok(void) {
  respond("OK" CRLF);
}


static void error(void) {
  respond("ERROR" CRLF);
}


static void copy_to_rx(size_t n) {
  /* Patch size. */
  snprintf((char *) &self.rx_temp[5], 4 + 1, "%04lu", n);
  self.rx_temp[HEADER_SIZE - 1] = ':';

  /* Copy data to rx buffer. */
  respond_n(self.rx_temp, HEADER_SIZE + n);
}


static int rx_thread(void *ptr) {
  TCPsocket socket;
  size_t    i;

  /* Prepend the header. */
  snprintf((char *) self.rx_temp, HEADER_SIZE, "%s", HEADER);

  while (!self.do_finit) {

    /* Wait until we have a socket. */
    SDL_LockMutex(self.socket_mutex);
    while (self.socket == NULL && !self.do_finit) {
      SDL_CondWaitTimeout(self.socket_changed, self.socket_mutex, 1000);
    }
    socket = self.socket;
    SDL_UnlockMutex(self.socket_mutex);

    i = HEADER_SIZE;

    while (!self.do_finit) {
      /* Wait until there is activity. */
      const int result = SDLNet_CheckSockets(self.socket_set, 20);

      if (result == 0) {
        /* Timeout: copy to rx what we have. */
        const size_t n = i - HEADER_SIZE;
        if (n > 0) {
          copy_to_rx(n);
          i = HEADER_SIZE;
        }
      } else if (result == 1) {
        /* Our socket should be ready. */
        if (!SDLNet_SocketReady(socket)) {
          break;
        }

        if (SDLNet_TCP_Recv(socket, &self.rx_temp[i], 1) != 1) {
          break;
        }
        
        if (++i == RX_SIZE) {
          /* Buffer full: copy to rx what we have. */
          const size_t n = i - HEADER_SIZE;
          copy_to_rx(n);
          i = HEADER_SIZE;
        }
      } else {
        /* Error */
        break;
      }
    }
  }

  return 0;
}


int esp_init(void) {
  self.socket   = NULL;
  self.do_finit = 0;

  if (buffer_init(&self.rx, RX_SIZE) != 0) {
    goto exit;
  }

  if (buffer_init(&self.tx, TX_SIZE) != 0) {
    goto exit_rx;
  }   

  self.socket_set = SDLNet_AllocSocketSet(1);
  if (self.socket_set == NULL) {
    goto exit_tx;
  }

  self.socket_changed = SDL_CreateCond();
  if (self.socket_changed == NULL) {
    goto exit_socket_set;
  }
  
  self.socket_mutex = SDL_CreateMutex();
  if (self.socket_mutex == NULL) {
    goto exit_socket_changed;
  }

  self.rx_temp = malloc(RX_SIZE + 1);
  if (self.rx_temp == NULL) {
    goto exit_socket_mutex;
  }
  
  self.rx_thread = SDL_CreateThread(rx_thread, "rx_thread", NULL);
  if (self.rx_thread == NULL) {
    goto exit_rx_temp;
  }

  esp_reset(E_RESET_HARD);

  return 0;

exit_rx:
  buffer_finit(&self.rx);
exit_tx:
  buffer_finit(&self.tx);
exit_socket_set:
  SDLNet_FreeSocketSet(self.socket_set);
exit_socket_changed:
  SDL_DestroyCond(self.socket_changed);
exit_socket_mutex:
  SDL_DestroyMutex(self.socket_mutex);
exit_rx_temp:
  free(self.rx_temp);
exit:
  log_err("esp: out of memory\n");
  return 1;
}


static void close_socket(void) {
  SDL_LockMutex(self.socket_mutex);

  SDLNet_TCP_Close(self.socket);
  SDLNet_TCP_DelSocket(self.socket_set, self.socket);

  self.socket = NULL;

  SDL_CondSignal(self.socket_changed);
  SDL_UnlockMutex(self.socket_mutex);
}


void esp_finit(void) {
  self.do_finit = 1;

  if (self.socket) {
    close_socket();
  }

  SDL_WaitThread(self.rx_thread, NULL);

  buffer_finit(&self.rx);
  buffer_finit(&self.tx);
  SDLNet_FreeSocketSet(self.socket_set);
  SDL_DestroyCond(self.socket_changed);
  SDL_DestroyMutex(self.socket_mutex);
  free(self.rx_temp);
}


static void skip_crlf(void) {
  u8_t value;

  /* Precondition: there is a CRLF in the TX buffer. */

  while (1) {
    /* Hunt for CR. */
    while (buffer_read_n(&self.tx, 1, &value) && value != CR);

    /* Could be sole CR, not followed by LF. */
    if (!buffer_read_n(&self.tx, 1, &value)) {
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

  if (!buffer_read_n(&self.tx, 1, &value)) {
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
  for (i = 0; (i < sizeof(host)) && buffer_read_n(&self.tx, 1, &host[i]) && host[i] != '"'; i++);
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
  if (!buffer_read_n(&self.tx, 1, s)) {
    error();
    return;
  }
  if (s[0] != ',') {
    error();
    return;
  }

  /* Read numeric port. */
  memset(port, 0, sizeof(port));
  for (i = 0; (i < sizeof(port) - 1) && buffer_read_n(&self.tx, 1, &port[i]) && isdigit(port[i]); i++);
  if (i == sizeof(port) - 1) {
    error();
    return;
  }
  port[i] = 0;

  if (self.socket) {
    respond("ALREADY CONNECTED" CRLF);
    return;
  }

  if (SDLNet_ResolveHost(&ip, (const char *) host, atoi((const char *) port)) != 0) {
    error();
    return;
  }

  SDL_LockMutex(self.socket_mutex);

  self.socket = SDLNet_TCP_Open(&ip);
  if (!self.socket) {
    SDL_UnlockMutex(self.socket_mutex);
    error();
    return;
  }
  SDLNet_TCP_AddSocket(self.socket_set, self.socket);

  SDL_CondSignal(self.socket_changed);
  SDL_UnlockMutex(self.socket_mutex);

  ok();
}


/**
 * AT+CIPCLOSE
 */
static void at_cipclose(void) {
  if (self.socket) {
    close_socket();
    respond("CLOSED" CRLF);
  }

  ok();
}


static void send_tx(void) {
  u8_t packet[MAX_PACKET_LENGTH];

  /* Wait for the packet. */
  if (buffer_peek_n(&self.tx, 0, self.length, packet) != self.length) {
    return;
  }

  (void) buffer_read_n(&self.tx, self.length, NULL);

  if (SDLNet_TCP_Send(self.socket, packet, self.length) < self.length) {
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

  (void) buffer_peek_n(&self.tx, 0, 1, &value);
  switch (value) {
    case '=':
      /* Normal transmission mode. */
      (void) buffer_read_n(&self.tx, 1, NULL);
 
      /* Read length. */
      memset(length, 0, sizeof(length));
      while (buffer_read_n(&self.tx, 1, &length[0]) && length[0] == '0');
      for (i = 1; (i < sizeof(length) - 1) && buffer_read_n(&self.tx, 1, &length[i]) && isdigit(length[i]); i++);
      if (i == sizeof(length) - 1) {
        error();
        return;
      }
      length[i] = 0;
      self.length = atoi((const char *) length);
      if (self.length > MAX_PACKET_LENGTH) {
        error();
        return;
      }
      respond(">");
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
    { "+CIPCLOSE", at_cipclose },
    { "+CIPMUX",   ok          },
    { "+CIPSTART", at_cipstart },
    { "+CIPSEND",  at_cipsend  },
    { "",          ok          }
  };
  const size_t n_handlers = sizeof(handlers) / sizeof(*handlers);
  char         prefix[MAX_AT_PREFIX_LENGTH + 1];
  size_t       i, j;

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
      (void) buffer_peek_n(&self.tx, i, 1, &value);
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

  return buffer_read_n(&self.rx, 1, &value) ? value : 0x00;
}


void esp_reset(reset_t reset) {
  self.tx.read_index = 0;
  self.tx.n_elements = 0;
  self.rx.read_index = 0;
  self.rx.n_elements = 0;

  self.tx_handler = idle_tx;

  self.do_echo = 1;

  if (self.socket) {
    close_socket();
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
