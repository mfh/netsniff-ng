/*
 * netsniff-ng - the packet sniffing beast
 * By Daniel Borkmann <daniel@netsniff-ng.org>
 * Copyright 2011 Daniel Borkmann
 * Copyright 1991-2007 Kawahara Lab., Kyoto University
 * Copyright 2000-2005 Shikano Lab., Nara Institute of Science and Technology
 * Copyright 2005-2007 Julius project team, Nagoya Institute of Technology
 * All rights reserved
 * Subject to the GPL.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "patricia.h"
#include "compiler.h"
#include "xmalloc.h"

static unsigned char mbit[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

static inline int testbit(char *str, size_t slen, int bitplace)
{
	int maskptr;
	if ((maskptr = bitplace >> 3) > slen)
		return 0;
	return (str[maskptr] & mbit[bitplace & 7]);
}

static inline int testbit_max(char *str, int bitplace, int maxbitplace)
{
	if (bitplace >= maxbitplace)
		return 0;
	return (str[bitplace >> 3] & mbit[bitplace & 7]);
}

int where_the_bit_differ(char *str1, char *str2)
{
	int p = 0, bitloc, slen1, slen2;
	while (str1[p] == str2[p])
		p++;
	bitloc = p * 8;
	slen1 = strlen(str1);
	slen2 = strlen(str2);
	while (testbit(str1, slen1, bitloc) ==
	       testbit(str2, slen2, bitloc))
		bitloc++;
	return bitloc;
}

static struct patricia_node *new_node(void)
{
	struct patricia_node *n = xzmalloc(sizeof(*n));
	n->l = n->r = NULL;
	return n;
}

static void free_node(struct patricia_node *n)
{
	if (n->key)
		xfree(n->key);
	xfree(n);
}

void ptree_display(struct patricia_node *node, int level)
{
	int i;
	for (i = 0; i < level; ++i)
		printf("-");
	if (node->l == NULL && node->r == NULL)
		printf("leaf: (%s -> %d)\n", node->key, node->value.data);
	else {
		printf("thres: %d\n", node->value.thres_bit);
		if (node->l != NULL)
			ptree_display(node->l, level + 1);
		if (node->r != NULL)
			ptree_display(node->r, level + 1);
	}
}

static int ptree_search_data_r(struct patricia_node *node, char *str,
			       int maxbitplace)
{
	if (node->l == NULL && node->r == NULL)
		return node->value.data;
	if (testbit_max(str, node->value.thres_bit, maxbitplace) != 0)
		return ptree_search_data_r(node->r, str, maxbitplace);
	else
		return ptree_search_data_r(node->l, str, maxbitplace);
}

static int *ptree_search_data_r_p(struct patricia_node *node, char *str,
			          int maxbitplace)
{
	if (node->l == NULL && node->r == NULL)
		return &(node->value.data);
	if (testbit_max(str, node->value.thres_bit, maxbitplace) != 0)
		return ptree_search_data_r_p(node->r, str, maxbitplace);
	else
		return ptree_search_data_r_p(node->l, str, maxbitplace);
}

static int ptree_search_data_r_x(struct patricia_node *node, char *str,
				 int maxbitplace)
{
	if (node->l == NULL && node->r == NULL) {
		if (node->klen == strlen(str) &&
		    !strncmp(str, node->key, node->klen))
			return node->value.data;
		else
			return -ENOENT;
	}
	if (testbit_max(str, node->value.thres_bit, maxbitplace) != 0)
		return ptree_search_data_r_x(node->r, str, maxbitplace);
	else
		return ptree_search_data_r_x(node->l, str, maxbitplace);
}

int ptree_search_data_nearest(char *str, struct patricia_node *root)
{
	if (!root)
		return -ENOENT;
	return ptree_search_data_r(root, str, strlen(str) * 8 + 8);
}

static int *ptree_search_data_nearest_ptr(char *str, struct patricia_node *root)
{
	return ptree_search_data_r_p(root, str, strlen(str) * 8 + 8);
}


int ptree_search_data_exact(char *str, struct patricia_node *root)
{
	if (!root)
		return -ENOENT;
	return ptree_search_data_r_x(root, str, strlen(str) * 8 + 8);
}

struct patricia_node *ptree_make_root_node(char *str, int data)
{
	struct patricia_node *n = new_node();
	n->value.data = data;
	n->key = xstrdup(str);
	n->klen = strlen(str);
	return n;
}

static void ptree_add_entry_at(char *str, size_t slen, int bitloc, int data,
			       struct patricia_node **parentlink)
{
	struct patricia_node *node = (*parentlink);
	if (node->value.thres_bit > bitloc ||
	    (node->l == NULL && node->r == NULL)) {
		struct patricia_node *newleaf, *newbranch;
		newleaf = new_node();
		newleaf->value.data = data;
		newleaf->key = xstrdup(str);
		newleaf->klen = slen;
		newbranch = new_node();
		newbranch->value.thres_bit = bitloc;
		(*parentlink) = newbranch;
		if (testbit(str, slen, bitloc) ==0) {
			newbranch->l = newleaf;
			newbranch->r = node;
		} else {
			newbranch->l = node;
			newbranch->r = newleaf;
		}
		return;
	} else {
		if (testbit(str, slen, node->value.thres_bit) != 0)
			ptree_add_entry_at(str, slen, bitloc, data, &(node->r));
		else
			ptree_add_entry_at(str, slen, bitloc, data, &(node->l));
	}
}

void ptree_add_entry_exm(char *str, int data, char *matchstr,
			 struct patricia_node **root)
{
	if (!(*root))
		(*root) = ptree_make_root_node(str, data);
	else {
		int bitloc = where_the_bit_differ(str, matchstr);
		ptree_add_entry_at(str, strlen(str), bitloc, data, root);
	}
}

void ptree_add_entry(char *str, int data, struct patricia_node **root)
{
	int *ptr, bitloc;
	char *matchstr;
	struct patricia_node *n;

	if (!(*root))
		(*root) = ptree_make_root_node(str, data);
	else {
		ptr = ptree_search_data_nearest_ptr(str, (*root));
		n = container_of(ptr, struct patricia_node, value.data);
		matchstr = n->key;
		if (n->klen == strlen(str) && !strncmp(str, n->key, n->klen))
			return;
		bitloc = where_the_bit_differ(str, matchstr);
		ptree_add_entry_at(str, strlen(str), bitloc, data, root);
	}
}

static void ptree_remove_entry_r(struct patricia_node *now,
				 struct patricia_node *up,
				 struct patricia_node *up2,
				 char *str, int maxbitplace,
				 struct patricia_node **root)
{
	struct patricia_node *b;

	if (now->l == NULL && now->r == NULL) {
		if (strncmp(now->key, str, now->klen))
			return;
		if (up == NULL) {
			*root = NULL;
			free_node(now);
			return;
		}
		b = (up->r == now) ? up->l : up->r;
		if (up2 == NULL) {
			*root = b;
			free_node(now);
			free_node(up);
			return;
		}
		if (up2->l == up)
			up2->l = b;
		else
			up2->r = b;
		free_node(now);
		free_node(up);
		return;
	} else {
		if (testbit_max(str, now->value.thres_bit, maxbitplace) != 0)
			ptree_remove_entry_r(now->r, now, up, str,
					     maxbitplace, root);
		else
			ptree_remove_entry_r(now->l, now, up, str,
					     maxbitplace, root);
	}
}

void ptree_del_entry(char *str, struct patricia_node **root)
{
	if (!(*root))
		return;
	ptree_remove_entry_r(*root, NULL, NULL, str, strlen(str) * 8 + 8, root);
}

void ptree_free(struct patricia_node *node)
{
	if (!node)
		return;
	if (node->l)
		ptree_free(node->l);
	if (node->r)
		ptree_free(node->r);
	free_node(node);
}
