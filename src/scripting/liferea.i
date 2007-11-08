%module liferea
%{
#include "../node.h"
#include "../feed.h"
#include "../feedlist.h"
#include "../item.h"
#include "../itemlist.h" 
#include "../social.h"
#include "../subscription.h"

#include "../ui/ui_feedlist.h"
#include "../ui/ui_itemlist.h"
#include "../ui/ui_mainwindow.h"
#include "../ui/ui_node.h"
%}
 
#define gchar	char
#define gint	int
#define guint	unsigned int
#define gboolean	int

%include "../node.h"
%include "../item.h"
%include "../itemlist.h"
%include "../feedlist.h"
%include "../social.h"
%include "../subscription.h"

%include "../ui/ui_feedlist.h"
%include "../ui/ui_itemlist.h"
%include "../ui/ui_mainwindow.h"
%include "../ui/ui_node.h"
