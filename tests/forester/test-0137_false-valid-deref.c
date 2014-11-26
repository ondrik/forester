extern void __VERIFIER_error() __attribute__ ((__noreturn__));

/*
 * This source code is licensed under the GPL license, see License.GPLv2.txt
 * for details.  The list implementation is taken from the Linux kernel.
 */

#include <stdlib.h>

extern int __VERIFIER_nondet_int(void);

struct list_head {
	struct list_head *next;
};

#define LIST_HEAD_INIT(name) { &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); \
} while (0)

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


struct node {
    int                         value;
    struct list_head            linkage;
};

LIST_HEAD(gl_list);


int main()
{
	do {
		struct node *node = malloc(sizeof *node);

		node->value = __VERIFIER_nondet_int();
		node->linkage.next = gl_list.next;
		gl_list.next = &node->linkage;
	}
	while (__VERIFIER_nondet_int());


	struct list_head *pos, *max_pos = NULL;


	max_pos = gl_list.next;


	pos = (max_pos)->next; 
	struct node *entry = list_entry(pos, struct node, linkage);
	int i = entry->value;


	max_pos = pos;


	struct list_head *next;
	while (&gl_list != (next = gl_list.next)) {
		gl_list.next = next->next;
		free(list_entry(next, struct node, linkage));
	}
	return 0;
}
