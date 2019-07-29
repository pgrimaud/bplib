/************************************************************************
 * File: rb_tree.c
 *
 *  Copyright 2019 United States Government as represented by the
 *  Administrator of the National Aeronautics and Space Administration.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * Maintainer(s):
 *  Alexander Meade, Code 582 NASA GSFC
 *
 *************************************************************************/


/******************************************************************************
 INCLUDES
 ******************************************************************************/ 

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "rb_tree.h"

/******************************************************************************
 DEFINES
 ******************************************************************************/ 

/* The maximum number of allowable rb_node_ts. This can never be larger than one more
   than the max data size because since we are representing ranges. Thus in the worse case
   scenario where values are added one apart so that a new node is added for each insertion
   once we reach half + 1 nodes then merging will occur. */
#define MAX_TREE_SIZE ((UINT32_MAX  / 2) + 1) 
#define RED   true  /* Boolean representing a red rb_node_t. */
#define BLACK false /* Boolean representing a black rb_node_t, */

/******************************************************************************
 LOCAL FUNCTIONS
 ******************************************************************************/ 

/*--------------------------------------------------------------------------------------
 * pop_free_node - Retrieves a block of unallocated memory to assign a rb_node_t if available. 
 *
 * tree: A ptr to a rb_tree_t. [OUTPUT]
 * returns: A ptr to a free block of memory to allocate an rb_node_t. If no space is available
 *      a NULL ptr is returned. If the ptr is non NULL the tree's size is incremented.
 *-------------------------------------------------------------------------------------*/
static rb_node_t* pop_free_node(rb_tree_t* tree)
{
    rb_node_t* free_node = tree->free_node_tail;
    
    if (free_node != NULL) {
        tree->free_node_tail = free_node->left;
        if (tree->free_node_tail == NULL)
        {
            tree->free_node_head = NULL;
        }
        tree->size += 1;
    }    
    return free_node;
}

/*--------------------------------------------------------------------------------------
 * push_free_node - Recovers a block of unallocated memory for assigning future rb_node_ts to.
 *
 * tree: A ptr to a rb_tree_t. The trees size is decremented after pushing the node. [OUTPUT] 
 * node: A ptr to a rb_node_t to reassign that can now be treated as unallocated memory.
 *-------------------------------------------------------------------------------------*/
static void push_free_node(rb_tree_t* tree, rb_node_t* node)
{
    // Push the node into the queue.
    node->right = tree->free_node_head;
    node->left = NULL;
    tree->size -= 1;

    if (tree->free_node_head == NULL) {
        // If head and tail are currently NULL make this node both.
        tree->free_node_head = node;
        tree->free_node_tail = node;
    }
    else 
    {
        // If head exists reassign head to point to this new node.
        tree->free_node_head->left = node;
        tree->free_node_head = node;
    }
}

/*--------------------------------------------------------------------------------------
 * set_black -
 *
 * node: A ptr to an rb_node_t to set the color to black. [OUTPUT] 
 *-------------------------------------------------------------------------------------*/
static inline void set_black(rb_node_t* node)
{
    node->color = BLACK;
}

/*--------------------------------------------------------------------------------------
 * set_red -
 *
 * node: A ptr to an rb_node_t to set the color to red. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static inline void set_red(rb_node_t* node)
{
    node->color = RED;
}


/*--------------------------------------------------------------------------------------
 * is_black -
 *
 * node: A rb_node_t to check its color. [INPUT]
 * returns: Whether or not the provided rb_node_t is black.  
 *-------------------------------------------------------------------------------------*/
static inline bool is_black(rb_node_t* node)
{
    return node == NULL || node->color == BLACK;
}

/*--------------------------------------------------------------------------------------
 * is_red -
 *
 * node: A rb_node_t to check its color. [INPUT]
 * returns: Whether or not the provided rb_node_t is red.  
 *-------------------------------------------------------------------------------------*/
static inline bool is_red(rb_node_t* node)
{
    return node != NULL && node->color == RED;
}


/*--------------------------------------------------------------------------------------
 * get_grandparent -
 *
 * node: A ptr to an rb_node_t to get its grandparent. [INPUT]
 * returns: A ptr to the node grandparent or NULL
 *-------------------------------------------------------------------------------------*/
static inline rb_node_t* get_grandparent(rb_node_t* node)
{
    return node->parent == NULL ? NULL : node->parent->parent;
}

/*--------------------------------------------------------------------------------------
 * is_root -
 *
 * node: A ptr to an rb_node_t to check if it is the root of an red black tree. [INPUT]
 * returns: Whether or not the provided node is the tree root.  
 *-------------------------------------------------------------------------------------*/
static inline bool is_root(rb_node_t* node)
{
    return node->parent == NULL;
}

/*--------------------------------------------------------------------------------------
 * is_left_child -
 *
 * node: A ptr to an rb_node_t to check if it is the left child of its parent node. [INPUT]
 * returns: Whether or not the provided node is the left child of its parent. If the
 *      provided node is root then this returns false. 
 *-------------------------------------------------------------------------------------*/
static inline bool is_left_child(rb_node_t* node)
{
    return !is_root(node) && node == node->parent->left;
}

/*--------------------------------------------------------------------------------------
 * get_sibling - A sibling is defined as a parents other child within the rb_tree_t.
 *
 * node: A ptr to an rb_node_t to get its sibling. [INPUT]
 * returns: A ptr to the sibling or NULL of node.
 *-------------------------------------------------------------------------------------*/
static inline rb_node_t* get_sibling(rb_node_t* node)
{
    return node->parent == NULL ? NULL : /* If parent is NULL return NULL else. */
           is_left_child(node)  ? node->parent->right : node->parent->left; /* Return left child if it exists. Else return right child, which may be NULL. */
}

/*--------------------------------------------------------------------------------------
 * get_uncle - An uncle within an rb_tree is a nodes parent's parent's sibling.
 *
 * node: A ptr to an rb_node to get its uncle. [INPUT]
 * returns: A ptr to the nodes uncle or or NULL.  
 *-------------------------------------------------------------------------------------*/
static inline rb_node_t* get_uncle(rb_node_t* node)
{
    return node->parent == NULL          ? NULL : /* If parent is NULL Return NULL. */
           get_grandparent(node) == NULL ? NULL : get_sibling(node->parent); /* If grandparent is NULL return NULL. Else return parent's sibling. */
}

/*--------------------------------------------------------------------------------------
 * has_left_child -
 *
 * node: A ptr to an rb_node_t to check if it has a left child. [INPUT]
 * returns: Whether or not the provided node has a non NULL left child.   
 *-------------------------------------------------------------------------------------*/
static inline bool has_left_child(rb_node_t* node)
{
    return node->left != NULL;
}

/*--------------------------------------------------------------------------------------
 * has_right_child -
 *
 * node: A ptr to an rb_node_t to check if it has a right child. [INPUT]
 * returns: Whether or not the provided node has a non NULL right child.   
 *-------------------------------------------------------------------------------------*/
static inline bool has_right_child(rb_node_t* node)
{
    return node->right != NULL;
}

/*--------------------------------------------------------------------------------------
 * remove_from_parent - Removes references to a node from its parent.
 * 
 * node: A ptr to an rb_node_t to remove references to it in its parent. [OUTPUT]
 *--------------------------------------------------------------------------------------*/  
static void remove_from_parent(rb_node_t* node)
{
    if (is_root(node))
    {
        return;
    }
    /* If node is not root we need to remove its parents references to it. */
    if (is_left_child(node))
    {
        /* If node is a left child of parent set parent's left child to NULL. */
        node->parent->left = NULL;
    }
    else
    {
        /* If node is a right child of parent set parent's right child to NULL. */
        node->parent->right = NULL;
    }
}

/*--------------------------------------------------------------------------------------
 * swap_parents - Swaps the parents of two nodes which are currently parent and child
 *      of eachother.
 *
 * Example:
 *
 *     3                                     3
 *    / \                                   / \
 *   1   6      swap_parents(6, 8) --->    1   8
 *        \                                   / 
 *         8                                 6    
 *
 * node_1: A ptr to a rb_node_t to move down in the tree when swapping parents. [OUTPUT]
 * node_2: A ptr to a rb_node_t that was previously the child of node_1 and should be moved up
 *      in the tree when swapping parents. [OUTPUT]
 * tree: A ptr to the red black to within which to swap parents. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void swap_parents(rb_node_t* node_1, rb_node_t* node_2, rb_tree_t* tree)
{
    node_2->parent = node_1->parent;

    if (is_root(node_1))
    {
        tree->root = node_2;
    }
    else if (is_left_child(node_1))
    {
        node_1->parent->left = node_2;
    }
    else
    {
        node_1->parent->right = node_2;
    }
    
    node_1->parent = node_2;
}

/*--------------------------------------------------------------------------------------
 * rotate_left - Rotates the red black tree left around a given node.
 *
 * Example:
 *          
 *         5                                    5
 *        /  \                                 /  \
 *       0   15     rotate_left(5, 15) --->   0   20 
 *          /  \                                 /  \
 *        10   20                               15   22
 *            /  \                             /  \
 *           17  22                           10  17
 * tree: A ptr to a rb_tree_t to rotate. [OUTPUT]
 * node: A ptr to a rb_node_t about which to rotate the red black tree left. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void rotate_left(rb_tree_t* tree, rb_node_t* node)
{
    rb_node_t* new_parent = node->right;
    node->right = new_parent->left;
    new_parent->left = node;

    if (has_right_child(node))
    {
        node->right->parent = node;
    }
    swap_parents(node, new_parent, tree);
}

/*--------------------------------------------------------------------------------------
 * rotate_right - Rotates the red black tree right around a given node.
 *
 * Example:
 *          
 *         5                                     5 
 *        /  \                                  /  \
 *       0   15     rotate_right(5, 15) --->   0   10 
 *          /  \                                  /  \
 *        10    20                               7   15
 *       / \                                         / \
 *      7  13                                       13  20
 * tree: A ptr to a rb_tree_t to rotate. [OUTPUT]
 * node: A ptr to a rb_node_t about which to rotate the red black tree right. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void rotate_right(rb_tree_t* tree, rb_node_t* node)
{
    rb_node_t* new_parent = node->left;
    node->left = new_parent->right;
    new_parent->right = node;

    if (has_left_child(node))
    {
        node->left->parent = node;
    }
    
    swap_parents(node, new_parent, tree);
}

/*-------------------------------------------------------------------------------------
 * create_black_node - Creates a black colored rb_node with value and with no children.
 *
 * value: The value to assign to the new rb_node. [INPUT]
 * color: A boolean indicating the color of the newly created node. [INPUT]
 * tree: A ptr to a rb_tree_t from which to obtain free memory  the new node. [OUTPUT]
 * returns: A ptr to a new rb_node_t or NULL if no memory exists in the rb_tree_t.
 *-------------------------------------------------------------------------------------*/
