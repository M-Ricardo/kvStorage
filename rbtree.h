
// key --> char*

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

//#include "kvengine.h"


#define RED				1
#define BLACK 			2

#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512



typedef char* KEY_TYPE;

//

typedef struct _rbtree_node {
	unsigned char color;
	struct _rbtree_node *right;
	struct _rbtree_node *left;
	struct _rbtree_node *parent;
	

	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];


} rbtree_node;


typedef struct _rbtree {
	rbtree_node *root;
	rbtree_node *nil;
	
	pthread_mutex_t lock;
	int nb_elements;
} rbtree;

typedef rbtree rbtree_t;
typedef rbtree_node rbtree_node_t;


rbtree_t tree;



rbtree_node *rbtree_mini(rbtree *T, rbtree_node *x) {
	while (x->left != T->nil) {
		x = x->left;
	}
	return x;
}

rbtree_node *rbtree_maxi(rbtree *T, rbtree_node *x) {
	while (x->right != T->nil) {
		x = x->right;
	}
	return x;
}

rbtree_node *rbtree_successor(rbtree *T, rbtree_node *x) {
	rbtree_node *y = x->parent;

	if (x->right != T->nil) {
		return rbtree_mini(T, x->right);
	}

	while ((y != T->nil) && (x == y->right)) {
		x = y;
		y = y->parent;
	}
	return y;
}


void rbtree_left_rotate(rbtree *T, rbtree_node *x) {

	rbtree_node *y = x->right;  // x  --> y  ,  y --> x,   right --> left,  left --> right

	x->right = y->left; //1 1
	if (y->left != T->nil) { //1 2
		y->left->parent = x;
	}

	y->parent = x->parent; //1 3
	if (x->parent == T->nil) { //1 4
		T->root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}

	y->left = x; //1 5
	x->parent = y; //1 6
}


void rbtree_right_rotate(rbtree *T, rbtree_node *y) {

	rbtree_node *x = y->left;

	y->left = x->right;
	if (x->right != T->nil) {
		x->right->parent = y;
	}

	x->parent = y->parent;
	if (y->parent == T->nil) {
		T->root = x;
	} else if (y == y->parent->right) {
		y->parent->right = x;
	} else {
		y->parent->left = x;
	}

	x->right = y;
	y->parent = x;
}

