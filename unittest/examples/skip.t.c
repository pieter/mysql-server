
#include <tap.h>
#include <stdlib.h>

int main() {
  plan(4);
  ok(1, NULL);
  ok(1, NULL);
  SKIP_BLOCK_IF(1, 2, "No point") {
    ok(1, NULL);
    ok(1, NULL);
  }
  return exit_status();
}