static rb_node_t* create_rb_node(uint32_t value, bool color, rb_tree_t* tree)
{
    rb_node_t* node = pop_free_node(tree);
    if (node == NULL)
    {
        /* Return early since no memory exists within the tree to create a new node. */
        return NULL;
    }
    node->range.value = value;
    node->range.offset = 0;
    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;
    node->color = color;
    node->traversal_state = false;
    return node;
}

/*-------------------------------------------------------------------------------------
 * insert_child - Creates a parent and child relationship between two nodes.
 *
 * node: A ptr to the node to insert as a child. [OUTPUT]
 * parent: A ptr to the parent of node. [OUTPUT]
 * child_ptr: A ptr to a ptr owned by parent that will be assigned to its new child node. 
 *      This ptr should correspond to either parent->left or parent->right. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void insert_child(rb_node_t* node,
                         rb_node_t* parent,
                         rb_node_t** child_ptr)
{
    node->parent = parent;
    *child_ptr = node;
}

/*-------------------------------------------------------------------------------------
 * get_left_successor - Gets the successor node in the left subtree of the provided node.
 *      The left successor is defined as the right most node in the left subtree.
 *
 * Example:
 *         10
 *        /  \
 *       7    13    get_left_successor(10) --> 5.
 *        \
 *         5
 *
 * node: A ptr to the node to obtain its successor. [INPUT]
 * returns: A ptr to the successor node with a value less than that contained by node. If
 *      no successor exists, NULL is returned.
 *-------------------------------------------------------------------------------------*/
static rb_node_t* get_left_successor(rb_node_t* node)
{
    if (!has_left_child(node))
    {
        return NULL;
    }

    rb_node_t* successor = node->left;
    while (has_right_child(successor))
    {
        successor = successor->right;
    }
    return successor;
}

/*-------------------------------------------------------------------------------------
 * get_right_successor - Gets the successor node in the left subtree of the provided node.
 *      The right successor is defined as the left most node in the right subtree.
 *
 * Example:
 *         10
 *        /  \
 *       7    14    get_right_successor(10) --> 12.
 *            /
 *           12
 *
 * node: A ptr to the node to obtain its successor. [INPUT]
 * returns: A ptr to the successor node with a value less than that contained by node. If
 *      no successor exists, NULL is returned.
 *-------------------------------------------------------------------------------------*/
static rb_node_t* get_right_successor(rb_node_t* node)
{
    if (!has_right_child(node))
    {
        return NULL;
    }

    rb_node_t* successor = node->right;
    while (has_left_child(successor))
    {
        successor = successor->left;
    }
    return successor;
}

/*-------------------------------------------------------------------------------------
 * gets_successor - Gets the left or right successor node of a given node.
 *
 * node: A ptr to the node to obtain its successor. [INPUT]
 * returns: A ptr to the successor node. If no successor exists, NULL is returned.
 *-------------------------------------------------------------------------------------*/
static rb_node_t* get_successor(rb_node_t* node)
{
    rb_node_t* successor = get_left_successor(node);
    if (successor == NULL)
    {
        /* The left subtree has no successor. Check the right subtree. */
        successor = get_right_successor(node);
    }
    return successor;
}

/*-------------------------------------------------------------------------------------
 * swap_values - Swaps the values stored by two nodes.
 *
 * n1: A ptr to the first rb_node_t to swap its value. [OUTPUT]
 * n2: A ptr to the second rb_node_t to swap its value. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void swap_values(rb_node_t* n1, rb_node_t* n2)
{
    uint32_t temp = n1->range.value;
    n1->range.value = n2->range.value;
    n2->range.value = temp;
}

/*-------------------------------------------------------------------------------------
 * swap_offsets - Swaps the offsets stored by two nodes.
 *
 * n1: A ptr to the first rb_node_t to swap its offset. [OUTPUT]
 * n2: A ptr to the second rb_node_t to swap its offset. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void swap_offsets(rb_node_t* n1, rb_node_t* n2)
{
    uint32_t temp = n1->range.offset;
    n1->range.offset = n2->range.offset;
    n2->range.offset = temp;
}

/*-------------------------------------------------------------------------------------
 * replace_node - Replaces a node with its child.
 *
 * node: A ptr to the rb_node_t to replace with its child. [OUTPUT]
 * child: A ptr to an rb_node_t that is a child of node and will replace it. [OUTPUT]
 *-------------------------------------------------------------------------------------*/
static void replace_node(rb_node_t* node, rb_node_t* child)
{
    rb_node_t* parent = node->parent;

    if (is_left_child(node))
    {
        /* Node is the left child of its parent. */
        parent->left = child;
    }
    else
    {
        /* Node is the right child of its parent. */
        parent->right = child;
    }

    if (child != NULL)
    {
        /* Child node is a leaf. In this case there is no need to assign its parent. */
        child->parent = parent;
    }
}

/*--------------------------------------------------------------------------------------
 * delete_rebalance - Rebalance the rb_tree_t to account for the deletion.
 *
 * tree: A ptr to the rb_tree_t to rebalance. [OUTPUT]
 * node: A ptr to the rb_node_t from which to start the rebalancing operation. [INPUT]
 *
 *--------------------------------------------------------------------------------------*
 * CITATION: The code in this function was adapted from the following source.
 *
 * Wikipedia contributors. (2019, June 16). Red–black tree. In Wikipedia, The Free Encyclopedia.
 * Retrieved 14:34, June 20, 2019,
 * from https://en.wikipedia.org/w/index.php?title=Red%E2%80%93black_tree&oldid=902059008
 *--------------------------------------------------------------------------------------*/ 
static void delete_rebalance(rb_tree_t* tree, rb_node_t* node) {

    /* Allocate parent and sibling ptrs. */
    rb_node_t* sibling;
    rb_node_t* parent;

    /* DELETE_CASE_1 - If node is ever set to root rebalancing for deletion is complete. */
    while (!is_root(node))
    {

        /*-----------------------------------------------------------------------------------
        * DELETE_CASE_2 - If sibling is red the colors of it and parent are swapped and
        *      and the tree is rotated such that sibling becomes node's grandparent. This stage
        *      prepares the tree for the follow on cases, 3, 4, 5 & 6.
        *     
        *          BP                   BS
        *         / \                  /  \
        *        BN RS         -->    RP  BSR
        *           / \              / \
        *          BSL BSR          BN  BSL
        *
        *----------------------------------------------------------------------------------*/
        sibling = get_sibling(node);
        parent = node->parent;

        if (is_red(sibling))
        {
            /* Swap colors between parent and sibling. */
            set_red(parent);
            set_black(sibling);

            /* Rotate sibling into the grandparent position. */
            if (is_left_child(node))
            {
                rotate_left(tree, parent);
            }
            else
            {
                rotate_right(tree, parent);
            }
        }
        /* END DELETE_CASE_2 */

        /*-----------------------------------------------------------------------------------
        * DELETE_CASE_3 - If parent, its siblings and its children are black then sibling can
        *      be recolored such that pathes through sibling have one less black node which 
        *      allows us to delete node and maintain balance in tree. Pathes through parent 
        *      however now have one fewer black node than other pathes and so we must return
        *      to DELETE_CASE_1 to fix parent. If nodes are not recolored as described above,
        *      this function casecades
        *      into cases 4, 5 & 6.
        *
        *     
        *          BP                   BP
        *         / \                  /  \
        *        BN BS         -->    BN   RS
        *           / \                    / \
        *          BSL BSR                BSL BSR
        *----------------------------------------------------------------------------------*/
        sibling = get_sibling(node);
        parent = node->parent;
        if (is_black(parent) &&
            is_black(sibling) && 
            is_black(sibling->left) &&
            is_black(sibling->right))
        {
            set_red(sibling);
            node = parent;
            continue; 
        }
        /* END DELETE_CASE_3 */

        /*-----------------------------------------------------------------------------------
        * DELETE_CASE_4 - Parent is red but sibling and its children are black. In this case
        *      the colors of parent and sibling can be swapped and node can then be deleted 
        *      preserving black depth. If this is not the case then this function casecades 
        *      in cases 5 & 6. 
        *     
        *          RP                   BP
        *         / \                  /  \
        *        BN BS         -->    BN   RS
        *           / \                    / \
        *          BSL BSR               BSL  BSR
        *-----------------------------------------------------------------------------------*/
        sibling = get_sibling(node);
        parent = node->parent;

        if (is_red(parent) &&
            is_black(sibling) &&
            is_black(sibling->left) && 
            is_black(sibling->right))
        {
            set_red(sibling);
            set_black(parent);
            break;
        }
        /* END DELETE_CASE_4 */


        /*-----------------------------------------------------------------------------------
        * DELETE_CASE_5 - Sibling is a black left child and has a red left child and a black 
        *      right child. The mirror case is also applicable. In these situations the tree is 
        *      rotated such sibling becomes the child of its red child and then its color is
        *      swapped with the red child (new parent). If this case is not met, this function 
        *      casecades into case 6.
        *     
        *            BS        -->   BSL    
        *           / \               \
        *          RSL BSR            RS
        *                              \
        *                              BSL
        *-----------------------------------------------------------------------------------*/
        sibling = get_sibling(node);
    
        if (is_black(sibling))
        {
            bool is_left = is_left_child(node);
            if (is_left && is_black(sibling->right) && is_red(sibling->left))
            {
                set_red(sibling);
                set_black(sibling->left);
                rotate_right(tree, sibling);
            }
            else if (!is_left && is_black(sibling->left) && is_red(sibling->right))
            {
                rotate_left(tree, sibling);
            }
        }
        /* END DELETE_CASE_5 */

        /*-----------------------------------------------------------------------------------
        * DELETE_CASE_6 - Sibling is black. Node is a left child of parent and sibling has a 
        *      right red child or node is a right child and sibling has a left red child. 
        *      In these cases parent and sibling swap colors and the red child of sibling is
        *      recolored black. The tree is then rotated such that sibling becomes nodes
        *      grandparent.
        *     
        *            P       -->     S    
        *           / \             / \
        *          BN  BS          BP  BSR
        *               \          /    
        *                RSR      BN
        *
        *----------------------------------------------------------------------------------*/
        sibling =  get_sibling(node);
        parent = node->parent;
        sibling->color = parent->color;
        set_black(parent);

        if (is_left_child(node))
        {
            set_black(sibling->right);
            rotate_left(tree, parent);
        }
        else
        {
            set_black(sibling->left);
            rotate_right(tree, parent);
        }
        break;
        /* END DELETE_CASE_6 */
    }
}

