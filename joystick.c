#include <SDL2/SDL.h>
#include "defs.h"
#include "joystick.h"
#include "log.h"


typedef struct {
  SDL_GameController* controller;
  joystick_type_t     type;
  int                 is_pressed_up;
  int                 is_pressed_down;
  int                 is_pressed_left;
  int                 is_pressed_right;
  int                 is_pressed_a;
  int                 is_pressed_b;
  int                 is_pressed_c;
  int                 is_pressed_x;
  int                 is_pressed_y;
  int                 is_pressed_z;
  int                 is_pressed_start;
} entity_t;


typedef struct {
  entity_t entity[2];
} self_t;


static self_t self;


int joystick_init(SDL_GameController* controller_left, SDL_GameController* controller_right) {
  memset(&self.entity[E_JOYSTICK_LEFT],  0, sizeof(entity_t));
  memset(&self.entity[E_JOYSTICK_RIGHT], 0, sizeof(entity_t));

  self.entity[E_JOYSTICK_LEFT ].controller = controller_left;
  self.entity[E_JOYSTICK_RIGHT].controller = controller_right;
  self.entity[E_JOYSTICK_LEFT ].type       = E_JOYSTICK_TYPE_KEMPSTON_1;
  self.entity[E_JOYSTICK_RIGHT].type       = E_JOYSTICK_TYPE_KEMPSTON_2;

  return 0;
}


void joystick_finit(void) {
}


u8_t joystick_kempston_read(joystick_t n) {
  const entity_t* entity = &self.entity[n];

  switch (entity->type) {
    case E_JOYSTICK_TYPE_KEMPSTON_1:
    case E_JOYSTICK_TYPE_KEMPSTON_2:
      return (entity->is_pressed_start << 7)
           | (entity->is_pressed_x     << 6)
           | (entity->is_pressed_b     << 5)
           | (entity->is_pressed_a     << 4)
           | (entity->is_pressed_up    << 3)
           | (entity->is_pressed_down  << 2)
           | (entity->is_pressed_left  << 1)
           |  entity->is_pressed_right;

    default:
      break;
  }

  return 0x00;
}


static void refresh(entity_t* entity) {
  /* TODO: map C and Z. */
  entity->is_pressed_up    = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
  entity->is_pressed_down  = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
  entity->is_pressed_left  = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
  entity->is_pressed_right = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
  entity->is_pressed_a     = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_A);
  entity->is_pressed_b     = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_B);
  entity->is_pressed_x     = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_X);
  entity->is_pressed_y     = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_Y);
  entity->is_pressed_start = SDL_GameControllerGetButton(entity->controller, SDL_CONTROLLER_BUTTON_START);
}


void joystick_refresh(void) {
  if (self.entity[E_JOYSTICK_LEFT ].controller) refresh(&self.entity[E_JOYSTICK_LEFT ]);
  if (self.entity[E_JOYSTICK_RIGHT].controller) refresh(&self.entity[E_JOYSTICK_RIGHT]);
}


joystick_type_t joystick_type_get(joystick_t n) {
  return self.entity[n].type;
}


void joystick_type_set(joystick_t n, joystick_type_t type) {
  self.entity[n].type = type;
}

