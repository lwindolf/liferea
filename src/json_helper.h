/*// C++ Interface: json_helper*/
/*// Description:*/
/*// Author: Rui Maciel <rui_maciel@users.sourceforge.net>, (C) 2007*/
/*// Copyright: See COPYING file that comes with this distribution*/


#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "json.h"


/**
Renders the tree structure where root is the tree's root, which can also be a tree branch. This function is used recursively by json_render_tree()
@param root the tree's root node (may be a child node)
@param level the indentation level (number of tabs)
 **/
void json_render_tree_indented (json_t * root, int level);

/**
Renders the tree structure where root is the tree's root, which can also be a tree branch.
@param root the tree's root node (may be a child node)
 **/
void json_render_tree (json_t * root);


#endif