/*--------------------------------------------------------------------------------------
 * delete_one_child - Deletes a node from a rb_tree containing at most one child.
 *
 * tree: A ptr to the rb_tree_t from which to delete a node. [OUTPUT] 
 * node: A ptr to the rb_node_t to delete. [OUTPUT]
 *
 *--------------------------------------------------------------------------------------*
 * CITATION: The code in this function was adapted from the following source.
 *
 * Wikipedia contributors. (2019, June 16). Red–black tree. In Wikipedia, The Free Encyclopedia.
 * Retrieved 14:34, June 20, 2019, 
 * from https://en.wikipedia.org/w/index.php?title=Red%E2%80%93black_tree&oldid=902059008
 *--------------------------------------------------------------------------------------*/ 
static void delete_one_child(rb_tree_t* tree, rb_node_t* node)
{
    rb_node_t* child = has_left_child(node) ? node->left : node->right;

    if (child == NULL)
    {
        /* The current node is a leaf with no children. */

        if (is_black(node))
        {
            /* If the node is a black leaf with no children then deleting it
               requires rebalancing. */
            delete_rebalance(tree, node); 
        }
       
        /* Replace the current node with its NULl child. In the case where the current
           node was red this step results in a trivial deletion. */
        replace_node(node, child);
    }
    else {
        
        /* Replace the current node with its non-null child and rebalance. */
        replace_node(node, child);

        if (is_black(node))
        {
            /* Rebalancing is only required upon deleting a black node 
               since a red node results in a trivial swap and deletion. */

            if (is_red(child))
            {
                /* If node is a black and child is red then simply recolor
                   child to black to preserve the black depth of the tree. */
                set_black(child);
            }
            else
            {
                /* Node and its child are black. The tree must be rebalanced to 
                   account for the change in black depth. */
                delete_rebalance(tree, node);
            }
        }
    }
    
    /* Free the block of memory for the deleted node. */
    push_free_node(tree, node); 
}

/*--------------------------------------------------------------------------------------
 * delete_rb_node - Deletes a rb_node_t from a rb_tree_t and rebalances the tree accordingly. 
 *
 * tree: A ptr to the rb_tree_t from which to delete an rb_node. [OUTPUT] 
 * node: A ptr to the rb_node_t to delete from the tree. [OUTPUT]
 *--------------------------------------------------------------------------------------*/ 
static void delete_rb_node(rb_tree_t* tree, rb_node_t* node)
{
    /* Attempt to find a successor node for the current node. */
    rb_node_t* successor_node = get_successor(node);

    if (successor_node != NULL)
    {
        /* There exists a leaf node within the subtrees of node such that
         * we can replace it with the node we want to delet and simply focus on 
         * the case of deleting the leaf node.
         */
        swap_values(node, successor_node);
        swap_offsets(node, successor_node);
        node = successor_node;
        /* Node now points to a node with at most one child that we want to delete. */
        delete_one_child(tree, node);
    }
    else 
    {
        /* Node has no successor. */
        if(is_root(node))
        {
            /* The current node is root and has no successors so deleting it is trivial. */
            push_free_node(tree, node);
            tree->root = NULL;
        }
        else
        {
            /* The current node is a leaf with no children. */
            delete_one_child(tree, node);
        }
    }
}

/*--------------------------------------------------------------------------------------
 * are_consecutive - Checks if value_2 is the consecutive integer after value_1.
 *
 * value_1: The lesser of the two potentially consecutive values. [INPUT]
 * value_2: The greater of the two potentially consecutive values. [INPUT]
 * returns: Whether value_2 is the consecutive integer after value_1.
 *-------------------------------------------------------------------------------------*/
static bool are_consecutive(uint32_t value_1, uint32_t value_2)
{
    return (value_1 != UINT32_MAX) && (value_1 + 1 == value_2);
}

/*--------------------------------------------------------------------------------------
 * try_binary_insert_or_merge - Attempts to insert a new value into a red black tree. If
 *      the value is a duplicate it is ignored. If the value is a consecutive number with
 *      any existing node values then those nodes are merged and their offsets are incremented
 *      to reflect the range of values the modified nodes will represent.
 *
 * value: The value of the node to attempt to insert or merge into the red black tree. [INPUT]
 * tree: A ptr to the rb_tree_t to insert values into. [OUTPUT]
 * inserted_node: A ptr to a ptr to the rb_node that was inserted. NULL if no node was 
 *      inserted. [OUTPUT]
 * returns: A rb_tree status indicating the result of the insertion attempt.
 *-------------------------------------------------------------------------------------*/
static rb_tree_status_t try_binary_insert_or_merge(uint32_t value, rb_tree_t* tree, rb_node_t** inserted_node)
{
    rb_tree_status_t status;
    *inserted_node = NULL;
    rb_node_t* node = tree->root;
    if (node == NULL) 
    {
        /* The tree is empty and so a root node is created. */
        tree->root = create_rb_node(value,  BLACK, tree);
        status = RB_SUCCESS;
        *inserted_node = tree->root;
    }
    else 
    {
        /* Root is not NULL and so a binary insertion is attempted. */
        while (true)
        {  
            if (are_consecutive(value, node->range.value)) 
            {
                /* The current node value is greater than value and consecutive and so we should
                   merge the new value into the tree. */
                rb_node_t* successor = get_left_successor(node);
                if (successor != NULL &&
                    are_consecutive(successor->range.value + successor->range.offset, value))
                {
                    /* The left child of the current node is also consecutive with the new
                       value and so we can merge the three values into a single node. */
                    node->range.value = successor->range.value;
                    node->range.offset += successor->range.offset + 2;
                    delete_rb_node(tree, successor);
                    status = RB_SUCCESS;
                    break; 
                }
                else {
                    /* Merge the new value into the current node. */
                    node->range.value = value;
                    node->range.offset += 1;
                    status = RB_SUCCESS;
                    break;
                }
            }
            else if (value < node->range.value)
            {
                /* Value is less than the current node's value and is not consecutive 
                   so we either search a subtree of the child or insert a new node. */
                if (has_left_child(node))
                {
                    /* Node already has a left child and so we search the left subtree and repeat
                       the while loop. */
                    node = node->left;
                }
                else
                {
                    /* Memory was available for new node and it is added. */
                    *inserted_node = create_rb_node(value, RED, tree);
                    if (*inserted_node == NULL)
                    {
                        /* There was no memory remaining for inserting a new child. */
                        status = RB_FAIL_TREE_FULL;
                        break;
                    }
                    insert_child(*inserted_node, node, &node->left);
                    status = RB_SUCCESS;
                    break;
                }
               
            }
            else if (are_consecutive(node->range.value + node->range.offset, value))
            {
                /* The current node value is lesser than value and consecutive and so we should
                   merge the new value into the tree. */
                rb_node_t* successor = get_right_successor(node);
                if (successor != NULL && are_consecutive(value, successor->range.value))
                {
                    /* The right child of the current node is also consecutive the the new value
                       and so we can merge the three values into a single node. */
                    node->range.offset += successor->range.offset + 2;
                    delete_rb_node(tree, successor);
                    status = RB_SUCCESS;
                    break;
                }
                else
                {
                    node->range.offset += 1;
                    status = RB_SUCCESS;
                    break;
                }             
            }
            else if (value > node->range.value)
            {    
                /* Value is greater than the current node's value and is not consecutive and so we
                   either search a subtree of the child or insert a new node. */
                if (has_right_child(node))
                {
                    /* Node already has a left child and so we search the right subtree and 
                       repeat the while loop. */
                    node = node->right;
                }
                else
                {
                    /* Memory was available for new node and it is added. */
                    *inserted_node = create_rb_node(value, RED, tree);
                    if (*inserted_node == NULL)
                    {
                        /* There was no memory remaining for inserting a new child. */
                        status = RB_FAIL_TREE_FULL;
                        break;
                    }
                    insert_child(*inserted_node, node, &node->right);
                    status = RB_SUCCESS;
                    break;
                }
            }
            else
            {
                /* Value already exists within the tree. Do not insert any duplicates. */
                status = RB_FAIL_INSERT_DUPLICATE;
                break;
            }
        }
    }
    return status;
}

/*--------------------------------------------------------------------------------------
 * try_insert_rebalance - Ensure that the red black tree adheres to all rules and rebalances
 *      the nodes within it accordingly. 
 *
 * root: A ptr to the rb_tree_t to rebalance. [OUTPUT]
 * node: A ptr to the last modified node in the red black tree. [OUTPUT]
 *
 *--------------------------------------------------------------------------------------*
 * CITATION: The code in this function was adapted from the following source.
 *
 * Wikipedia contributors. (2019, June 16). Red–black tree. In Wikipedia, The Free Encyclopedia.
 * Retrieved 14:34, June 20, 2019, 
 * from https://en.wikipedia.org/w/index.php?title=Red%E2%80%93black_tree&oldid=902059008
 *--------------------------------------------------------------------------------------*/ 
static void try_insert_rebalance(rb_tree_t* tree, rb_node_t* node)
{
    while(true)
    {
        rb_node_t* parent = node->parent;
        rb_node_t* uncle = get_uncle(node);   
 
        if (parent == NULL)
        {
            /* Case Root:
               The inserted node is root. Set its color to black. */
            set_black(node);
            break;
        }
        else if (is_black(parent))
        {
            /* Case Black Parent:
               For insertion, this means we have not violated any rules. */
            break;
        }
        else if (uncle != NULL && is_red(uncle))
        {
            /* Case Red Parent and Red Uncle:
               Both parent and uncle should be made black. */
            set_black(parent);
            set_black(uncle);
            rb_node_t* grandparent = get_grandparent(node);
            set_red(grandparent);

            /* Repeat the rebalancing steps to correct for red root case or red parent. */
            node = grandparent;
        }
        else
        {     
            /* Case Red Parent and Black Uncle:   
               The goal of the below steps is to rotate the current node into the grandparent
               position. */
            rb_node_t* grandparent = get_grandparent(node);

            if (parent == grandparent->left && node == parent->right)
            {
                /* If parent is a left child and node is a right child then rotate left
                   and reassign node to perform an additional rebalancing loop. */
                rotate_left(tree, parent);
                node = node->left; 
            }
            else if (parent == grandparent->right && node == parent->left)
            {
                /* If parent is a right child and node is a left child then rotate right
                   and reassign node to perform an additional rebalancing loop. */
                rotate_right(tree, parent);
                node = node->right;
            }
            
            grandparent = get_grandparent(node);
            parent = node->parent;

            if (is_left_child(node))
            {
                rotate_right(tree, grandparent);
            }
            else
            {
                rotate_left(tree, grandparent);
            }
            set_black(parent);
            set_red(grandparent);
            break;
        }
    }
}

