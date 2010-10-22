/*
*  C Implementation: json_helper
*
* Description:
*
*
* Author: Rui Maciel <rui_maciel@users.sourceforge.net>, (C) 2007
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "json_helper.h"

#include <stdio.h>
#include <assert.h>


void
json_render_tree_indented (json_t * root, int level)
{
	int tab;
	assert (root != NULL);
	for (tab = 0; tab < level; tab++)
	{
		printf ("> ");
	}
	switch (root->type)
	{
	case JSON_STRING:
		printf ("STRING: %s\n", root->text);
		break;
	case JSON_NUMBER:
		printf ("NUMBER: %s\n", root->text);
		break;
	case JSON_OBJECT:
		printf ("OBJECT: \n");
		break;
	case JSON_ARRAY:
		printf ("ARRAY: \n");
		break;
	case JSON_TRUE:
		printf ("TRUE:\n");
		break;
	case JSON_FALSE:
		printf ("FALSE:\n");
		break;
	case JSON_NULL:
		printf ("NULL:\n");
		break;
	}
	/* iterate through children */
	if (root->child != NULL)
	{
		json_t *ita, *itb;
		ita = root->child;
		while (ita != NULL)
		{
			json_render_tree_indented (ita, level + 1);
			itb = ita->next;
			ita = itb;
		}
	}
}


void
json_render_tree (json_t * root)
{
	assert (root != NULL);
	json_render_tree_indented (root, 0);
}
