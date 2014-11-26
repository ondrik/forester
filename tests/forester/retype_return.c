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


static inline int list_empty(struct list_head *head)
{
	return head->next == head;
}


struct node {
    int                         value;
    struct list_head            linkage;
};

LIST_HEAD(gl_list);

int main()
{
	struct node *node = malloc(sizeof *node);

	node->value = 3;
	//list_add(&node->linkage, &gl_list);
	node->linkage.next = gl_list.next;
	gl_list.next = &node->linkage;



	if (!list_empty(&gl_list))
	{
		struct list_head *pos = NULL;

		struct node *entry = list_entry(pos, struct node, linkage);
		int i = entry->value;
	}

	free(list_entry(gl_list.next, struct node, linkage));

	return 0;
}