/*--------------------------------------------------------------------------------------
 * delete_rb_node_without_rebalancing - Deletes a node from the rb_tree without rebalancing.
 *      This does not result in any memory deallocation, but the node is now assigned to
 *      to a "free block" within the tree and can be reused. This function should only be
 *      called when traversing a rb_tree inorder and writing the nodes to a dacs. Calling
 *      this function also assumes that node has at most one child node that is a right
 *      right child.
 * 
 * tree: A ptr to an rb_tree from which to remove the provided node. [OUTPUT]
 * node: A ptr to an rb_node to delete and remove references to it in its parent. [OUTPUT]
 *--------------------------------------------------------------------------------------*/  
static void delete_rb_node_without_rebalancing(rb_tree_t* tree, rb_node_t* node)
{
    if (!is_root(node))
    {
        // If node is not root we need to remove its parents references to it.
        if (is_left_child(node))
        {
            // If node is a left child of parent set parent's left child to NULL.
            node->parent->left = node->right;
        }
        else
        {
            // If node is a right child of parent set parent's right child to NULL.
            node->parent->right = node->right;
        }
    }
    else
    {
        tree->root = node->right;
    }

    if (node->right != NULL)
    {
        // If the right sub node exists we also update its references to parent.
        node->right->parent = node->parent;
    }
    push_free_node(tree, node);
}

/*--------------------------------------------------------------------------------------
 * rb_tree_binary_search - Searches a rb_tree for a node containing a given value.
 *
 * tree: A ptr to a rb_tree_t to search. [INPUT]
 * value: The value to search for within the rb_tree_t. [INPUT]
 * returns: A ptr to a rb_node_t to populate with the identified node. This is set to NULL
 *      if no node is found.
 *--------------------------------------------------------------------------------------*/ 
static rb_node_t* rb_tree_binary_search(rb_tree_t* tree, uint32_t value)
{
    rb_node_t* node = tree->root;
    while(node != NULL)
    {
        if ((node->range.value <= value) && 
            ((node->range.value + node->range.offset) >= value))
        {
            /* Node contains the current value. */
            break;
        }
        else if (node->range.value > value)
        {
            node = node->left;
        }
        else
        {
            node = node->right;
        }
    }
    return node;
}

/******************************************************************************
 EXPORTED FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
 * rb_tree_create -
 *
 * max_size: The maximum number of allowable nodes within the red black tree.
 * tree: A rb_tree_t to allocate memory to.
 *--------------------------------------------------------------------------------------*/ 
