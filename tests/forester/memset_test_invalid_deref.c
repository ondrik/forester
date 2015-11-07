#include <stdlib.h>
#include <string.h>
#include <verifier-builtins.h>

int main()
{
	int *x;

	x = malloc(sizeof(int));

	memset(&x, 0, sizeof(int));

	__VERIFIER_assert(*x == 0);

	free(x);

	return 1;
}
