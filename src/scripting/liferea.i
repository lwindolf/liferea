%module liferea
%{
#include "../node.h"
#include "../feed.h"
#include "../item.h"
#include "../item_state.h"
#include "../itemlist.h" 
#include "../social.h"
#include "../subscription.h"

#include "../ui/ui_feedlist.h"
#include "../ui/ui_node.h"
%}
 
#define gchar	char
#define gint	int
#define guint	unsigned int
#define gboolean	int

%include "../node.h"
%include "../item.h"
%include "../item_state.h"
%include "../itemlist.h"
%include "../social.h"
%include "../subscription.h"

%include "../ui/ui_feedlist.h"
%include "../ui/ui_node.h"