rb_tree_status_t rb_tree_create(uint32_t max_size, rb_tree_t* tree) 
{
    if (tree == NULL)
    {
        return RB_FAIL_NULL_TREE;
    }
    
    tree->size = 0;
    tree->max_size = 0;
    tree->root = NULL;
    tree->free_node_head = NULL;
    tree->free_node_tail = NULL;
    tree->node_block = NULL;
    

    if (max_size == 0)
    {
        return RB_FAIL_SIZE_ZERO;
    }
    if (max_size > MAX_TREE_SIZE)
    {
        return RB_FAIL_EXCEEDED_MAX_SIZE;
    }

    /* Size starts maxed out until free blocks are allocated. */
    tree->size = max_size;
    tree->max_size = max_size;
    
    /* Allocate a block of memory for the nodes in the tree and add them all to the
       the free nodes queue. */
    tree->node_block = (rb_node_t*) calloc(max_size, sizeof(rb_node_t));

    if (tree->node_block == NULL)
    {
        /* If no memory is allocated return an empty tree. */
        return RB_FAIL_MEM_ERR;
    }

    rb_node_t* start = tree->node_block;
    rb_node_t* end = start + max_size;
    
    for (; start < end; start++)
    {
        push_free_node(tree, start);
    }
    return RB_SUCCESS;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_clear - Clears all the nodes in an rb_tree_t. No memory is deallocated. Nodes
 *      are simply appended to the free stack.
 *
 * tree - A ptr to an rb_tree_t to clear all of its nodes. [OUTPUT]
 * returns: A status indicating the sucess of the clear operation.
 *--------------------------------------------------------------------------------------*/
rb_tree_status_t rb_tree_clear(rb_tree_t* tree)
{
    if (tree == NULL)
    {
        return RB_FAIL_NULL_TREE;
    }

    if (rb_tree_is_empty(tree))
    {
        return RB_SUCCESS;
    }
    
    rb_node_t* node = tree->root;
    while(node != NULL)
    {
        if (has_left_child(node))
        {
            node = node->left;
        }

        else if (has_right_child(node))
        {
            node = node->right;
        }

        remove_from_parent(node);
        push_free_node(tree, node);
        node = node->parent;
    }

    return RB_SUCCESS;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_is_empty - 
 *
 * tree: The rb_tree_t to check whether is empty.
 * returns: True when the rb_tree_t has no nodes, otherwise false. If tree is NULL returns
 *      false.
 *--------------------------------------------------------------------------------------*/ 
bool rb_tree_is_empty(rb_tree_t *tree)
{
    return tree != NULL && tree->size == 0;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_is_full - 
 *
 * tree: The rb_tree to check whether is full.
 * returns: True when the rb_tree is full, otherwise false. If tree is NULL returns true.
 *--------------------------------------------------------------------------------------*/ 
bool rb_tree_is_full(rb_tree_t *tree)
{
    return tree == NULL || tree->size == tree->max_size;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_insert - Inserts a value into the red black tree and rebalances it accordingly.
 *
 * value - The value to insert into the rb_tree_t. [INPUT]
 * tree: A ptr to a rb_tree_t to insert the value into. [OUTPUT]
 * returns: An rb_tree_status_t enum indicating the result of the insertion. 
 *--------------------------------------------------------------------------------------*/ 
rb_tree_status_t rb_tree_insert(uint32_t value, rb_tree_t* tree)
{
    if (tree == NULL)
    {
        return RB_FAIL_NULL_TREE;
    }   
    
    if (tree->node_block == NULL || tree->max_size == 0)
    {
        return RB_FAIL_SIZE_ZERO;
    }

    rb_node_t* inserted_node = NULL;
    rb_tree_status_t status = try_binary_insert_or_merge(value, tree, &inserted_node);

    if (status == RB_SUCCESS && inserted_node != NULL)
    {
        /* Correct any violations within the red black tree due to the insertion. */
        try_insert_rebalance(tree, inserted_node);
    }
    return status;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_delete - Deletes a value into the red black tree and rebalances it accordingly.
 *
 * value - The value to delete into the rb_tree_t. [INPUT]
 * tree: A ptr to a rb_tree_t to delete value from. [OUTPUT]
 * returns: An rb_tree_status_t enum indicating the result of the deletion. 
 *--------------------------------------------------------------------------------------*/ 
rb_tree_status_t rb_tree_delete(uint32_t value, rb_tree_t* tree)
{
    if (tree == NULL)
    {
        return RB_FAIL_NULL_TREE;
    }

    rb_node_t* node = rb_tree_binary_search(tree, value);
    rb_tree_status_t status = RB_SUCCESS;
    if (node == NULL)
    {
        /* No node containing value was found. */
        status = RB_FAIL_VALUE_NOT_FOUND;
    }
    else
    {
        if(node->range.offset == 0)
        {
            /* Node contains a single value so it can be deleted. */
            delete_rb_node(tree, node);
        }
        else
        {
            /* Node contains a range and so it must be split. */
            if (value == node->range.value && node->range.offset != 0)
            {
                /* Value is at the start of nodes range so we can redefine the range. */
                node->range.value += 1;
                node->range.offset -= 1;
            }
            else if (value == node->range.value + node->range.offset)
            {
                /* Value at the end of the nodes range so it can be redefind. */
                node->range.offset -= 1;
            }
            else
            {
                /* Value is somewhere within the range of the current node and so that
                   node must be split. */
                rb_node_t* upper_node = NULL;
                rb_tree_status_t insert_status = try_binary_insert_or_merge(value + 1, 
                                                                          tree, 
                                                                          &upper_node);
                
                if (insert_status != RB_SUCCESS)
                {
                    /* Failure in inserting the new upper range node into the tree. In theory
                       this should only ever be caused by a lack of memory in the tree. */
                    status = insert_status;
                    assert(status == RB_FAIL_TREE_FULL);
                }
                else
                {
                    /* Memory was sucessfully allocated to the new node. */
                    assert(upper_node != NULL);
                    upper_node->range.offset = node->range.value + node->range.offset - upper_node->range.value;
                    node->range.offset = value - node->range.value - 1;
                }
            }
        }
    }
    return status;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_destroy - Frees all memory allocated by a given rb_tree_t. 
 *
 * tree: A ptr to a rb_tree_t to free its memory. [OUTPUT]
 *--------------------------------------------------------------------------------------*/ 
rb_tree_status_t rb_tree_destroy(rb_tree_t* tree)
{
    if (tree == NULL) return RB_FAIL_NULL_TREE;
    free(tree->node_block);
    tree->node_block = NULL;
    return RB_SUCCESS;
}

/*--------------------------------------------------------------------------------------
 * rb_tree_get_first_rb_node - Traverses a rb_tree_t inorder and finds the node
 *      with the lowest value to serve as an iterator.

 * tree: A ptr to a rb_tree_t to identify the lowest range. [OUTPUT]
 * iter: A ptr of a ptr to the next rb_node_t node from which to continue the inorder 
 *      traversal of the tree. After each call to this function iter is updated. If iter is 
 *      set to NULL then the traversal is complete. Providing a NULL iter to this function 
 *      will result in a NULL rb_range_t ptr. [OUTPUT]
 * returns: A status indicating the outcome of the function call.
 *--------------------------------------------------------------------------------------*/ 
rb_tree_status_t rb_tree_get_first_rb_node(rb_tree_t* tree,
                                                   rb_node_t** iter)
{
    if (tree == NULL)
    {
        /* No nodes exist in the tree. */
        return RB_FAIL_NULL_TREE;
    }

    *iter = tree->root;
    if (*iter == NULL)
    {
        /* There exist no nodes within the tree. */
        return RB_FAIL_NULL_TREE;
    }

    /* Resets the is visited state of root. */
    (*iter)->traversal_state = false;

    while (has_left_child(*iter))
    {
        /* Update iter to the left most child of root and reset is visited. */
        *iter = (*iter)->left;
        (*iter)->traversal_state = false;
    }
    return RB_SUCCESS;
}

 /*--------------------------------------------------------------------------------------
 * rb_tree_get_next_rb_node - Traverses a rb_tree_t inorder and returns the
 *      the ranges within the tree. This function performs no rebalancing and therefore if
 *      should_pop is true and should_rebalance is false, it should be called until no 
 *      nodes remain in the tree to ensure that the tree's operation does not degrade.
 *
 * tree: A ptr to a rb_tree_t to identify the lowest range. [OUTPUT]
 * iter: A ptr of a ptr to the next rb_node_t node from which to continue the inorder 
 *      traversal of the tree. After each call to this function iter is updated. If iter is 
 *      set to NULL then the traversal is complete. Providing a NULL iter to this function 
 *      will result in a NULL rb_range_t ptr. [OUTPUT]
 * range: A ptr to a bp_range_t to update with the next range. [OUTPUT]
 * should_pop: A boolean determining whether or not the node pointed to by iter at the start of
 *      of the function call should be deleted from the tree. [INPUT]
 * should_rebalance: A boolean determining whether or not the tree should be balanced upon
 *      deletions. This variable is only used when should_pop is true. If should_rebalance is
 *      set to false then this function should be called to for all iterations until completion
 *      or else the tree is no longer garunteed to operate properly.
 * returns: A status indicating the outcome of the function call.
 *--------------------------------------------------------------------------------------*/ 
rb_tree_status_t rb_tree_get_next_rb_node(rb_tree_t* tree,
                                                   rb_node_t** iter, 
                                                   rb_range_t* range,
                                                   bool should_pop,
                                                   bool should_rebalance)
{
    if (*iter == NULL)
    {
        /* There is no next if the provided node is NULL. */
        return RB_FAIL_NULL_NODE;
    }
    if (range == NULL)
    {
        /* The range to fill is NULL and so the function must exit early. */
        return RB_FAIL_NULL_RANGE;
    }

    /* Fill the range from the current iter ptr. */
    range->value = (*iter)->range.value;
    range->offset = (*iter)->range.offset;
    
    /* Set iter to visited and store node for deletion. */
    rb_node_t* delete_node = *iter;
    rb_node_t* node = *iter;
   
    if (should_pop && should_rebalance)
    {
        /* Remove the node and rebalance the tree. Iter must be recalculated. */
        delete_rb_node(tree, delete_node);
        rb_tree_get_first_rb_node(tree, iter);
        return RB_SUCCESS;
    }

    /* Node hasn't been traversed. This means we are at a leaf. */
    if (has_right_child(node))
    {
        /* Node has a right child and so the right subtree must be explored. */
        node->traversal_state = true;
        node = node->right;
        node->traversal_state = false;
        while (has_left_child(node))
        {
            node = node->left;
            node->traversal_state = false;
        }
        *iter = node;
    }
    else
    {
        /* Node has no children and so the tree is searched upward for an unvisted node. */
        node->traversal_state = true;
        while (node != NULL && node->traversal_state)
        {
            node = node->parent;
        }
        *iter = node;
    }
    
    if (should_pop && !should_rebalance)
    {
        /* Remove the the node from the tree. NO REBALANCING WILL OCCUR. */
        delete_rb_node_without_rebalancing(tree, delete_node);
    }
    return RB_SUCCESS;
}

#ifdef RBTREETESTS
/*--------------------------------------------------------------------------------------
 * DEFINES TEST AND HELPER FUNCTIONS FOR THE RED BLACK TREE
 * 
 * RUN TESTS COMMAND LINE RECIPE:
 *
 * sudo make APP_COPT=-DRBTREETESTS unittest && ./bp.out
 *--------------------------------------------------------------------------------------*/

/******************************************************************************
 TEST AND DEBUGGING HELPER FUNCTIONS 
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
 * print_node -
 * 
 * node: A ptr to an rb_node to print. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void print_node(rb_node_t* node)
{
    if (node == NULL)
    {
        printf("NULL NODE\n");
        return;
    }
    printf("[ C: %5s || N: %3d || P: %3d || L: %3d || R: %3d || O: %3u]\n",
        node->color ? "RED" : "BLACK",
        node != NULL ? (int) node->range.value : -1,
        node->parent != NULL ? (int) node->parent->range.value : -1,
        node->left != NULL ? (int) node->left->range.value : -1,
        node->right != NULL ? (int) node->right->range.value : -1,
        node->range.offset);
}

/*--------------------------------------------------------------------------------------
 * apply_inorder - Applies a function in order to nodes in a rb_tree. 
 * 
 * node: A ptr to a rb_node to apply a function to and recurse its subtrees. [INPUT]
 * func. A function pointer accepting a ptr to an rb_node. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void apply_inorder(rb_node_t* node, void (*func)(rb_node_t*))
{
    if (has_left_child(node))
    {
        /* Apply function to left subtree. */
        apply_inorder(node->left, func);
    }

    /* Apply function to the current node. */
    func(node);

    if (has_right_child(node))
    {
        /* Apply function to right subtree. */
        apply_inorder(node->right, func);
    }
}

/*--------------------------------------------------------------------------------------
 * print_tree - 
 * 
 * tree: A ptr to an rb_tree to print. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void print_tree(rb_tree_t* tree)
{
    printf("\n##################################\n");
    printf("* Size: %u / %u\n", tree->size, tree->max_size);
    printf("**********************************\n");
   
    if (tree->size == 0) {
        return;
    }
    printf("* In Order Elements:               \n");
    printf("**********************************\n"); 
    apply_inorder(tree->root, print_node);
    printf("**********************************\n");
    fflush(stdout);
}

/*--------------------------------------------------------------------------------------
 * assert_inorder_values_are - Asserts that the nodes in tree match the provided values.
 *
 * node: A ptr to a rb_node_t to recursively check if its values match the values array.
 * nodes: An array of ptr to rb_node_ts whose values, offsets and colors will be 
 *      compared to the nodes in the tree tree inorder.
 * length: The length of the nodes array.
 * index: A ptr to an integer index into the nodes array which is incremented within recursive
 *      calls to this function.
 * returns: The number of times the current index was incremented by its subtrees
 *--------------------------------------------------------------------------------------*/  
static uint32_t assert_inorder_nodes_are(rb_node_t* node, 
                                    rb_node_t** nodes, 
                                    uint32_t length, 
                                    uint32_t index)
{
    uint32_t index_offset = 0;

    if (node == NULL)
    {
        /* In the case where root is empty we should have no nodes.  */
        assert(length == 0);
        return 0;
    }

    if (has_left_child(node))
    {
        /* Check left subtree values. */
        index_offset += assert_inorder_nodes_are(node->left, nodes, length, index);
    }

    /* Check that there are not more nodes than expected. */
    assert(index < length);
    
    /* Check that the nodes matches those provided in the list. */
    assert(node->range.value == nodes[index + index_offset]->range.value);
    assert(node->range.offset == nodes[index + index_offset]->range.offset);   
    assert(node->color == nodes[index + index_offset]->color); 

    index_offset += 1;

    if (has_right_child(node))
    {
        /* Check right subtre values. */
        index_offset += assert_inorder_nodes_are(node->right, nodes, length, 
                                                 index + index_offset);
    }

    return index_offset;
}

/*--------------------------------------------------------------------------------------
 * assert_node_has_no_adjacent_red - Checks the condition that a given node has no adjacent
 *      red nodes among its children.
 * 
 * node: A ptr to check rb_node_t to check it and its subtrees for validity. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void assert_node_has_no_adjacent_red(rb_node_t* node)
{
    if (is_black(node))
    {
        /* Return early since the node is black and cannot have an adajcent red. */
        return;
    }
    if (has_left_child(node))
    {
        /* Assert left child is black since current node is red. */
        assert(is_black(node->left));
        assert_node_has_no_adjacent_red(node->left);
    }
    if (has_right_child(node))
    {
        /* Assert right child is black since current node is red. */
        assert(is_black(node->right));
        assert_node_has_no_adjacent_red(node->right);
    }
}

/*--------------------------------------------------------------------------------------
 * count_is_black - 
 * 
 * node: A ptr to an rb_node_t to check its color. [INPUT]
 * returns: 1 if the provided node is black or null, else 0.
 *--------------------------------------------------------------------------------------*/  
static uint32_t count_is_black(rb_node_t* node)
{
    return is_black(node) ? 1 : 0;
}

/*--------------------------------------------------------------------------------------
 * assert_tree_pathes_have_equal_black_depths - Asserts the validity condition that
 *      all pathes from a given node in a red black tree have equal black depths.
 * 
 * node: A ptr to a node to check its subtrees for equal black depths. [INPUT]
 * returns: The black depth in indicating the number of black nodes along a path to a 
 * leaf starting at node.
 *--------------------------------------------------------------------------------------*/  
static uint32_t assert_tree_pathes_have_equal_black_depths(rb_node_t* node)
{
    if (node == NULL)
    {   
        /* If the node is NULL leaf or NULL root return early. */
        return 0;
    }

    uint32_t left_count = 0;
    uint32_t right_count = 0;
    if (has_left_child(node))
    {
        /* Node has a left child. Check that the black depths are
           equal down the child subtree. */
        left_count = assert_tree_pathes_have_equal_black_depths(node->left);
    }
    else {
        /* If node has no left child the left tree has a black
           depth of 1 since leaves are black. */
        left_count += 1;
    }
    if (has_right_child(node))
    {
        /* Node has a left child. Check that the black depths are equal
           down the child subtree. */
        right_count = assert_tree_pathes_have_equal_black_depths(node->right);
    }
    else
    {
        /* Node has no right child and so the right subtree black depth
           count is 1 since leaves are black. */
        right_count += 1;
    }

    assert (left_count == right_count);
    /* Need to only test one path since they are equal. */
    return count_is_black(node) + left_count; 
}

/*--------------------------------------------------------------------------------------
 * assert_node_values_in_between_child - Asserts the red black tree validity condition that
 *      a given nodes left child has a value less than node and the right child has a value
 *      greater than node.
 * 
 * node: A ptr to an rb_node_t to compare its value to its children and also check the children's subtrees. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void assert_node_value_in_between_children(rb_node_t* node)
{
    if (node == NULL)
    {
        return;
    }

    if (has_left_child(node))
    {
        assert((node->range.value) > (node->left->range.value + node->left->range.offset));
        assert_node_value_in_between_children(node->left);
    }
    if (has_right_child(node))
    {
        assert((node->range.value + node->range.offset) < node->right->range.value);
        assert_node_value_in_between_children(node->right);
    }
}

/*--------------------------------------------------------------------------------------
 * assert_rb_tree_is_valid - Checks all the validity conditions for a valid red black tree.
 * 
 * tree: A ptr to an rb_tree_t to check for validity. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void assert_rb_tree_is_valid(rb_tree_t* tree)
{
    assert(is_black(tree->root));
    assert_node_has_no_adjacent_red(tree->root);
    assert_tree_pathes_have_equal_black_depths(tree->root);
    assert_node_value_in_between_children(tree->root);
}

/*--------------------------------------------------------------------------------------
 * shuffle - Shuffles an array of integers randomly.
 * 
 * array: A ptr to the array to shuffle. [OUTPUT]
 * length: The length of the uint32_t array to shuffle. [INPUT]
 *--------------------------------------------------------------------------------------*/  
static void shuffle(uint32_t* array, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        uint32_t j = i + rand() / (RAND_MAX / (length - i) + 1);
        uint32_t temp = array[j];
        array[j] = array[i];
        array[i] = temp;
    }
}

/******************************************************************************
 RB_TREE TESTS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
 * test_new_tree_empty - 
 *--------------------------------------------------------------------------------------*/  
static void test_new_tree_empty()
{
    printf("test_new_empty_tree: ");
    rb_tree_t tree;
    assert(rb_tree_create(0, &tree) == RB_FAIL_SIZE_ZERO);
    assert(tree.root == NULL);
    assert(tree.max_size == 0);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_unable_to_insert_into_empty_tree - 
 *--------------------------------------------------------------------------------------*/  
static void test_unable_to_insert_into_empty_tree()
{
    printf("test_unable_to_insert_into_empty_tree: ");
    rb_tree_t tree;
    assert(rb_tree_create(0, &tree) == RB_FAIL_SIZE_ZERO);
    assert(rb_tree_is_full(&tree));
    assert(tree.root == NULL); 
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_unable_to_insert_into_full_tree - 
 *--------------------------------------------------------------------------------------*/  
static void test_unable_to_insert_into_full_tree()
{
    printf("test_unable_to_insert_into_full_tree: ");
    rb_tree_t tree;
    rb_tree_create(4, &tree);
    assert(tree.size == 0);
    assert(!rb_tree_is_full(&tree));
    rb_tree_insert(0, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(2, &tree);
    assert_rb_tree_is_valid(&tree);       
    rb_tree_insert(4, &tree);
    assert_rb_tree_is_valid(&tree);   
    rb_tree_insert(6, &tree);
    assert_rb_tree_is_valid(&tree);
    assert(tree.size == 4);
    assert(rb_tree_is_full(&tree));
    /* The final insert should fail when tree is full. */
    assert(rb_tree_insert(8, &tree) == RB_FAIL_TREE_FULL);
    assert_rb_tree_is_valid(&tree);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}


/*--------------------------------------------------------------------------------------
 * test_deletes_tree -
 *--------------------------------------------------------------------------------------*/  
static void test_deletes_tree()
{
    printf("test_deletes_tree: ");
    /* Tests deleting a tree with nodes. */
    rb_tree_t tree;
    rb_tree_create(5, &tree);
    rb_tree_insert(0, &tree);
    rb_tree_insert(1, &tree);
    rb_tree_insert(2, &tree);
    rb_tree_insert(3, &tree);
    rb_tree_destroy(&tree);
    
    /* Tests deleting an empty tree. */
    rb_tree_create(0, &tree);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_single_insertion -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_root()
{
    printf("test_insert_root: ");
    rb_tree_t tree;
    rb_tree_create(1, &tree);
    rb_tree_insert(5, &tree);
    assert_rb_tree_is_valid(&tree);
 
    rb_node_t n1 = {.range = {.value = 5, .offset = 0}, .color=BLACK};
    rb_node_t* nodes[] = {&n1};
    assert_inorder_nodes_are(tree.root, nodes, 1, 0);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_insert_left_subtree -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_left_subtree()
{
    printf("test_insert_left_subtree: ");
    /* Tests a single insertion into the left subtree from root. */
    rb_tree_t tree;
    rb_tree_create(4, &tree);
    rb_tree_insert(7, &tree);
    rb_tree_insert(5, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_node_t n1 = {.range = {.value = 7, .offset = 0}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 5, .offset = 0}, .color = RED};
    rb_node_t* nodes_1[] = {&n2, &n1};
    assert_inorder_nodes_are(tree.root, nodes_1, 2, 0);
    
    /* Tests inserting into a left subtree requiring a rebalancing. */
    rb_tree_insert(3, &tree);
    n1.color = RED;
    n2.color = BLACK;
    rb_node_t n3 = {.range = {.value = 3, .offset = 0}, .color = RED};
    rb_node_t* nodes_2[] = {&n3, &n2, &n1}; 
    assert_inorder_nodes_are(tree.root, nodes_2, 3, 0);

    /* Tests an insertion into the left subtree when the parent node is not root. */
    rb_tree_insert(1, &tree); 
    n1.color = BLACK;
    n2.color = BLACK;
    n3.color = BLACK;
    rb_node_t n4 = {.range = {.value = 1, .offset = 0}, .color = RED};
    rb_node_t* nodes_3[] = {&n4, &n3, &n2, &n1};
    assert_inorder_nodes_are(tree.root, nodes_3, 4, 0);
    rb_tree_destroy(&tree);
    printf("PASS\n");
 }

/*--------------------------------------------------------------------------------------
 * test_insert_right_subtree -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_right_subtree()
{
    printf("test_insert_right_subtree: ");
    /* Tests a single insertion into the right subtree from root. */
    rb_tree_t tree;
    rb_tree_create(4, &tree);
    rb_tree_insert(1, &tree);
    rb_tree_insert(3, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_node_t n1 = {.range = {.value = 1, .offset = 0}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 3, .offset = 0}, .color = RED};
    rb_node_t* nodes_1[] = {&n1, &n2};
    assert_inorder_nodes_are(tree.root, nodes_1, 2, 0);
    
    /* Tests inserting into a right subtree requiring a rebalancing. */
    rb_tree_insert(5, &tree);
    n1.color = RED;
    n2.color = BLACK;
    rb_node_t n3 = {.range = {.value = 5, .offset = 0}, .color = RED};
    rb_node_t* nodes_2[] = {&n1, &n2, &n3}; 
    assert_inorder_nodes_are(tree.root, nodes_2, 3, 0);

    /* Tests an insertion into the right subtree when the parent node is not root. */
    rb_tree_insert(7, &tree); 
    n1.color = BLACK;
    n2.color = BLACK;
    n3.color = BLACK;
    rb_node_t n4 = {.range = {.value = 7, .offset = 0}, .color = RED};
    rb_node_t* nodes_3[] = {&n1, &n2, &n3, &n4};
    assert_inorder_nodes_are(tree.root, nodes_3, 4, 0);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_insert_merge_lower -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_merge_lower()
{
    printf("test_insert_merge_lower: ");
    rb_tree_t tree;
    rb_tree_create(3, &tree);
    rb_tree_insert(5, &tree);
    rb_tree_insert(2, &tree);
    rb_tree_insert(10, &tree);
     
    rb_node_t n1 = {.range = {.value =  2, .offset = 0}, .color = RED};
    rb_node_t n2 = {.range = {.value =  5, .offset = 0}, .color = BLACK};
    rb_node_t n3 = {.range = {.value = 10, .offset = 0}, .color = RED};
    rb_node_t* nodes[] = {&n1, &n2, &n3};
    
    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 3, 0);
    
    /* After inserting the following values all node values in the tree should be replaced
       with the corresponding consecutive number that is lower. The offset should also be
       incremented to indicate that each node stores multiple values. */
    rb_tree_insert(4, &tree);
    rb_tree_insert(1, &tree);
    rb_tree_insert(9, &tree);
    rb_tree_insert(8, &tree);
    rb_tree_insert(7, &tree);
    rb_tree_insert(0, &tree);
   
    n1.range.value = 0;
    n1.range.offset = 2;

    n2.range.value = 4;
    n2.range.offset = 1;

    n3.range.value = 7;
    n3.range.offset = 3;

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 3, 0);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_insert_merge_upper -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_merge_upper()
{
    printf("test_insert_merge_upper: ");
    rb_tree_t tree;
    rb_tree_create(3, &tree);
    rb_tree_insert(5, &tree);
    rb_tree_insert(2, &tree);
    rb_tree_insert(10, &tree);
     
    rb_node_t n1 = {.range = {.value =  2, .offset = 0}, .color = RED};
    rb_node_t n2 = {.range = {.value =  5, .offset = 0}, .color = BLACK};
    rb_node_t n3 = {.range = {.value = 10, .offset = 0}, .color = RED};
    rb_node_t* nodes[] = {&n1, &n2, &n3};
    
    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 3, 0);
    
    /* After inserting the following values all node values in the tree should increase
       have increased offsets reflecting the fact that the node stores a range of inserted
       values. */
    rb_tree_insert(6, &tree);
    rb_tree_insert(7, &tree);
    rb_tree_insert(3, &tree);
    rb_tree_insert(11, &tree);
    rb_tree_insert(12, &tree);
    rb_tree_insert(13, &tree);
    rb_tree_insert(14, &tree);
    rb_tree_insert(15, &tree);
   
    n1.range.offset = 1;
    n2.range.offset = 2;
    n3.range.offset = 5;

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 3, 0);
    rb_tree_destroy(&tree);
    printf(" PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_insert_merge_lower_and_child -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_merge_lower_and_child()
{
    printf("test_insert_merge_lower_and_child: ");
    rb_tree_t tree;
    rb_tree_create(7, &tree);
    rb_tree_insert(20, &tree);
    rb_tree_insert(15, &tree);
    rb_tree_insert(25, &tree);
    rb_tree_insert(10, &tree);
    rb_tree_insert(30, &tree);
    rb_tree_insert(5,  &tree);
    rb_tree_insert(35, &tree);
 
    rb_node_t n1 = {.range = {.value =  5, .offset = 0}, .color = RED};
    rb_node_t n2 = {.range = {.value = 10, .offset = 0}, .color = BLACK}; 
    rb_node_t n3 = {.range = {.value = 15, .offset = 0}, .color = RED}; 
    rb_node_t n4 = {.range = {.value = 20, .offset = 0}, .color = BLACK};  
    rb_node_t n5 = {.range = {.value = 25, .offset = 0}, .color = RED};  
    rb_node_t n6 = {.range = {.value = 30, .offset = 0}, .color = BLACK}; 
    rb_node_t n7 = {.range = {.value = 35, .offset = 0}, .color = RED}; 
    rb_node_t* nodes_1[] = {&n1, &n2, &n3, &n4, &n5, &n6, &n7};

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes_1, 7, 0);
    rb_tree_insert(11, &tree);
    rb_tree_insert(12, &tree);
    rb_tree_insert(13, &tree);
    rb_tree_insert(14, &tree);


    n2.range.offset = 5;    
    rb_node_t* nodes_2[] = {&n1, &n2, &n4, &n5, &n6, &n7};
    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes_2, 6, 0);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_insert_merge_upper_and_child -
 *--------------------------------------------------------------------------------------*/  
static void test_insert_merge_upper_and_child()
{
    printf("test_insert_merge_upper_and_child: ");
    rb_tree_t tree;
    rb_tree_create(4, &tree);
    rb_tree_insert(20, &tree);
    rb_tree_insert(10, &tree);
    rb_tree_insert(28, &tree);
    rb_tree_insert(30, &tree);
    
    rb_node_t n1 = {.range = {.value = 10, .offset = 0}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 20, .offset = 0}, .color = BLACK}; 
    rb_node_t n3 = {.range = {.value = 28, .offset = 0}, .color = BLACK}; 
    rb_node_t n4 = {.range = {.value = 30, .offset = 0}, .color = RED};  
    rb_node_t* nodes_1[] = {&n1, &n2, &n3, &n4};

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes_1, 4, 0);

    rb_tree_insert(29, &tree);
    
    n3.range.offset = 2;
    rb_node_t* nodes_2[] = {&n1, &n2, &n3};

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes_2, 3, 0);
    rb_tree_destroy(&tree);
    printf("PASS \n");
}

/*--------------------------------------------------------------------------------------
 * test_merge_to_single_node -
 *--------------------------------------------------------------------------------------*/  
static void test_merge_to_single_node()
{
    printf("test_merge_to_single_node: ");
    rb_tree_t tree;
    rb_tree_create(10, &tree);
    rb_tree_insert(1, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(3, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(5, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(7, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(9, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(11, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(13, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(15, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(12, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(8, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(4, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(14, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(2, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(6, &tree);
    assert_rb_tree_is_valid(&tree);
    rb_tree_insert(10, &tree);
 
    rb_node_t final_node = {.range = {.value = 1, .offset = 14}, .color = false};
    rb_node_t* nodes[] = {&final_node};

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 1, 0);
    rb_tree_destroy(&tree);
    printf(" PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_no_duplicates -
 *--------------------------------------------------------------------------------------*/  
static void test_no_duplicates()
{
    printf("test_no_duplicates: ");
    rb_tree_t tree;
    rb_tree_create(10, &tree);
    rb_tree_insert(5, &tree);
    rb_tree_insert(10, &tree);
    rb_tree_insert(15, &tree);
    rb_node_t n1 = {.range = {.value =  5, .offset = 0}, .color = RED};
    rb_node_t n2 = {.range = {.value = 10, .offset = 0}, .color = BLACK}; 
    rb_node_t n3 = {.range = {.value = 15, .offset = 0}, .color = RED};
    rb_node_t* nodes[] = {&n1, &n2, &n3};

   
    assert(tree.size == 3);
    assert_rb_tree_is_valid(&tree); 
    assert_inorder_nodes_are(tree.root, nodes, 3, 0);

    assert(rb_tree_insert(5, &tree)  == RB_FAIL_INSERT_DUPLICATE);
    assert(rb_tree_insert(5, &tree)  == RB_FAIL_INSERT_DUPLICATE);
    assert(rb_tree_insert(10, &tree) == RB_FAIL_INSERT_DUPLICATE);
    assert(rb_tree_insert(10, &tree) == RB_FAIL_INSERT_DUPLICATE);
    assert(rb_tree_insert(15, &tree) == RB_FAIL_INSERT_DUPLICATE);
    assert(rb_tree_insert(15, &tree) == RB_FAIL_INSERT_DUPLICATE);
 
    assert(tree.size == 3);
    assert_rb_tree_is_valid(&tree); 
    assert_inorder_nodes_are(tree.root, nodes, 3, 0);
    rb_tree_destroy(&tree); 
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_are_consecutive -
 *--------------------------------------------------------------------------------------*/
static void test_are_consecutive()
{
    printf("test_are_consecutive: ");
    assert(are_consecutive(0, 1));
    assert(!are_consecutive(1, 0));
    assert(are_consecutive(UINT32_MAX - 1, UINT32_MAX));
    assert(!are_consecutive(UINT32_MAX, 0));
    printf("PASS\n");
}


/*--------------------------------------------------------------------------------------
 * test_max_size_configured_properly -
 *--------------------------------------------------------------------------------------*/
static void test_max_size_configured_properly()
{
    printf("test_max_size_configured_properly: ");
    assert(MAX_TREE_SIZE < UINT32_MAX);
    rb_tree_t tree;
    assert(rb_tree_create(UINT32_MAX, &tree) == RB_FAIL_EXCEEDED_MAX_SIZE);
    printf("PASS\n");
}

 /*--------------------------------------------------------------------------------------
 * test_max_range_offset -
 *--------------------------------------------------------------------------------------*/
static void test_max_range_offset()
{
    printf("test_max_range_offset: ");
    rb_tree_t tree;
    rb_tree_status_t status = rb_tree_create(UINT32_MAX - 1, &tree);
    assert(status == RB_SUCCESS || status == RB_FAIL_MEM_ERR);
    if (status == RB_FAIL_MEM_ERR)
    {
        /* Return early if no memory was allocated. */
        return;
    }

    for (uint32_t i = 0; i < UINT32_MAX - 1; i++)
    {
        rb_tree_insert(i, &tree);
    }
    assert_rb_tree_is_valid(&tree);

    rb_node_t n = {.range = {.value = 0, .offset = UINT32_MAX - 1}, .color = BLACK};
    rb_node_t* nodes[] = {&n};
    print_tree(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 1, 0);
    rb_tree_destroy(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_tree_traversed_and_deleted_inorder_with_rebalancing - 
 *--------------------------------------------------------------------------------------*/
static void test_unable_to_delete_value_that_does_not_exist()
{
    printf("test_unable_to_delete_value_that_does_not_exist: ");
    rb_tree_t tree;
    rb_tree_create(30, &tree);

    /* Write out 5 bundles and then skip 5 bundles. */
    for (uint32_t i = 0; i < 50; i += 10)
    {
        rb_tree_insert(i, &tree);
        rb_tree_insert(i + 1, &tree);
        rb_tree_insert(i + 2, &tree);
        rb_tree_insert(i + 3, &tree);
        rb_tree_insert(i + 4, &tree);
    }
    assert_rb_tree_is_valid(&tree);

    assert(tree.size == 5);
    assert(rb_tree_delete(6, &tree) == RB_FAIL_VALUE_NOT_FOUND);
    assert(rb_tree_delete(25, &tree) == RB_FAIL_VALUE_NOT_FOUND);
    assert(rb_tree_delete(39, &tree) == RB_FAIL_VALUE_NOT_FOUND);
    assert(tree.size == 5);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_delete_single_node - 
 *--------------------------------------------------------------------------------------*/
static void test_delete_single_node()
{
    printf("test_delete_single_node: ");
    rb_tree_t tree;
    rb_tree_create(10, &tree);
    for (uint32_t i = 0; i <= 10; i += 2)
    {
        rb_tree_insert(i, &tree);
    }
    assert(tree.size == 6);
    assert_rb_tree_is_valid(&tree);

    rb_node_t n1 = {.range = {.value = 0,  .offset = 0}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 2,  .offset = 0}, .color = BLACK};
    rb_node_t n3 = {.range = {.value = 4,  .offset = 0}, .color = BLACK};
    rb_node_t n4 = {.range = {.value = 6,  .offset = 0}, .color = RED};
    rb_node_t n5 = {.range = {.value = 8,  .offset = 0}, .color = BLACK};
    rb_node_t n6 = {.range = {.value = 10, .offset = 0}, .color = RED};

    rb_node_t* nodes_start[] = {&n1, &n2, &n3, &n4, &n5, &n6};
    assert_inorder_nodes_are(tree.root, nodes_start, 6, 0);

    rb_tree_delete(6, &tree);
    n5.color = RED;
    n6.color = BLACK;
    rb_node_t* nodes_end[] = {&n1, &n2, &n3, &n5, &n6};
    assert_inorder_nodes_are(tree.root, nodes_end, 5, 0);
    assert(tree.size == 5);
    assert_rb_tree_is_valid(&tree);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_tree_traversed_and_deleted_inorder_with_rebalancing - 
 *--------------------------------------------------------------------------------------*/
static void test_deletes_node_at_start_of_range()
{
    printf("test_deletes_node_at_start_of_range: ");
    rb_tree_t tree;
    rb_tree_create(10, &tree);
    rb_tree_insert(0, &tree);
    rb_tree_insert(1, &tree);
    rb_tree_insert(2, &tree);
    rb_tree_insert(5, &tree);
    rb_tree_insert(6, &tree);
    rb_tree_insert(7, &tree);
    rb_tree_insert(9, &tree);
    rb_tree_insert(10, &tree);
    rb_tree_insert(13, &tree);
    rb_tree_insert(14, &tree);
    rb_tree_insert(16, &tree);

    assert(tree.size == 5);

    rb_node_t n1 = {.range = {.value = 0,  .offset = 2}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 5,  .offset = 2}, .color = BLACK};
    rb_node_t n3 = {.range = {.value = 9,  .offset = 1}, .color = RED};
    rb_node_t n4 = {.range = {.value = 13, .offset = 1}, .color = BLACK};
    rb_node_t n5 = {.range = {.value = 16, .offset = 0}, .color = RED};
    rb_node_t* nodes[] = {&n1, &n2, &n3, &n4, &n5};

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 5, 0);

    rb_tree_delete(13, &tree);
    assert(tree.size == 5);
    n4.range.value = 14;
    n4.range.offset = 0;
    assert_inorder_nodes_are(tree.root, nodes, 5, 0);
    
    rb_tree_delete(5, &tree);
    assert(tree.size == 5);
    n2.range.value = 6;
    n2.range.offset = 1;
    assert_inorder_nodes_are(tree.root, nodes, 5, 0);
    assert_rb_tree_is_valid(&tree);

    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_deletes_node_at_end_of_range - 
 *--------------------------------------------------------------------------------------*/
static void test_deletes_node_at_end_of_range()
{
    printf("test_deletes_node_at_end_of_range: ");
    rb_tree_t tree;
    rb_tree_create(10, &tree);
    rb_tree_insert(0, &tree);
    rb_tree_insert(1, &tree);
    rb_tree_insert(2, &tree);
    rb_tree_insert(5, &tree);
    rb_tree_insert(6, &tree);
    rb_tree_insert(7, &tree);
    rb_tree_insert(9, &tree);
    rb_tree_insert(10, &tree);
    rb_tree_insert(13, &tree);
    rb_tree_insert(14, &tree);
    rb_tree_insert(16, &tree);

    assert(tree.size == 5);

    rb_node_t n1 = {.range = {.value = 0,  .offset = 2}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 5,  .offset = 2}, .color = BLACK};
    rb_node_t n3 = {.range = {.value = 9,  .offset = 1}, .color = RED};
    rb_node_t n4 = {.range = {.value = 13, .offset = 1}, .color = BLACK};
    rb_node_t n5 = {.range = {.value = 16, .offset = 0}, .color = RED};
    rb_node_t* nodes[] = {&n1, &n2, &n3, &n4, &n5};

    assert_rb_tree_is_valid(&tree);
    assert_inorder_nodes_are(tree.root, nodes, 5, 0);

    rb_tree_delete(14, &tree);
    assert(tree.size == 5);
    n4.range.offset = 0;
    assert_inorder_nodes_are(tree.root, nodes, 5, 0);
    
    rb_tree_delete(7, &tree);
    assert(tree.size == 5);
    n2.range.offset = 1;
    assert_inorder_nodes_are(tree.root, nodes, 5, 0);
    assert_rb_tree_is_valid(&tree);

    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_deletes_node_inside_of_range - 
 *--------------------------------------------------------------------------------------*/
static void test_deletes_node_inside_of_range()
{
    printf("test_deletes_node_inside_of_range: ");
    rb_tree_t tree;
    rb_tree_create(10, &tree);
    rb_tree_insert(0, &tree);
    rb_tree_insert(1, &tree);
    rb_tree_insert(2, &tree);
    rb_tree_insert(5, &tree);
    rb_tree_insert(6, &tree);
    rb_tree_insert(7, &tree);
    rb_tree_insert(8, &tree);
    rb_tree_insert(9, &tree);
    rb_tree_insert(13, &tree);
    rb_tree_insert(14, &tree);
    rb_tree_insert(16, &tree);
    rb_tree_insert(18, &tree);

    rb_node_t n1 = {.range = {.value = 0,  .offset = 2}, .color = BLACK};
    rb_node_t n2 = {.range = {.value = 5,  .offset = 4}, .color = BLACK};
    rb_node_t n3 = {.range = {.value = 13, .offset = 1}, .color = RED};
    rb_node_t n4 = {.range = {.value = 16, .offset = 0}, .color = BLACK};
    rb_node_t n5 = {.range = {.value = 18, .offset = 0}, .color = RED};

    rb_node_t* nodes_start[] = {&n1, &n2, &n3, &n4, &n5};

    assert_rb_tree_is_valid(&tree);
    assert(tree.size == 5);
    assert_inorder_nodes_are(tree.root, nodes_start, 5, 0);

    rb_tree_delete(6, &tree);

    n2.range.offset = 0;
    rb_node_t n6 = {.range = {.value = 7, .offset = 2}, .color = RED};
    rb_node_t* nodes_end[] = {&n1, &n2, &n6, &n3, &n4, &n5};
    assert(tree.size == 6);
    assert_inorder_nodes_are(tree.root, nodes_end, 6, 0);
    assert_rb_tree_is_valid(&tree);

    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_tree_traversed_and_deleted_inorder_with_rebalancing - 
 *--------------------------------------------------------------------------------------*/
static void test_tree_traversed_and_deleted_inorder_with_rebalancing()
{
    printf("test_tree_traversed_and_deleted_inorder_with_rebalancing: ");
    rb_tree_t tree;
    uint32_t max_nodes = 10;
    rb_tree_create(max_nodes, &tree);
    assert(rb_tree_insert(6, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(2, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(3, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(8, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(10, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(12, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(11, &tree) == RB_SUCCESS);
    assert_rb_tree_is_valid(&tree);
    assert(tree.size == 4);

    rb_node_t* iter = tree.root;
    rb_range_t range;

    assert(rb_tree_get_first_rb_node(NULL, &iter) == RB_FAIL_NULL_TREE); 
    assert(rb_tree_get_first_rb_node(&tree, &iter) == RB_SUCCESS);
    assert(rb_tree_get_next_rb_node(&tree, &iter, NULL, true, true) 
        == RB_FAIL_NULL_RANGE); 
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, true) == RB_SUCCESS); 
    assert(range.value == 2 && range.offset == 1);
    assert_rb_tree_is_valid(&tree);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, true) == RB_SUCCESS); 
    assert(range.value == 6 && range.offset == 0);
    assert_rb_tree_is_valid(&tree);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, true) == RB_SUCCESS); 
    assert(range.value == 8 && range.offset == 0);
    assert_rb_tree_is_valid(&tree);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, true) == RB_SUCCESS);
    assert(range.value == 10 && range.offset == 2); 
    assert_rb_tree_is_valid(&tree);
    
    assert(iter == NULL);
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, true) == 
        RB_FAIL_NULL_NODE); 
    assert(tree.size == 0);
    printf("PASS\n");
}


/*--------------------------------------------------------------------------------------
 * test_tree_traversed_and_deleted_inorder_without_rebalancing - 
 *--------------------------------------------------------------------------------------*/
static void test_tree_traversed_and_deleted_inorder_without_rebalancing()
{
    printf("test_tree_traversed_and_deleted_inorder_without_rebalancing: ");
    rb_tree_t tree;
    uint32_t max_nodes = 10;
    rb_tree_create(max_nodes, &tree);
    assert(rb_tree_insert(6, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(2, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(3, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(8, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(10, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(12, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(11, &tree) == RB_SUCCESS);
    assert_rb_tree_is_valid(&tree);
    assert(tree.size == 4);

    rb_node_t* iter = tree.root;
    rb_range_t range;

    assert(rb_tree_get_first_rb_node(&tree, &iter) == RB_SUCCESS);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, false) == RB_SUCCESS); 
    assert(range.value == 2 && range.offset == 1);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, false) == RB_SUCCESS); 
    assert(range.value == 6 && range.offset == 0);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, false) == RB_SUCCESS); 
    assert(range.value == 8 && range.offset == 0);
    
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, false) == RB_SUCCESS);
    assert(range.value == 10 && range.offset == 2); 
    assert(iter == NULL);
    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, false) == 
        RB_FAIL_NULL_NODE); 
    assert(tree.size == 0);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_tree_traversed_inorder_after_partial_traversal - 
 *--------------------------------------------------------------------------------------*/
static void test_tree_traversed_inorder_after_partial_traversal()
{
    printf("test_tree_traversed_inorder_after_partial_traversal: ");
    rb_tree_t tree;
    uint32_t max_nodes = 10;
    rb_tree_create(max_nodes, &tree);
    assert(rb_tree_insert(2, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(4, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(6, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(8, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(10, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(12, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(14, &tree) == RB_SUCCESS);
    assert(rb_tree_insert(16, &tree) == RB_SUCCESS);
    assert_rb_tree_is_valid(&tree);
    assert(tree.size == 8);

    rb_node_t* iter = tree.root;
    rb_range_t range;

    assert(rb_tree_get_first_rb_node(&tree, &iter) == RB_SUCCESS);
    for (uint32_t i = 2; i <= 8; i += 2)
    {
        assert(rb_tree_get_next_rb_node(&tree, &iter, &range, false, false) == RB_SUCCESS); 
        assert(range.value == i && range.offset == 0);
    }

    assert(rb_tree_get_first_rb_node(&tree, &iter) == RB_SUCCESS);
    for (uint32_t i = 2; i <= 16; i += 2)
    {
        assert(rb_tree_get_next_rb_node(&tree, &iter, &range, false, false) == RB_SUCCESS); 
        assert(range.value == i && range.offset == 0);
    }

    assert(rb_tree_get_next_rb_node(&tree, &iter, &range, true, false) == 
        RB_FAIL_NULL_NODE); 
    assert(tree.size == 8);
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * test_random_stress -
 *--------------------------------------------------------------------------------------*/  
static void test_random_stress()
{
    printf("test_random_stress: ");
    uint32_t number_trees = 10;
    uint32_t max_bundles = 16000; 
    rb_node_t n = {.range = {.value = 0, .offset = max_bundles - 1}, .color = BLACK};
    rb_node_t* nodes[] = {&n};

    uint32_t bundle_ids[max_bundles];
    for (uint32_t i = 0; i < max_bundles; i++)
    {
        bundle_ids[i] = i;
    }

    for (uint32_t i = 0; i < number_trees; i++)
    {
        rb_tree_t tree; 
        rb_tree_create(max_bundles, &tree);
        shuffle(bundle_ids, max_bundles);
        for (uint32_t j = 0; j < max_bundles; j++)
        {
            rb_tree_insert(bundle_ids[j], &tree);
            assert_rb_tree_is_valid(&tree);
        }
        assert_inorder_nodes_are(tree.root, nodes, 1, 0);
        rb_tree_destroy(&tree);
    }
    printf("PASS\n");
}

/*--------------------------------------------------------------------------------------
 * run_tests - Run all rb_tree tests. 
 *--------------------------------------------------------------------------------------*/  
static void run_rb_tree_tests()
{
    printf("Running RB Tree Tests...\n");
    test_new_tree_empty();
    test_unable_to_insert_into_empty_tree();
    test_unable_to_insert_into_full_tree();
    test_deletes_tree();
    test_insert_root();
    test_insert_left_subtree();
    test_insert_right_subtree();
    test_insert_merge_lower();
    test_insert_merge_upper();
    test_insert_merge_lower_and_child();
    test_insert_merge_upper_and_child();
    test_merge_to_single_node();
    test_no_duplicates();
    test_are_consecutive();
    test_max_size_configured_properly();
    test_max_range_offset();
    test_unable_to_delete_value_that_does_not_exist();
    test_delete_single_node();
    test_deletes_node_at_start_of_range();
    test_deletes_node_at_end_of_range();
    test_deletes_node_inside_of_range();
    test_tree_traversed_and_deleted_inorder_with_rebalancing();
    test_tree_traversed_and_deleted_inorder_without_rebalancing();
    test_tree_traversed_inorder_after_partial_traversal();
    test_random_stress();
    printf("All tests passed!\n");
}

int main() {    
    run_rb_tree_tests();
}

#endif /* RBTREETESTS */