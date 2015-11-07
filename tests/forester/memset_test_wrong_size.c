#include <stdlib.h>
#include <string.h>
#include <verifier-builtins.h>

int main()
{
	int *x;

	x = malloc(sizeof(int));

	memset(x, 1, 2*sizeof(int));

	__VERIFIER_assert(*x == 1);

	free(x);

	return 1;
}
