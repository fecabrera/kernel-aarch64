#include <mm/heap.h>
#include "ordered_set.h"

void ordered_set64_init(struct ordered_set64 *set)
{
    set->root = NULL;
    set->length = 0;
}

static void _ordered_set64_destroy_node(struct ordered_set64_node *node)
{
    if (node == NULL)
        return;
    _ordered_set64_destroy_node(node->left);
    _ordered_set64_destroy_node(node->right);
    kfree(node);
}

void ordered_set64_destroy(struct ordered_set64 *set)
{
    _ordered_set64_destroy_node(set->root);
    set->root = NULL;
    set->length = 0;
}

int ordered_set64_insert(struct ordered_set64 *set, uint64_t value)
{
    struct ordered_set64_node **current = &set->root;

    while (*current != NULL)
    {
        if (value == (*current)->value)
            return 0;
        else if (value < (*current)->value)
            current = &(*current)->left;
        else
            current = &(*current)->right;
    }

    struct ordered_set64_node *node = (struct ordered_set64_node *)kmalloc(sizeof(struct ordered_set64_node));
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    *current = node;
    set->length++;
    return 1;
}

void ordered_set64_remove(struct ordered_set64 *set, uint64_t value)
{
    struct ordered_set64_node **current = &set->root;

    while (*current != NULL)
    {
        if (value == (*current)->value)
        {
            struct ordered_set64_node *node = *current;

            if (node->left == NULL)
            {
                *current = node->right;
                kfree(node);
            }
            else if (node->right == NULL)
            {
                *current = node->left;
                kfree(node);
            }
            else
            {
                // replace with in-order successor: leftmost node in right subtree
                struct ordered_set64_node **successor = &node->right;
                while ((*successor)->left != NULL)
                    successor = &(*successor)->left;

                node->value = (*successor)->value;

                struct ordered_set64_node *to_free = *successor;
                *successor = to_free->right;
                kfree(to_free);
            }

            set->length--;
            return;
        }
        else if (value < (*current)->value)
            current = &(*current)->left;
        else
            current = &(*current)->right;
    }
}

int ordered_set64_contains(struct ordered_set64 *set, uint64_t value)
{
    struct ordered_set64_node *current = set->root;

    while (current != NULL)
    {
        if (value == current->value)
            return 1;
        else if (value < current->value)
            current = current->left;
        else
            current = current->right;
    }

    return 0;
}

uint64_t ordered_set64_min(struct ordered_set64 *set)
{
    struct ordered_set64_node *current = set->root;
    while (current->left != NULL)
        current = current->left;
    return current->value;
}

uint64_t ordered_set64_max(struct ordered_set64 *set)
{
    struct ordered_set64_node *current = set->root;
    while (current->right != NULL)
        current = current->right;
    return current->value;
}

static void _ordered_set64_foreach_node(struct ordered_set64_node *node, void (*fn)(uint64_t, void *), void *ctx)
{
    if (node == NULL)
        return;
    _ordered_set64_foreach_node(node->left, fn, ctx);
    fn(node->value, ctx);
    _ordered_set64_foreach_node(node->right, fn, ctx);
}

void ordered_set64_foreach(struct ordered_set64 *set, void (*fn)(uint64_t, void *), void *ctx)
{
    _ordered_set64_foreach_node(set->root, fn, ctx);
}
