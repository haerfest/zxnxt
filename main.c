#include "clock.h"
#include "cpu.h"


int main(int argc, char* argv[]) {
  clock_init();
  cpu_init();

  return 0;
}
