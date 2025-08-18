## Liferea UI architecture

This is a rough schematic how the code is designed. In general the idea
is to follow the typical GTK MVP (model-view-presenter) pattern.

There are models

- src/itemlist.c
- src/feedlist.c
- src/node.c
- src/item.c
- ...

There are actions

- src/actions/node_actions.c
- src/actions/item_actions.c
- src/actions/shell_actions.c

Views are 

- GtkTreeView (for itemlist and feedlist)
  - in the future probably migrated to GtkListView
- WebkitWebView (for content rendering)

Presenter implementations can be found in

- src/ui/feed_list_view.c
- src/ui/item_list_view.c

To map multiple views reacting to selection changes we apply an observer pattern
for itemlist/feedlist model select state changes in

- src/ui/feed_list_view.c
- src/ui/item_list_view.c
- src/ui/content_view.c
- src/ui/item_actions.c
- src/ui/node_actions.c

Custom widgets

- src/ui/liferea_browser.c (encapsulating Webkit and a browser bar)
- src/ui/rule_editor.c (reusable rules dialog for search + search folders)
- src/ui/gedit-close-button.c (to be removed)

Other

- src/ui/liferea_shell.c is the overall large glue tape ball.
- src/ui/liferea_dialog.c is a generic gresource based dialog loading
- src/ui/browser_tabs.c is handling different types of tabs in a GtkNotebook
- Dialogs callbacks are defined in GtkBuilder XML only
- All src/ui/*_dialog.c are more or less isolated standalone code
