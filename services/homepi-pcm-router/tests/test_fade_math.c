#include <assert.h>
#include <math.h>
#include <stdio.h>

static double fade_gain(unsigned int position, unsigned int total) {
  if (total == 0) {
    return 1.0;
  }
  const double t = (double)position / (double)total;
  return 1.0 - t;
}

int main(void) {
  assert(fade_gain(0, 10) == 1.0);
  assert(fade_gain(10, 10) == 0.0);
  assert(fabs(fade_gain(5, 10) - 0.5) < 0.0001);
  printf("test_fade_math: OK\n");
  return 0;
}