void rbtree_insert_fixup(rbtree *T, rbtree_node *z) {

	while (z->parent->color == RED) { //z ---> RED
		if (z->parent == z->parent->parent->left) {
			rbtree_node *y = z->parent->parent->right;
			if (y->color == RED) {
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;

				z = z->parent->parent; //z --> RED
			} else {

				if (z == z->parent->right) {
					z = z->parent;
					rbtree_left_rotate(T, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rbtree_right_rotate(T, z->parent->parent);
			}
		}else {
			rbtree_node *y = z->parent->parent->left;
			if (y->color == RED) {
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;

				z = z->parent->parent; //z --> RED
			} else {
				if (z == z->parent->left) {
					z = z->parent;
					rbtree_right_rotate(T, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rbtree_left_rotate(T, z->parent->parent);
			}
		}
		
	}

	T->root->color = BLACK;
}


void rbtree_insert(rbtree *T, rbtree_node *z) {

	rbtree_node *y = T->nil;
	rbtree_node *x = T->root;
	
// strcmp(s1, s2)  -1, 0, 1

	while (x != T->nil) {
		y = x;

		int ret = strcmp(z->key, x->key);
		if (ret < 0) {
			x = x->left;
		} else if (ret > 0) {
			x = x->right;
		} else {  //exist
			return ;
		}



	}


	z->parent = y;
	if (y == T->nil) {
		T->root = z;
	} else if (strcmp(z->key, y->key) < 0) {
		y->left = z;
	} else {
		y->right = z;
	}



	z->left = T->nil;
	z->right = T->nil;
	z->color = RED;

	rbtree_insert_fixup(T, z);

}

void rbtree_delete_fixup(rbtree *T, rbtree_node *x) {

	while ((x != T->root) && (x->color == BLACK)) {
		if (x == x->parent->left) {

			rbtree_node *w= x->parent->right;
			if (w->color == RED) {
				w->color = BLACK;
				x->parent->color = RED;

				rbtree_left_rotate(T, x->parent);
				w = x->parent->right;
			}

			if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
				w->color = RED;
				x = x->parent;
			} else {

				if (w->right->color == BLACK) {
					w->left->color = BLACK;
					w->color = RED;
					rbtree_right_rotate(T, w);
					w = x->parent->right;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->right->color = BLACK;
				rbtree_left_rotate(T, x->parent);

				x = T->root;
			}

		} else {

			rbtree_node *w = x->parent->left;
			if (w->color == RED) {
				w->color = BLACK;
				x->parent->color = RED;
				rbtree_right_rotate(T, x->parent);
				w = x->parent->left;
			}

			if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
				w->color = RED;
				x = x->parent;
			} else {

				if (w->left->color == BLACK) {
					w->right->color = BLACK;
					w->color = RED;
					rbtree_left_rotate(T, w);
					w = x->parent->left;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->left->color = BLACK;
				rbtree_right_rotate(T, x->parent);

				x = T->root;
			}

		}
	}

	x->color = BLACK;
}

rbtree_node *rbtree_delete(rbtree *T, rbtree_node *z) {

	rbtree_node *y = T->nil;
	rbtree_node *x = T->nil;

	if ((z->left == T->nil) || (z->right == T->nil)) {
		y = z;
	} else {
		y = rbtree_successor(T, z);
	}

	if (y->left != T->nil) {
		x = y->left;
	} else if (y->right != T->nil) {
		x = y->right;
	}

	x->parent = y->parent;
	if (y->parent == T->nil) {
		T->root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}

	if (y != z) {

		strncpy(z->key , y->key, MAX_KEY_LEN);
		strncpy(z->value, y->value, MAX_VALUE_LEN);

	}

	if (y->color == BLACK) {
		rbtree_delete_fixup(T, x);
	}

	return y;
}


rbtree_node *rbtree_search(rbtree *T, KEY_TYPE key) {

	rbtree_node *node = T->root;
	
	while (node != T->nil) {
		
		
		int ret = strcmp(key, node->key);
		if (ret < 0) {
			node = node->left;
		} else if (ret > 0) {
			node = node->right;
		} else {
			return node;
		}	

	}
	return T->nil;
}


void rbtree_traversal(rbtree *T, rbtree_node *node) {
	if (node != T->nil) {
		rbtree_traversal(T, node->left);

		printf("key:%s, value:%s\n", node->key, node->value);

		rbtree_traversal(T, node->right);
	}
}


// tree
int init_rbtree(rbtree_t *tree) {

	if (!tree) return -1;

	tree->nil = (rbtree_node*)malloc(sizeof(rbtree_node));
	memset(tree->nil, 0, sizeof(rbtree_node));
	tree->nil->color = BLACK;
	
	tree->nil->left = tree->nil;
	tree->nil->right = tree->nil;
	
	tree->root = tree->nil;
	tree->nb_elements = 0;

	pthread_mutex_init(&tree->lock, NULL);

	return 0;
} 

void dest_rbtree(rbtree_t *tree) {

	if (!tree) return ;

	rbtree_node_t *node = tree->root;
	while (node != tree->nil) {
		
        node = rbtree_mini(tree, tree->root);
        if (node == tree->nil) break;
        
        pthread_mutex_lock(&tree->lock);
		node = rbtree_delete(tree, node);
		
        pthread_mutex_unlock(&tree->lock);
		
		free(node);
	}

	free(tree->nil);

}


int put_kv_rbtree(rbtree_t *tree, char *key, char *value) {

	if (!tree || !key || !value) return -1;

	rbtree_node *node = rbtree_search(tree, key);
	if (node != tree->nil) { // key exist;
		return 1;
	}

	node = (rbtree_node*)malloc(sizeof(rbtree_node));
	if (!node) return -2;
	memset(node, 0, sizeof(rbtree_node));
	node->parent = tree->nil;
	
	strncpy(node->key, key, MAX_KEY_LEN);
	strncpy(node->value, value, MAX_VALUE_LEN);

	pthread_mutex_lock(&tree->lock);
	rbtree_insert(tree, node);
	tree->nb_elements ++;
	pthread_mutex_unlock(&tree->lock);

	return 0;

}


char* get_kv_rbtree(rbtree_t *tree, char *key) { //

	if (!tree || !key) return NULL;

	rbtree_node *node = rbtree_search(tree, key);

	return node->value;

}


int count_kv_rbtree(rbtree_t *tree) {
	return tree->nb_elements;
}

int exist_kv_rbtree(rbtree_t *tree, char *key) {
	
	if (!tree || !key) return -1;

	rbtree_node *node = rbtree_search(tree, key);
	if (node != tree->nil) { // key exist;
		return 1;
	}

	return 0;
}


int delete_kv_rbtree(rbtree_t *tree, char *key) {

	if (!tree || !key) return -1;

	rbtree_node *node = rbtree_search(tree, key);
	if (node == tree->nil) { // key no exist;
		return 1;
	}

	rbtree_node *tmp = NULL;
	pthread_mutex_lock(&tree->lock);
	tmp = rbtree_delete(tree, node);
	tree->nb_elements --;
	pthread_mutex_unlock(&tree->lock);

	free(tmp);

	return 0;

}

#if 0

int main() {

	init_rbtree(&tree);

	char *k1 = "Name";
	char *v1 = "King";
	int res = put_kv_rbtree(&tree, k1, v1);
	printf("res: %d\n\n", res);

	char *k2 = "Content-Type";
	char *v2 = "text/html;charset=utf-8";
	res = put_kv_rbtree(&tree, k2, v2);
	printf("res: %d\n\n", res);

	char *k3 = "Server";
	char *v3 = "Nginx";
	res = put_kv_rbtree(&tree, k3, v3);
	printf("res: %d\n\n", res);

	printf("\n---> traversal\n");
	rbtree_traversal(&tree, tree.root);
	printf("<--- traversal\n\n");

	res = put_kv_rbtree(&tree, k1, v1);
	printf("res: %d\n\n", res);

	printf("---> traversal\n");
	rbtree_traversal(&tree, tree.root);
	printf("<--- traversal\n\n");

	char *k4 = "Connection";
	char *v4 = "keep-alive";
	res = put_kv_rbtree(&tree, k4, v4);
	printf("res: %d\n\n", res);

	char *k5 = "Pragma";
	char *v5 = "no-cache";
	res = put_kv_rbtree(&tree, k5, v5);
	printf("res: %d\n\n", res);

	char *k6 = "Content-Encoding";
	char *v6 = "gzip";
	res = put_kv_rbtree(&tree, k6, v6);
	printf("res: %d\n\n", res);

	

}

#endif

