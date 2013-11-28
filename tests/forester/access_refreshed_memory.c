/*
 * Accessing refreshed memory
 */

#include <stdlib.h>
#include <verifier-builtins.h>

int main() {

	struct T {
		int data;
	};

	struct T* x = malloc(sizeof(*x));
  free(x);
	struct T* y = malloc(sizeof(*y));

  if (x == y)
  {
    x->data = 0;
  }

  free(y);

	return 0;
}

