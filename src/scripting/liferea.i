%module liferea
%{
#include "../node.h"
#include "../feed.h"
#include "../item.h"
#include "../itemlist.h" 
%}

#define gchar	char

%include "../node.h"
%include "../item.h"
%include "../itemlist.h"
%include "../feedlist.h"
