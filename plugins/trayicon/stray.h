/* stray.h - library for system tray icons */

#ifndef STRAY_H
#define STRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <dbus/dbus.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STRAY_OBJECT_PATH        "/StatusNotifierItem"
#define STRAY_INTERFACE_NAME     "org.kde.StatusNotifierItem"
#define STRAY_WATCHER_SERVICE    "org.kde.StatusNotifierWatcher"
#define STRAY_WATCHER_PATH       "/StatusNotifierWatcher"
#define STRAY_MENU_OBJECT_PATH   "/StatusNotifierMenu"
#define STRAY_DBUSMENU_INTERFACE "com.canonical.dbusmenu"

#define STRAY_DEFAULT_ICON       "application-x-executable"
#define STRAY_DEFAULT_TITLE      "My Application"
#define STRAY_DEFAULT_ID         "my-app"

typedef struct TrayIcon TrayIcon;
typedef struct TrayMenu TrayMenu;
typedef struct TrayMenuItem TrayMenuItem;
typedef struct {
    int width, height;
    uint32_t *data; /* ARGB32 format, each pixel is 32 bits */
} StrayPixmap;

/* Click button types */
typedef enum {
    STRAY_BUTTON_LEFT = 1,
    STRAY_BUTTON_MIDDLE = 2,
    STRAY_BUTTON_RIGHT = 3
} TrayButton;

/* Scroll direction */
typedef enum {
    STRAY_SCROLL_UP = 0,
    STRAY_SCROLL_DOWN = 1,
    STRAY_SCROLL_LEFT = 2,
    STRAY_SCROLL_RIGHT = 3
} TrayScrollDirection;

/* Scroll orientation */
typedef enum {
    STRAY_ORIENTATION_VERTICAL = 0,
    STRAY_ORIENTATION_HORIZONTAL = 1
} TrayScrollOrientation;

/* Menu item types */
typedef enum {
    STRAY_MENU_ITEM_NORMAL = 0,
    STRAY_MENU_ITEM_SEPARATOR = 1,
    STRAY_MENU_ITEM_CHECK = 2,
    STRAY_MENU_ITEM_RADIO = 3
} TrayMenuItemType;

typedef enum {
    STRAY_STATUS_PASSIVE = 0,
    STRAY_STATUS_ACTIVE = 1,
    STRAY_STATUS_NEEDS_ATTENTION = 2
} TrayStatus;

typedef void (*TrayMenuCallback)(int menu_id, void *user_data);
typedef void (*TrayClickCallback)(int x, int y, void *user_data);
typedef void (*TrayButtonCallback)(
    TrayButton button, int x, int y, void *user_data
);

typedef void (*TrayScrollCallback)(
    TrayScrollDirection direction, int delta, void *user_data
);

/* Icon API */

/* Creates the tray icon. */
TrayIcon *
stray_create(const char *app_name, const char *icon_name, const char *title);
/* Sets the icon status */
void stray_set_status(TrayIcon *icon, TrayStatus status);
/* Sets the click callback (button-agnostic). */
void stray_set_click_callback(
    TrayIcon *icon, TrayClickCallback callback, void *user_data
);
/* Sets the button callback for left, middle, and right clicks. */
void stray_set_button_callback(
    TrayIcon *icon, TrayButtonCallback callback, void *user_data
);
/* Sets the scroll callback. */
void stray_set_scroll_callback(
    TrayIcon *icon, TrayScrollCallback callback, void *user_data
);

/* Processes the events. */
void stray_process_events(TrayIcon *icon);
/* Sets the icon title. */
void stray_set_title(TrayIcon *icon, const char *title);
/* Sets the named icon. */
void stray_set_icon(TrayIcon *icon, const char *icon_name);
/* Sets the pixmap icon. */
void stray_set_icon_pixmap(
    TrayIcon *icon, int width, int height, const uint32_t *data
);
/* Sets the tooltip. */
void stray_set_tooltip(TrayIcon *icon, const char *title, const char *text);
/* Sets the window ID for the icon. */
void stray_set_window_id(TrayIcon *icon, dbus_uint32_t window_id);
/* Destroys the icon and its content, then unregisters from D-Bus */
void stray_destroy(TrayIcon *icon);
/* Registers with D-Bus */
int stray_register(TrayIcon *icon);
/* Returns the Unix file descriptor for the D-Bus connection */
int stray_get_fd(TrayIcon *icon);

/* Menu API */

/* Creates the menu */
TrayMenu *stray_menu_create(void);
/* Sets the menu for the icon. */
void stray_set_menu(TrayIcon *icon, TrayMenu *menu);
/* Sets the item label */
void stray_menu_set_item_label(TrayMenu *menu, int item_id, const char *label);
/* Adds a separator. */
int stray_menu_add_separator(TrayMenu *menu);
/* Adds a menu item. */
int stray_menu_add_item(
    TrayMenu *menu, const char *label, TrayMenuCallback callback,
    void *user_data
);

/* Adds a checked item. */
int stray_menu_add_check_item(
    TrayMenu *menu, const char *label, TrayMenuCallback callback,
    void *user_data
);

/* Adds a radio item. */
int stray_menu_add_radio_item(
    TrayMenu *menu, const char *label, TrayMenuCallback callback,
    void *user_data
);

/* Sets the state for a checked item. */
void stray_menu_set_item_checked(
    TrayMenu *menu, int item_id, dbus_bool_t checked
);

/* Sets the item state. */
void stray_menu_set_item_enabled(
    TrayMenu *menu, int item_id, dbus_bool_t enabled
);

/* Sets a named icon for a menu item. */
void stray_menu_set_item_icon(
    TrayMenu *menu, int item_id, const char *icon_name
);

/* Adds a submenu to the main menu. */
int stray_menu_add_submenu(
    TrayMenu *menu, const char *label, TrayMenu *submenu
);

TrayMenu *stray_menu_get_submenu(TrayMenu *menu, int item_id);

#ifdef STRAY_IMPL

struct TrayMenuItem {
    TrayMenuCallback callback;
    TrayMenuItemType type;
    TrayMenu *submenu;

    int id;
    char *label;
    char *icon_name;

    dbus_bool_t enabled;
    dbus_bool_t checked;

    void *user_data;
};

struct TrayMenu {
    TrayMenuItem **items;
    TrayMenu *parent;
    TrayIcon *icon;

    int item_count;
    int item_capacity;
    int *next_id_ptr;

    dbus_uint32_t revision;
};

struct TrayIcon {
    char *app_id;
    char *service_name;
    char *icon_name;
    char *title;
    char *tooltip_title;
    char *tooltip_text;

    TrayStatus status;
    TrayMenu *menu;

    TrayClickCallback click_callback;
    TrayButtonCallback button_callback;
    TrayScrollCallback scroll_callback;

    void *click_user_data;
    void *button_user_data;
    void *scroll_user_data;

    int registered;
    int filter_added;
    int match_added;
    int icon_pixmap_count;

    dbus_uint32_t window_id;

    StrayPixmap *icon_pixmaps;
    DBusConnection *conn;
};

static char *safe_strdup(const char *str) { return str ? strdup(str) : NULL; }
static void safe_free(char **str) {
    if (str && *str) {
        free(*str);
        *str = NULL;
    }
}

static const char *status_to_string(TrayStatus status) {
    switch (status) {
        case STRAY_STATUS_PASSIVE: return "Passive";
        case STRAY_STATUS_ACTIVE: return "Active";
        case STRAY_STATUS_NEEDS_ATTENTION: return "NeedsAttention";
        default: return "Active";
    }
}

static int stray_instance_counter = 0;
static int stray_global_menu_next_id = 1;

static void add_variant(
    DBusMessageIter *args, int type, const char *sig, const void *value
) {
    DBusMessageIter variant;
    dbus_message_iter_open_container(args, DBUS_TYPE_VARIANT, sig, &variant);
    dbus_message_iter_append_basic(&variant, type, value);
    dbus_message_iter_close_container(args, &variant);
}

static void add_dict_entry(
    DBusMessageIter *array, const char *key, int type, const char *sig,
    const void *value
) {
    DBusMessageIter dict_entry, variant;
    dbus_message_iter_open_container(
        array, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry
    );

    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(
        &dict_entry, DBUS_TYPE_VARIANT, sig, &variant
    );

    dbus_message_iter_append_basic(&variant, type, value);
    dbus_message_iter_close_container(&dict_entry, &variant);
    dbus_message_iter_close_container(array, &dict_entry);
}

static void add_pixmap_array(DBusMessageIter *variant, TrayIcon *icon) {
    DBusMessageIter pixmap_array, pixmap_struct, data_array;

    /* open array of (iiay) */
    dbus_message_iter_open_container(
        variant, DBUS_TYPE_ARRAY, "(iiay)", &pixmap_array
    );

    if (icon && icon->icon_pixmaps && icon->icon_pixmap_count > 0) {
        int i;
        for (i = 0; i < icon->icon_pixmap_count; i++) {
            dbus_int32_t width;
            dbus_int32_t height;
            StrayPixmap *pixmap = &icon->icon_pixmaps[i];

            dbus_message_iter_open_container(
                &pixmap_array, DBUS_TYPE_STRUCT, NULL, &pixmap_struct
            );

            width = pixmap->width;
            dbus_message_iter_append_basic(
                &pixmap_struct, DBUS_TYPE_INT32, &width
            );

            height = pixmap->height;
            dbus_message_iter_append_basic(
                &pixmap_struct, DBUS_TYPE_INT32, &height
            );

            dbus_message_iter_open_container(
                &pixmap_struct, DBUS_TYPE_ARRAY, "y", &data_array
            );

            if (pixmap->data) {
                /* convert ARGB32 data to byte array */
                size_t data_size =
                    /* 4 bytes per pixel */
                    pixmap->width * pixmap->height * 4;

                const uint8_t *byte_data = (const uint8_t *)pixmap->data;
                dbus_message_iter_append_fixed_array(
                    &data_array, DBUS_TYPE_BYTE, &byte_data, data_size
                );
            }

            dbus_message_iter_close_container(&pixmap_struct, &data_array);
            dbus_message_iter_close_container(&pixmap_array, &pixmap_struct);
        }
    }

    dbus_message_iter_close_container(variant, &pixmap_array);
}

static void add_tooltip_struct(DBusMessageIter *variant, TrayIcon *icon) {
    DBusMessageIter struct_iter, array_iter;
    const char *icon_name = "";
    const char *title = icon->tooltip_title ? icon->tooltip_title : "";
    const char *text = icon->tooltip_text ? icon->tooltip_text : "";

    dbus_message_iter_open_container(
        variant, DBUS_TYPE_STRUCT, NULL, &struct_iter
    );

    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &icon_name);
    dbus_message_iter_open_container(
        &struct_iter, DBUS_TYPE_ARRAY, "(iiay)", &array_iter
    );

    dbus_message_iter_close_container(&struct_iter, &array_iter);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &title);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &text);
    dbus_message_iter_close_container(variant, &struct_iter);
}

static void get_icon_properties(
    TrayIcon *icon, const char **out_icon, const char **out_title,
    const char **out_menu, dbus_bool_t *out_is_menu, const char **out_id,
    const char **out_status, dbus_uint32_t *out_window_id
) {
    *out_icon = icon->icon_name ? icon->icon_name : STRAY_DEFAULT_ICON;
    *out_title = icon->title ? icon->title : STRAY_DEFAULT_TITLE;
    *out_menu = icon->menu ? STRAY_MENU_OBJECT_PATH : "/NO_DBUSMENU";
    *out_is_menu = (icon->menu != NULL);
    *out_id = icon->app_id ? icon->app_id : STRAY_DEFAULT_ID;
    *out_window_id = icon->window_id;
    *out_status = status_to_string(icon->status);
}

static void emit_signal(TrayIcon *icon, const char *signal_name) {
    DBusMessage *msg;

    if (!icon) return;

    msg = dbus_message_new_signal(
        STRAY_OBJECT_PATH, STRAY_INTERFACE_NAME, signal_name
    );

    if (msg) {
        dbus_connection_send(icon->conn, msg, NULL);
        dbus_connection_flush(icon->conn);
        dbus_message_unref(msg);
    }
}

static void
emit_signal_string(TrayIcon *icon, const char *signal_name, const char *value) {
    DBusMessage *msg;

    if (!icon || !value) return;

    msg = dbus_message_new_signal(
        STRAY_OBJECT_PATH, STRAY_INTERFACE_NAME, signal_name
    );

    if (msg) {
        DBusMessageIter args;
        dbus_message_iter_init_append(msg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &value);
        dbus_connection_send(icon->conn, msg, NULL);
        dbus_connection_flush(icon->conn);
        dbus_message_unref(msg);
    }
}

static void emit_properties_changed(TrayIcon *icon, const char *property_name) {
    const char *interface;
    const char *current_icon;
    const char *current_title;
    const char *menu_path;
    const char *id_str;
    const char *status_str;
    const char *empty_str;
    DBusMessageIter args, changed_props, invalidated_props;
    DBusMessage *msg;
    dbus_bool_t item_is_menu;
    dbus_uint32_t window_id;
    int all;

    if (!icon) return;

    msg = dbus_message_new_signal(
        STRAY_OBJECT_PATH, "org.freedesktop.DBus.Properties",
        "PropertiesChanged"
    );

    if (!msg) return;

    get_icon_properties(
        icon, &current_icon, &current_title, &menu_path, &item_is_menu, &id_str,
        &status_str, &window_id
    );

    interface = STRAY_INTERFACE_NAME;
    empty_str = "";

    dbus_message_iter_init_append(msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_ARRAY, "{sv}", &changed_props
    );

    all = strcmp(property_name, "All") == 0;

    if (all || strcmp(property_name, "IconName") == 0)
        add_dict_entry(
            &changed_props, "IconName", DBUS_TYPE_STRING, "s", &current_icon
        );

    if (all || strcmp(property_name, "Title") == 0)
        add_dict_entry(
            &changed_props, "Title", DBUS_TYPE_STRING, "s", &current_title
        );

    if (all || strcmp(property_name, "Menu") == 0)
        add_dict_entry(
            &changed_props, "Menu", DBUS_TYPE_OBJECT_PATH, "o", &menu_path
        );

    if (all || strcmp(property_name, "ItemIsMenu") == 0)
        add_dict_entry(
            &changed_props, "ItemIsMenu", DBUS_TYPE_BOOLEAN, "b", &item_is_menu
        );

    if (all || strcmp(property_name, "Id") == 0)
        add_dict_entry(&changed_props, "Id", DBUS_TYPE_STRING, "s", &id_str);

    if (all || strcmp(property_name, "Status") == 0)
        add_dict_entry(
            &changed_props, "Status", DBUS_TYPE_STRING, "s", &status_str
        );

    if (all || strcmp(property_name, "WindowId") == 0)
        add_dict_entry(
            &changed_props, "WindowId", DBUS_TYPE_UINT32, "u", &window_id
        );

    if (all || strcmp(property_name, "IconPixmap") == 0) {
        const char *key = "IconPixmap";
        DBusMessageIter dict_entry, variant;
        dbus_message_iter_open_container(
            &changed_props, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry
        );

        dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(
            &dict_entry, DBUS_TYPE_VARIANT, "a(iiay)", &variant
        );

        add_pixmap_array(&variant, icon);
        dbus_message_iter_close_container(&dict_entry, &variant);
        dbus_message_iter_close_container(&changed_props, &dict_entry);
    }

    if (all || strcmp(property_name, "ToolTip") == 0) {
        const char *key = "ToolTip";
        DBusMessageIter dict_entry, variant;
        dbus_message_iter_open_container(
            &changed_props, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry
        );

        dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(
            &dict_entry, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &variant
        );

        add_tooltip_struct(&variant, icon);
        dbus_message_iter_close_container(&dict_entry, &variant);
        dbus_message_iter_close_container(&changed_props, &dict_entry);
    }

    dbus_message_iter_close_container(&args, &changed_props);
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_ARRAY, "s", &invalidated_props
    );

    dbus_message_iter_close_container(&args, &invalidated_props);
    dbus_connection_send(icon->conn, msg, NULL);
    dbus_connection_flush(icon->conn);
    dbus_message_unref(msg);
}

static void stray_free_icon_pixmap(TrayIcon *icon) {
    if (!icon || !icon->icon_pixmaps) return;

    int i;
    for (i = 0; i < icon->icon_pixmap_count; i++)
        free(icon->icon_pixmaps[i].data);

    free(icon->icon_pixmaps);
    icon->icon_pixmaps = NULL;
    icon->icon_pixmap_count = 0;
}

static TrayMenuItem *find_menu_item(TrayMenu *menu, dbus_int32_t id) {
    int i;
    TrayMenuItem *item;

    if (!menu) return NULL;

    /* check items in this menu */
    for (i = 0; i < menu->item_count; i++) {
        if (menu->items[i] && menu->items[i]->id == id) {
            return menu->items[i];
        }

        /* check submenu presence */
        if (menu->items[i] && menu->items[i]->submenu) {
            item = find_menu_item(menu->items[i]->submenu, id);
            if (item) return item;
        }
    }

    return NULL;
}

static TrayIcon *get_root_icon(TrayMenu *menu) {
    TrayMenu *root = menu;

    if (!menu) return NULL;

    while (root->parent) { root = root->parent; }

    return root->icon;
}

static void emit_layout_updated(TrayIcon *icon, dbus_int32_t parent_id) {
    DBusMessage *msg;
    dbus_uint32_t revision;

    if (!icon || !icon->menu) return;

    revision = icon->menu->revision;

    msg = dbus_message_new_signal(
        STRAY_MENU_OBJECT_PATH, STRAY_DBUSMENU_INTERFACE, "LayoutUpdated"
    );

    if (msg) {
        DBusMessageIter args;
        dbus_message_iter_init_append(msg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &revision);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &parent_id);
        dbus_connection_send(icon->conn, msg, NULL);
        dbus_connection_flush(icon->conn);
        dbus_message_unref(msg);
    }
}

static void
add_menu_item_properties(DBusMessageIter *props, TrayMenuItem *item) {
    dbus_bool_t visible;

    if (item->type == STRAY_MENU_ITEM_SEPARATOR) {
        const char *type_value = "separator";
        add_dict_entry(props, "type", DBUS_TYPE_STRING, "s", &type_value);
        return;
    }

    if (item->label)
        add_dict_entry(props, "label", DBUS_TYPE_STRING, "s", &item->label);

    add_dict_entry(props, "enabled", DBUS_TYPE_BOOLEAN, "b", &item->enabled);

    visible = TRUE;
    add_dict_entry(props, "visible", DBUS_TYPE_BOOLEAN, "b", &visible);

    /* add icon-name if present */
    if (item->icon_name) {
        add_dict_entry(
            props, "icon-name", DBUS_TYPE_STRING, "s", &item->icon_name
        );
    }

    /* add children-display property for items with submenus */
    if (item->submenu) {
        const char *children_display = "submenu";
        add_dict_entry(
            props, "children-display", DBUS_TYPE_STRING, "s", &children_display
        );
    }

    if (item->type == STRAY_MENU_ITEM_CHECK
        || item->type == STRAY_MENU_ITEM_RADIO) {
        dbus_int32_t toggle_state;

        const char *toggle_type =
            (item->type == STRAY_MENU_ITEM_CHECK) ? "checkmark" : "radio";

        add_dict_entry(
            props, "toggle-type", DBUS_TYPE_STRING, "s", &toggle_type
        );

        toggle_state = item->checked ? 1 : 0;

        add_dict_entry(
            props, "toggle-state", DBUS_TYPE_INT32, "i", &toggle_state
        );
    }
}

static void emit_menu_items_updated(TrayIcon *icon, int *item_ids, int count) {
    DBusMessage *msg;
    DBusMessageIter args, updated_array, removed_array;
    int i;

    if (!icon || !icon->menu) return;

    msg = dbus_message_new_signal(
        STRAY_MENU_OBJECT_PATH, STRAY_DBUSMENU_INTERFACE,
        "ItemsPropertiesUpdated"
    );

    if (!msg) return;

    dbus_message_iter_init_append(msg, &args);
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_ARRAY, "(ia{sv})", &updated_array
    );

    for (i = 0; i < count; i++) {
        /* find items in submenus */
        TrayMenuItem *item = find_menu_item(icon->menu, item_ids[i]);

        if (item) {
            DBusMessageIter item_struct, props_array;

            dbus_message_iter_open_container(
                &updated_array, DBUS_TYPE_STRUCT, NULL, &item_struct
            );
            dbus_message_iter_append_basic(
                &item_struct, DBUS_TYPE_INT32, &item_ids[i]
            );

            dbus_message_iter_open_container(
                &item_struct, DBUS_TYPE_ARRAY, "{sv}", &props_array
            );

            add_menu_item_properties(&props_array, item);
            dbus_message_iter_close_container(&item_struct, &props_array);
            dbus_message_iter_close_container(&updated_array, &item_struct);
        }
    }

    dbus_message_iter_close_container(&args, &updated_array);
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_ARRAY, "(ias)", &removed_array
    );

    dbus_message_iter_close_container(&args, &removed_array);
    dbus_connection_send(icon->conn, msg, NULL);
    dbus_connection_flush(icon->conn);
    dbus_message_unref(msg);
}

static void handle_property_get_all(
    DBusConnection *conn, DBusMessage *msg, TrayIcon *icon
) {
    DBusMessageIter args, array, dict_entry, variant;
    const char *prop_menu;
    const char *current_icon, *current_title, *menu_path, *id_str;
    const char *category_str;
    const char *status_str;
    const char *empty_str;
    const char *prop_pixmap;
    const char *prop_tooltip;
    dbus_bool_t item_is_menu;
    dbus_uint32_t window_id;

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) return;

    get_icon_properties(
        icon, &current_icon, &current_title, &menu_path, &item_is_menu, &id_str,
        &status_str, &window_id
    );

    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &array);

    category_str = "ApplicationStatus";
    empty_str = "";

    /* add standard properties */
    add_dict_entry(&array, "Category", DBUS_TYPE_STRING, "s", &category_str);
    add_dict_entry(&array, "Id", DBUS_TYPE_STRING, "s", &id_str);
    add_dict_entry(&array, "Title", DBUS_TYPE_STRING, "s", &current_title);
    add_dict_entry(&array, "Status", DBUS_TYPE_STRING, "s", &status_str);
    add_dict_entry(&array, "IconName", DBUS_TYPE_STRING, "s", &current_icon);
    add_dict_entry(&array, "IconThemePath", DBUS_TYPE_STRING, "s", &empty_str);
    add_dict_entry(&array, "WindowId", DBUS_TYPE_UINT32, "u", &icon->window_id);

    /* add IconPixmap property */
    prop_pixmap = "IconPixmap";
    dbus_message_iter_open_container(
        &array, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry
    );

    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &prop_pixmap);
    dbus_message_iter_open_container(
        &dict_entry, DBUS_TYPE_VARIANT, "a(iiay)", &variant
    );

    add_pixmap_array(&variant, icon);
    dbus_message_iter_close_container(&dict_entry, &variant);
    dbus_message_iter_close_container(&array, &dict_entry);

    /* add Menu property */
    prop_menu = "Menu";
    dbus_message_iter_open_container(
        &array, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry
    );

    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &prop_menu);
    dbus_message_iter_open_container(
        &dict_entry, DBUS_TYPE_VARIANT, "o", &variant
    );

    dbus_message_iter_append_basic(&variant, DBUS_TYPE_OBJECT_PATH, &menu_path);
    dbus_message_iter_close_container(&dict_entry, &variant);
    dbus_message_iter_close_container(&array, &dict_entry);

    /* add ItemIsMenu property */
    add_dict_entry(&array, "ItemIsMenu", DBUS_TYPE_BOOLEAN, "b", &item_is_menu);

    /* add ToolTip property */
    prop_tooltip = "ToolTip";
    dbus_message_iter_open_container(
        &array, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry
    );

    dbus_message_iter_append_basic(
        &dict_entry, DBUS_TYPE_STRING, &prop_tooltip
    );
    dbus_message_iter_open_container(
        &dict_entry, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &variant
    );

    add_tooltip_struct(&variant, icon);
    dbus_message_iter_close_container(&dict_entry, &variant);
    dbus_message_iter_close_container(&array, &dict_entry);
    dbus_message_iter_close_container(&args, &array);
    dbus_connection_send(conn, reply, NULL);
    dbus_connection_flush(conn);
    dbus_message_unref(reply);
}

static void handle_property_get(
    DBusConnection *conn, DBusMessage *msg, TrayIcon *icon, const char *prop
) {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter args;
    const char *current_icon, *current_title, *menu_path, *id_str;
    const char *category_str;
    const char *status_str;
    const char *theme_path;
    dbus_bool_t item_is_menu;
    dbus_uint32_t window_id;

    if (!reply) return;

    get_icon_properties(
        icon, &current_icon, &current_title, &menu_path, &item_is_menu, &id_str,
        &status_str, &window_id
    );

    dbus_message_iter_init_append(reply, &args);

    category_str = "ApplicationStatus";
    theme_path = "";

    if (strcmp(prop, "Category") == 0) {
        add_variant(&args, DBUS_TYPE_STRING, "s", &category_str);
    } else if (strcmp(prop, "Id") == 0) {
        add_variant(&args, DBUS_TYPE_STRING, "s", &id_str);
    } else if (strcmp(prop, "Title") == 0) {
        add_variant(&args, DBUS_TYPE_STRING, "s", &current_title);
    } else if (strcmp(prop, "Status") == 0) {
        add_variant(&args, DBUS_TYPE_STRING, "s", &status_str);
    } else if (strcmp(prop, "IconName") == 0) {
        add_variant(&args, DBUS_TYPE_STRING, "s", &current_icon);
    } else if (strcmp(prop, "IconThemePath") == 0) {
        add_variant(&args, DBUS_TYPE_STRING, "s", &theme_path);
    } else if (strcmp(prop, "IconPixmap") == 0) {
        DBusMessageIter variant;
        dbus_message_iter_open_container(
            &args, DBUS_TYPE_VARIANT, "a(iiay)", &variant
        );

        /* pass the icon to get pixmap data */
        add_pixmap_array(&variant, icon);
        dbus_message_iter_close_container(&args, &variant);
    } else if (strcmp(prop, "Menu") == 0) {
        DBusMessageIter variant;
        dbus_message_iter_open_container(
            &args, DBUS_TYPE_VARIANT, "o", &variant
        );

        dbus_message_iter_append_basic(
            &variant, DBUS_TYPE_OBJECT_PATH, &menu_path
        );

        dbus_message_iter_close_container(&args, &variant);
    } else if (strcmp(prop, "ItemIsMenu") == 0) {
        add_variant(&args, DBUS_TYPE_BOOLEAN, "b", &item_is_menu);
    } else if (strcmp(prop, "ToolTip") == 0) {
        DBusMessageIter variant;
        dbus_message_iter_open_container(
            &args, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &variant
        );

        add_tooltip_struct(&variant, icon);
        dbus_message_iter_close_container(&args, &variant);
    } else if (strcmp(prop, "WindowId") == 0) {
        add_variant(&args, DBUS_TYPE_UINT32, "u", &icon->window_id);
    } else {
        DBusMessage *error = dbus_message_new_error(
            msg, "org.freedesktop.DBus.Error.InvalidArgs", "Property not found"
        );

        dbus_connection_send(conn, error, NULL);
        dbus_message_unref(error);
        dbus_message_unref(reply);

        return;
    }

    dbus_connection_send(conn, reply, NULL);
    dbus_connection_flush(conn);
    dbus_message_unref(reply);
}

static void
add_menu_items_recursive(DBusMessageIter *parent_children, TrayMenu *menu) {
    int i;

    if (!menu) return;

    for (i = 0; i < menu->item_count; i++) {
        DBusMessageIter child_variant, child_struct, child_props,
            child_children;
        TrayMenuItem *item = menu->items[i];
        if (!item) continue;

        dbus_message_iter_open_container(
            parent_children, DBUS_TYPE_VARIANT, "(ia{sv}av)", &child_variant
        );
        dbus_message_iter_open_container(
            &child_variant, DBUS_TYPE_STRUCT, NULL, &child_struct
        );
        dbus_message_iter_append_basic(
            &child_struct, DBUS_TYPE_INT32, &item->id
        );
        dbus_message_iter_open_container(
            &child_struct, DBUS_TYPE_ARRAY, "{sv}", &child_props
        );

        add_menu_item_properties(&child_props, item);

        dbus_message_iter_close_container(&child_struct, &child_props);
        dbus_message_iter_open_container(
            &child_struct, DBUS_TYPE_ARRAY, "v", &child_children
        );

        /* recursively add submenu items */
        if (item->submenu) {
            add_menu_items_recursive(&child_children, item->submenu);
        }

        dbus_message_iter_close_container(&child_struct, &child_children);
        dbus_message_iter_close_container(&child_variant, &child_struct);
        dbus_message_iter_close_container(parent_children, &child_variant);
    }
}

static int watcher_exists(DBusConnection *conn) {
    DBusError err;
    int exists;

    dbus_error_init(&err);
    exists = dbus_bus_name_has_owner(conn, STRAY_WATCHER_SERVICE, &err);

    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return 0;
    }

    return exists;
}

static int
register_with_watcher(DBusConnection *conn, const char *service_name) {
    int success;
    DBusError err;
    DBusMessage *reply;
    DBusMessage *msg = dbus_message_new_method_call(
        STRAY_WATCHER_SERVICE, STRAY_WATCHER_PATH, STRAY_WATCHER_SERVICE,
        "RegisterStatusNotifierItem"
    );

    if (!msg) return 0;

    dbus_message_append_args(
        msg, DBUS_TYPE_STRING, &service_name, DBUS_TYPE_INVALID
    );

    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "D-Bus: %s\n", err.message);
        dbus_error_free(&err);
        return 0;
    }

    if (!reply) return 0;

    success = (dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_ERROR);
    if (!success) {
        fprintf(
            stderr, "RegisterStatusNotifierItem failed: %s\n",
            dbus_message_get_error_name(reply)
        );
    }

    dbus_message_unref(reply);
    return success;
}

static DBusHandlerResult
handle_menu_get_layout(DBusConnection *conn, DBusMessage *msg, TrayIcon *icon) {
    DBusMessageIter args, root_struct, root_props, root_children;
    DBusMessage *reply;
    dbus_int32_t parent_id;
    dbus_int32_t recursion_depth;
    dbus_uint32_t revision;
    DBusMessageIter iter;
    const char *prop_value;
    TrayMenu *target_menu;

    if (!icon || !icon->menu) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* read the parent_id parameter */
    if (!dbus_message_iter_init(msg, &iter))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_iter_get_basic(&iter, &parent_id);
    dbus_message_iter_next(&iter);
    dbus_message_iter_get_basic(&iter, &recursion_depth);
    dbus_message_iter_next(&iter);

    /* determine which menu to show based on parent_id */
    if (parent_id == 0) {
        target_menu = icon->menu;
    } else {
        TrayMenuItem *parent_item = find_menu_item(icon->menu, parent_id);

        if (!parent_item || !parent_item->submenu) {
            reply = dbus_message_new_method_return(msg);
            if (!reply) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

            revision = icon->menu->revision;

            dbus_message_iter_init_append(reply, &args);
            dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &revision);
            dbus_message_iter_open_container(
                &args, DBUS_TYPE_STRUCT, NULL, &root_struct
            );

            dbus_message_iter_append_basic(
                &root_struct, DBUS_TYPE_INT32, &parent_id
            );

            dbus_message_iter_open_container(
                &root_struct, DBUS_TYPE_ARRAY, "{sv}", &root_props
            );

            dbus_message_iter_close_container(&root_struct, &root_props);
            dbus_message_iter_open_container(
                &root_struct, DBUS_TYPE_ARRAY, "v", &root_children
            );

            dbus_message_iter_close_container(&root_struct, &root_children);
            dbus_message_iter_close_container(&args, &root_struct);
            dbus_connection_send(conn, reply, NULL);
            dbus_connection_flush(conn);
            dbus_message_unref(reply);

            return DBUS_HANDLER_RESULT_HANDLED;
        }

        target_menu = parent_item->submenu;
    }

    reply = dbus_message_new_method_return(msg);
    if (!reply) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    revision = target_menu->revision;

    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &revision);
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_STRUCT, NULL, &root_struct
    );

    dbus_message_iter_append_basic(&root_struct, DBUS_TYPE_INT32, &parent_id);
    dbus_message_iter_open_container(
        &root_struct, DBUS_TYPE_ARRAY, "{sv}", &root_props
    );

    prop_value = "submenu";
    add_dict_entry(
        &root_props, "children-display", DBUS_TYPE_STRING, "s", &prop_value
    );

    dbus_message_iter_close_container(&root_struct, &root_props);
    dbus_message_iter_open_container(
        &root_struct, DBUS_TYPE_ARRAY, "v", &root_children
    );

    add_menu_items_recursive(&root_children, target_menu);

    dbus_message_iter_close_container(&root_struct, &root_children);
    dbus_message_iter_close_container(&args, &root_struct);
    dbus_connection_send(conn, reply, NULL);
    dbus_connection_flush(conn);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_menu_event(DBusConnection *conn, DBusMessage *msg, TrayIcon *icon) {
    dbus_int32_t id;
    const char *type;
    DBusMessageIter iter;
    DBusMessage *reply;

    if (!icon || !icon->menu) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_iter_init(msg, &iter))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_iter_get_basic(&iter, &id);
    dbus_message_iter_next(&iter);
    dbus_message_iter_get_basic(&iter, &type);
    /* TODO */
    dbus_message_iter_next(&iter); /* skip data (v) */
    dbus_message_iter_next(&iter); /* skip timestamp (u) */

    if (strcmp(type, "clicked") == 0) {
        TrayMenuItem *item = find_menu_item(icon->menu, id);
        if (item && item->callback) { item->callback(id, item->user_data); }
    }

    reply = dbus_message_new_method_return(msg);

    if (reply) {
        dbus_connection_send(conn, reply, NULL);
        dbus_connection_flush(conn);
        dbus_message_unref(reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_menu_get_group_properties(
    DBusConnection *conn, DBusMessage *msg, TrayIcon *icon
) {
    DBusMessageIter args, props_array, iter, id_array_iter;
    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!icon || !icon->menu) {
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_ARRAY, "(ia{sv})", &props_array
    );

    if (dbus_message_iter_init(msg, &iter)
        && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
        dbus_message_iter_recurse(&iter, &id_array_iter);

        while (dbus_message_iter_get_arg_type(&id_array_iter) == DBUS_TYPE_INT32
        ) {
            dbus_int32_t id;
            DBusMessageIter tuple, item_props;

            dbus_message_iter_get_basic(&id_array_iter, &id);
            dbus_message_iter_open_container(
                &props_array, DBUS_TYPE_STRUCT, NULL, &tuple
            );

            dbus_message_iter_append_basic(&tuple, DBUS_TYPE_INT32, &id);
            dbus_message_iter_open_container(
                &tuple, DBUS_TYPE_ARRAY, "{sv}", &item_props
            );

            if (id != 0) {
                TrayMenuItem *item = find_menu_item(icon->menu, id);
                if (item) add_menu_item_properties(&item_props, item);
            }

            dbus_message_iter_close_container(&tuple, &item_props);
            dbus_message_iter_close_container(&props_array, &tuple);

            dbus_message_iter_next(&id_array_iter);
        }

        dbus_message_iter_next(&iter);
    }

    dbus_message_iter_close_container(&args, &props_array);
    dbus_connection_send(conn, reply, NULL);
    dbus_connection_flush(conn);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
menu_message_handler(DBusConnection *conn, DBusMessage *msg, void *data) {
    const char *interface;
    const char *member;
    TrayIcon *icon = (TrayIcon *)data;

    if (!icon) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    interface = dbus_message_get_interface(msg);
    member = dbus_message_get_member(msg);

    if (interface && strcmp(interface, "org.freedesktop.DBus.Properties") == 0) {
        if (strcmp(member, "GetAll") == 0) {
            DBusMessage *reply = dbus_message_new_method_return(msg);
            if (reply) {
                DBusMessageIter args, empty_array;
                dbus_message_iter_init_append(reply, &args);
                dbus_message_iter_open_container(
                    &args, DBUS_TYPE_ARRAY, "{sv}", &empty_array
                );

                dbus_message_iter_close_container(&args, &empty_array);
                dbus_connection_send(conn, reply, NULL);
                dbus_connection_flush(conn);
                dbus_message_unref(reply);
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (strcmp(member, "Get") == 0) {
            DBusMessage *error = dbus_message_new_error(
                msg, "org.freedesktop.DBus.Error.UnknownProperty",
                "No properties on this interface"
            );

            if (error) {
                dbus_connection_send(conn, error, NULL);
                dbus_connection_flush(conn);
                dbus_message_unref(error);
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    if (!interface || strcmp(interface, STRAY_DBUSMENU_INTERFACE) != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp(member, "GetLayout") == 0)
        return handle_menu_get_layout(conn, msg, icon);

    if (strcmp(member, "Event") == 0) return handle_menu_event(conn, msg, icon);

    if (strcmp(member, "AboutToShow") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);

        if (reply) {
            dbus_bool_t need_update = TRUE;
            DBusMessageIter args;

            dbus_message_iter_init_append(reply, &args);
            dbus_message_iter_append_basic(
                &args, DBUS_TYPE_BOOLEAN, &need_update
            );

            dbus_connection_send(conn, reply, NULL);
            dbus_connection_flush(conn);
            dbus_message_unref(reply);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(member, "AboutToShowGroup") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(msg);

        if (reply) {
            DBusMessageIter args, updates_array, errors_array;

            dbus_message_iter_init_append(reply, &args);
            dbus_message_iter_open_container(
                &args, DBUS_TYPE_ARRAY, "i", &updates_array
            );

            dbus_message_iter_close_container(&args, &updates_array);
            dbus_message_iter_open_container(
                &args, DBUS_TYPE_ARRAY, "i", &errors_array
            );

            dbus_message_iter_close_container(&args, &errors_array);
            dbus_connection_send(conn, reply, NULL);
            dbus_connection_flush(conn);
            dbus_message_unref(reply);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(member, "GetGroupProperties") == 0)
        return handle_menu_get_group_properties(conn, msg, icon);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
message_handler(DBusConnection *conn, DBusMessage *msg, void *data) {
    const char *interface;
    const char *member;
    TrayIcon *icon = (TrayIcon *)data;

    if (!icon) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    interface = dbus_message_get_interface(msg);
    member = dbus_message_get_member(msg);

    if (!interface || !member) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* handle property requests */
    if (strcmp(interface, "org.freedesktop.DBus.Properties") == 0) {
        if (strcmp(member, "GetAll") == 0) {
            handle_property_get_all(conn, msg, icon);
            return DBUS_HANDLER_RESULT_HANDLED;
        } else if (strcmp(member, "Get") == 0) {
            const char *iface, *prop;
            dbus_message_get_args(
                msg, NULL, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop,
                DBUS_TYPE_INVALID
            );

            handle_property_get(conn, msg, icon, prop);
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    /* handle StatusNotifierItem interface methods */
    if (strcmp(interface, STRAY_INTERFACE_NAME) == 0) {
        DBusMessage *reply = NULL;

        if (strcmp(member, "Activate") == 0
            || strcmp(member, "SecondaryActivate") == 0
            || strcmp(member, "ContextMenu") == 0) {

            dbus_int32_t x = 0, y = 0;
            DBusMessageIter iter;

            if (dbus_message_iter_init(msg, &iter)) {
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&iter, &x);
                    dbus_message_iter_next(&iter);
                }

                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&iter, &y);
                }
            }

            if (strcmp(member, "Activate") == 0) {
                /* left click */
                if (icon->button_callback) {
                    icon->button_callback(
                        STRAY_BUTTON_LEFT, (int)x, (int)y,
                        icon->button_user_data
                    );
                } else if (icon->click_callback)
                    icon->click_callback((int)x, (int)y, icon->click_user_data);

            } else if (strcmp(member, "SecondaryActivate") == 0) {
                /* middle click */
                if (icon->button_callback)
                    icon->button_callback(
                        STRAY_BUTTON_MIDDLE, (int)x, (int)y,
                        icon->button_user_data
                    );

            } else {
                /* right click (typically handled by the menu system) */
                if (icon->button_callback)
                    icon->button_callback(
                        STRAY_BUTTON_RIGHT, (int)x, (int)y,
                        icon->button_user_data
                    );
            }

            reply = dbus_message_new_method_return(msg);
        } else if (strcmp(member, "Scroll") == 0) {
            /* scroll event */
            DBusMessageIter iter;
            dbus_int32_t delta = 0;
            const char *orientation = NULL;

            if (dbus_message_iter_init(msg, &iter)
                && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {

                dbus_message_iter_get_basic(&iter, &delta);
                dbus_message_iter_next(&iter);

                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
                    dbus_message_iter_get_basic(&iter, &orientation);
                }
            }

            if (icon->scroll_callback && orientation != NULL) {
                TrayScrollDirection direction;
                dbus_int32_t abs_delta = delta < 0 ? -delta : delta;

                if (strcmp(orientation, "vertical") == 0) {
                    direction =
                        (delta > 0) ? STRAY_SCROLL_UP : STRAY_SCROLL_DOWN;
                } else {
                    direction =
                        (delta > 0) ? STRAY_SCROLL_RIGHT : STRAY_SCROLL_LEFT;
                }

                icon->scroll_callback(
                    direction, (int)abs_delta, icon->scroll_user_data
                );
            }

            reply = dbus_message_new_method_return(msg);
        } else if (strcmp(member, "NewIcon") == 0) {
            reply = dbus_message_new_method_return(msg);
        }

        if (reply) {
            dbus_connection_send(conn, reply, NULL);
            dbus_connection_flush(conn);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void process_events_with_timeout(DBusConnection *conn, int timeout_ms) {
    struct timespec start_time, current_time;
    long elapsed_ms;
    int remaining_ms;

    if (!conn) return;

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (1) {
        DBusDispatchStatus status;

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000
                   + (current_time.tv_nsec - start_time.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms) break;

        remaining_ms = timeout_ms - (int)elapsed_ms;
        dbus_connection_read_write(conn, remaining_ms);

        do {
            status = dbus_connection_dispatch(conn);
        } while (status == DBUS_DISPATCH_DATA_REMAINS);

        if (status == DBUS_DISPATCH_NEED_MEMORY) break;
    }
}

static DBusHandlerResult
connection_filter(DBusConnection *conn, DBusMessage *msg, void *data) {
    TrayIcon *icon = (TrayIcon *)data;
    const char *interface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);

    if (!interface || !member) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp(interface, "org.freedesktop.DBus") == 0
        && strcmp(member, "NameOwnerChanged") == 0) {
        const char *name = NULL, *old_owner = NULL, *new_owner = NULL;

        dbus_message_get_args(
            msg, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old_owner,
            DBUS_TYPE_STRING, &new_owner, DBUS_TYPE_INVALID
        );

        if (name && strcmp(name, STRAY_WATCHER_SERVICE) == 0 && new_owner
            && strlen(new_owner) > 0) {
            if (register_with_watcher(icon->conn, icon->service_name))
                icon->registered = 1;

            emit_properties_changed(icon, "All");
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int menu_ensure_capacity(TrayMenu *menu) {
    if (menu->item_count >= menu->item_capacity) {
        int i;
        int new_capacity = menu->item_capacity * 2;
        TrayMenuItem **new_items =
            realloc(menu->items, new_capacity * sizeof(TrayMenuItem *));

        if (!new_items) return 0;

        for (i = menu->item_capacity; i < new_capacity; i++) {
            new_items[i] = NULL;
        }

        menu->items = new_items;
        menu->item_capacity = new_capacity;
    }

    return 1;
}

static TrayMenuItem *create_menu_item(
    TrayMenu *menu, const char *label, TrayMenuItemType type,
    TrayMenuCallback callback, void *user_data
) {
    TrayMenuItem *item;

    if (!menu_ensure_capacity(menu)) return NULL;

    item = calloc(1, sizeof(TrayMenuItem));

    if (!item) return NULL;

    if (label) {
        item->label = safe_strdup(label);

        if (!item->label) {
            free(item);
            return NULL;
        }
    } else {
        item->label = NULL;
    }

    item->id = (*menu->next_id_ptr)++;
    item->type = type;
    item->enabled = TRUE;
    item->checked = FALSE;
    item->callback = callback;
    item->user_data = user_data;
    item->submenu = NULL;
    item->icon_name = NULL;

    menu->items[menu->item_count] = item;
    menu->item_count++;

    return item;
}

static void stray_menu_destroy(TrayMenu *menu) {
    int i;

    if (!menu) return;

    for (i = 0; i < menu->item_count; i++) {
        if (menu->items[i]) {
            if (menu->items[i]->submenu) {
                stray_menu_destroy(menu->items[i]->submenu);
            }

            free(menu->items[i]->label);
            free(menu->items[i]->icon_name);
            free(menu->items[i]);
        }
    }

    free(menu->items);
    free(menu);
}

int stray_get_fd(TrayIcon *icon) {
    int fd = -1;
    if (!icon || !icon->conn) return -1;
    if (!dbus_connection_get_unix_fd(icon->conn, &fd)) return -1;
    return fd;
}

TrayIcon *
stray_create(const char *app_name, const char *icon_name, const char *title) {
    char service_name[256];
    TrayIcon *icon;
    DBusConnection *conn;
    DBusObjectPathVTable vtable;
    DBusObjectPathVTable menu_vtable;
    DBusError err;
    int instance_id;
    int ret;

    if (!app_name) {
        fprintf(stderr, "Error: app_name cannot be NULL!\n");
        return NULL;
    }

    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);

    if (dbus_error_is_set(&err)) {
        fprintf(
            stderr, "Error: failed to get DBus connection: %s\n", err.message
        );

        dbus_error_free(&err);
        return NULL;
    }

    instance_id = ++stray_instance_counter;

    snprintf(
        service_name, sizeof(service_name), "org.kde.StatusNotifierItem-%d-%d",
        getpid(), instance_id
    );

    ret = dbus_bus_request_name(
        conn, service_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err
    );

    if (dbus_error_is_set(&err)) {
        fprintf(
            stderr, "Error: failed to request DBus name: %s\n", err.message
        );

        dbus_error_free(&err);
        dbus_connection_unref(conn);
        return NULL;
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "Error: failed to become primary owner of DBus name\n");
        dbus_connection_unref(conn);
        return NULL;
    }

    icon = calloc(1, sizeof(TrayIcon));

    if (!icon) {
        dbus_connection_unref(conn);
        return NULL;
    }

    /* set initial values */
    icon->conn = conn;
    icon->app_id = safe_strdup(app_name);
    icon->service_name = safe_strdup(service_name);
    icon->status = STRAY_STATUS_ACTIVE;
    icon->window_id = 0;
    icon->icon_name = safe_strdup(icon_name ? icon_name : STRAY_DEFAULT_ICON);
    icon->title = safe_strdup(title ? title : app_name);
    icon->tooltip_title = NULL;
    icon->tooltip_text = NULL;
    icon->menu = NULL;
    icon->click_callback = NULL;
    icon->button_callback = NULL;
    icon->scroll_callback = NULL;
    icon->click_user_data = NULL;
    icon->button_user_data = NULL;
    icon->scroll_user_data = NULL;
    icon->icon_pixmaps = NULL;
    icon->icon_pixmap_count = 0;

    if (!icon->app_id || !icon->service_name || !icon->icon_name
        || !icon->title) {
        fprintf(stderr, "Error: OOM during the icon creation\n");
        stray_destroy(icon);
        return NULL;
    }

    vtable.unregister_function = NULL;
    vtable.message_function = message_handler;

    if (!dbus_connection_register_object_path(
            conn, STRAY_OBJECT_PATH, &vtable, icon
        )) {
        fprintf(stderr, "Failed to register main object path\n");
        stray_destroy(icon);
        return NULL;
    }

    menu_vtable.unregister_function = NULL,
    menu_vtable.message_function = menu_message_handler;

    if (!dbus_connection_register_object_path(
            conn, STRAY_MENU_OBJECT_PATH, &menu_vtable, icon
        )) {
        fprintf(stderr, "Failed to register menu object path\n");
        stray_destroy(icon);
        return NULL;
    }

    /* process any final events */
    process_events_with_timeout(conn, 100);
    return icon;
}

int stray_register(TrayIcon *icon) {
    DBusError err;

    if (!icon) return 0;
    if (icon->registered) return 1;

    dbus_error_init(&err);
    dbus_bus_add_match(
        icon->conn,
        "type='signal',"
        "interface='org.freedesktop.DBus',"
        "member='NameOwnerChanged',"
        "arg0='" STRAY_WATCHER_SERVICE "'",
        &err
    );

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "D-Bus match error: %s\n", err.message);
        dbus_error_free(&err);
        return 0;
    }

    icon->match_added = 1;

    if (!dbus_connection_add_filter(
            icon->conn, connection_filter, icon, NULL
        )) {
        dbus_bus_remove_match(
            icon->conn,
            "type='signal',"
            "interface='org.freedesktop.DBus',"
            "member='NameOwnerChanged',"
            "arg0='" STRAY_WATCHER_SERVICE "'",
            NULL
        );
        icon->match_added = 0;
        return 0;
    }

    icon->filter_added = 1;

    if (!watcher_exists(icon->conn)) return 0;

    emit_properties_changed(icon, "All");
    process_events_with_timeout(icon->conn, 100);

    if (!register_with_watcher(icon->conn, icon->service_name)) { return 0; }

    process_events_with_timeout(icon->conn, 100);
    icon->registered = 1;
    return 1;
}

void stray_set_click_callback(
    TrayIcon *icon, TrayClickCallback callback, void *user_data
) {
    if (icon) {
        icon->click_callback = callback;
        icon->click_user_data = user_data;
    }
}

void stray_set_button_callback(
    TrayIcon *icon, TrayButtonCallback callback, void *user_data
) {
    if (icon) {
        icon->button_callback = callback;
        icon->button_user_data = user_data;
    }
}

void stray_set_scroll_callback(
    TrayIcon *icon, TrayScrollCallback callback, void *user_data
) {
    if (icon) {
        icon->scroll_callback = callback;
        icon->scroll_user_data = user_data;
    }
}

void stray_process_events(TrayIcon *icon) {
    DBusDispatchStatus status;

    if (!icon) return;

    dbus_connection_read_write(icon->conn, 0);

    do {
        status = dbus_connection_dispatch(icon->conn);
    } while (status == DBUS_DISPATCH_DATA_REMAINS);
}

void stray_set_icon(TrayIcon *icon, const char *icon_name) {
    if (!icon) return;

    safe_free(&icon->icon_name);
    icon->icon_name = safe_strdup(icon_name ? icon_name : STRAY_DEFAULT_ICON);
    if (!icon->icon_name) return;

    stray_free_icon_pixmap(icon);
    emit_signal(icon, "NewIcon");
    emit_properties_changed(icon, "IconName");
}

void stray_set_title(TrayIcon *icon, const char *title) {
    if (!icon) return;

    safe_free(&icon->title);
    icon->title = safe_strdup(title ? title : STRAY_DEFAULT_TITLE);
    if (!icon->title) return;

    emit_signal(icon, "NewTitle");
    emit_properties_changed(icon, "Title");
}

void stray_set_tooltip(TrayIcon *icon, const char *title, const char *text) {
    if (!icon) return;

    /* Free existing strings and duplicate new ones */
    safe_free(&icon->tooltip_title);
    safe_free(&icon->tooltip_text);

    icon->tooltip_title = safe_strdup(title ? title : "");
    icon->tooltip_text = safe_strdup(text ? text : "");

    emit_signal(icon, "NewToolTip");
    emit_properties_changed(icon, "ToolTip");
}

void stray_set_window_id(TrayIcon *icon, dbus_uint32_t window_id) {
    if (!icon) return;
    icon->window_id = window_id;
    emit_properties_changed(icon, "WindowId");
}

void stray_set_icon_pixmap(
    TrayIcon *icon, int width, int height, const uint32_t *data
) {
    size_t pixel_count = 0;
    size_t data_size = 0;

    if (!icon) return;

    if (data && width > 0 && height > 0) {
        pixel_count = (size_t)width * (size_t)height;
        data_size = pixel_count * sizeof(uint32_t);

        if (data_size >= DBUS_MAXIMUM_MESSAGE_LENGTH - 4096) {
            fprintf(
                stderr,
                "Error: pixmap (%zu bytes) is "
                "too big for a D-Bus message\n",
                data_size
            );

            return;
        }

        StrayPixmap *pixmap = malloc(sizeof(StrayPixmap));
        if (!pixmap) return;

        pixmap->width = width;
        pixmap->height = height;
        pixmap->data = malloc(data_size);

        if (!pixmap->data) {
            free(pixmap);
            return;
        }

        /* convert to network byte order */
        size_t i;
        for (i = 0; i < pixel_count; i++) pixmap->data[i] = htonl(data[i]);

        stray_free_icon_pixmap(icon);
        icon->icon_pixmaps = pixmap;
        icon->icon_pixmap_count = 1;
    } else {
        stray_free_icon_pixmap(icon);
    }

    /* hosts prefer IconName over IconPixmap when both are set */
    safe_free(&icon->icon_name);
    icon->icon_name = safe_strdup("");

    emit_signal(icon, "NewIcon");
    emit_properties_changed(icon, "IconPixmap");
}

TrayMenu *stray_menu_create(void) {
    TrayMenu *menu = calloc(1, sizeof(TrayMenu));

    if (!menu) return NULL;

    menu->items = calloc(8, sizeof(TrayMenuItem *));
    if (!menu->items) {
        free(menu);
        return NULL;
    }

    menu->item_capacity = 8;
    menu->revision = 1;
    menu->next_id_ptr = &stray_global_menu_next_id;

    return menu;
}

void stray_set_status(TrayIcon *icon, TrayStatus status) {
    if (!icon) return;

    if (icon->status != status) {
        icon->status = status;

        const char *status_str = status_to_string(status);

        emit_signal_string(icon, "NewStatus", status_str);
        emit_properties_changed(icon, "Status");
    }
}

void signal_layout_update(TrayMenu *menu) {
    TrayIcon *icon;

    if (!menu) return;

    icon = get_root_icon(menu);
    if (!icon) return;

    icon->menu->revision++;
    emit_layout_updated(icon, 0);
}

static void notify_item_changed(TrayMenu *menu, TrayIcon *icon, int item_id) {
    if (icon) {
        int ids[1];
        ids[0] = item_id;
        emit_menu_items_updated(icon, ids, 1);
    } else {
        signal_layout_update(menu);
    }
}

int stray_menu_add_item(
    TrayMenu *menu, const char *label, TrayMenuCallback callback,
    void *user_data
) {
    TrayMenuItem *item;

    if (!menu) return -1;

    item = create_menu_item(
        menu, label, STRAY_MENU_ITEM_NORMAL, callback, user_data
    );

    if (!item) return -1;

    signal_layout_update(menu);
    return item->id;
}

int stray_menu_add_separator(TrayMenu *menu) {
    TrayMenuItem *item;

    if (!menu) return -1;

    item = create_menu_item(menu, NULL, STRAY_MENU_ITEM_SEPARATOR, NULL, NULL);

    if (!item) return -1;

    signal_layout_update(menu);
    return item->id;
}

int stray_menu_add_check_item(
    TrayMenu *menu, const char *label, TrayMenuCallback callback,
    void *user_data
) {
    TrayMenuItem *item;

    if (!menu) return -1;

    item = create_menu_item(
        menu, label, STRAY_MENU_ITEM_CHECK, callback, user_data
    );
    if (!item) return -1;

    signal_layout_update(menu);
    return item->id;
}

int stray_menu_add_radio_item(
    TrayMenu *menu, const char *label, TrayMenuCallback callback,
    void *user_data
) {
    TrayMenuItem *item;

    if (!menu) return -1;

    item = create_menu_item(
        menu, label, STRAY_MENU_ITEM_RADIO, callback, user_data
    );

    if (!item) return -1;

    signal_layout_update(menu);
    return item->id;
}

int stray_menu_add_submenu(
    TrayMenu *menu, const char *label, TrayMenu *submenu
) {
    TrayMenuItem *item;

    if (!menu || !submenu) return -1;

    item = create_menu_item(menu, label, STRAY_MENU_ITEM_NORMAL, NULL, NULL);

    if (!item) return -1;

    item->submenu = submenu;
    submenu->parent = menu;
    submenu->next_id_ptr = menu->next_id_ptr;

    signal_layout_update(menu);
    return item->id;
}

TrayMenu *stray_menu_get_submenu(TrayMenu *menu, int item_id) {
    TrayMenuItem *item;

    if (!menu) return NULL;

    item = find_menu_item(menu, item_id);
    if (!item) return NULL;

    return item->submenu;
}

void stray_menu_set_item_checked(
    TrayMenu *menu, int item_id, dbus_bool_t checked
) {
    TrayMenuItem *item;
    TrayIcon *icon;

    if (!menu) return;

    icon = get_root_icon(menu);

    if (icon) {
        item = find_menu_item(icon->menu, item_id);
    } else {
        item = find_menu_item(menu, item_id);
    }

    if (item) {
        item->checked = checked;
        notify_item_changed(menu, icon, item_id);
    }
}

void stray_menu_set_item_enabled(
    TrayMenu *menu, int item_id, dbus_bool_t enabled
) {
    TrayMenuItem *item;
    TrayIcon *icon;

    if (!menu) return;

    icon = get_root_icon(menu);

    if (icon) {
        item = find_menu_item(icon->menu, item_id);
    } else {
        item = find_menu_item(menu, item_id);
    }

    if (item) {
        item->enabled = enabled;
        notify_item_changed(menu, icon, item_id);
    }
}

void stray_menu_set_item_label(TrayMenu *menu, int item_id, const char *label) {
    TrayMenuItem *item;
    TrayIcon *icon;

    if (!menu) return;

    icon = get_root_icon(menu);

    if (icon) {
        item = find_menu_item(icon->menu, item_id);
    } else {
        item = find_menu_item(menu, item_id);
    }

    if (item) {
        char *new_label;

        new_label = label ? safe_strdup(label) : NULL;

        if (label && !new_label) return;

        free(item->label);
        item->label = new_label;
        notify_item_changed(menu, icon, item_id);
    }
}

void stray_menu_set_item_icon(
    TrayMenu *menu, int item_id, const char *icon_name
) {
    TrayMenuItem *item;
    TrayIcon *icon;

    if (!menu) return;

    icon = get_root_icon(menu);

    if (icon) {
        item = find_menu_item(icon->menu, item_id);
    } else {
        item = find_menu_item(menu, item_id);
    }

    if (item) {
        char *new_icon_name = icon_name ? safe_strdup(icon_name) : NULL;

        if (icon_name && !new_icon_name) return;

        free(item->icon_name);
        item->icon_name = new_icon_name;
        notify_item_changed(menu, icon, item_id);
    }
}

void stray_set_menu(TrayIcon *icon, TrayMenu *menu) {
    if (!icon) return;
    if (icon->menu == menu) return;

    if (icon->menu) {
        icon->menu->icon = NULL;
        stray_menu_destroy(icon->menu);
        icon->menu = NULL;
    }

    icon->menu = menu;

    if (menu) menu->icon = icon;

    emit_properties_changed(icon, "All");
}

void stray_destroy(TrayIcon *icon) {
    if (!icon) return;

    if (icon->filter_added) {
        dbus_connection_remove_filter(icon->conn, connection_filter, icon);
        icon->filter_added = 0;
    }

    if (icon->match_added) {
        dbus_bus_remove_match(
            icon->conn,
            "type='signal',"
            "interface='org.freedesktop.DBus',"
            "member='NameOwnerChanged',"
            "arg0='" STRAY_WATCHER_SERVICE "'",
            NULL
        );

        icon->match_added = 0;
    }

    if (icon->registered) {
        dbus_bus_release_name(icon->conn, icon->service_name, NULL);
        dbus_connection_flush(icon->conn);
        process_events_with_timeout(icon->conn, 200);
        icon->registered = 0;
    }

    stray_free_icon_pixmap(icon);

    if (icon->menu) {
        icon->menu->icon = NULL;
        stray_menu_destroy(icon->menu);
        icon->menu = NULL;
    }

    safe_free(&icon->tooltip_title);
    safe_free(&icon->tooltip_text);

    if (icon->conn) {
        dbus_connection_unregister_object_path(icon->conn, STRAY_OBJECT_PATH);
        dbus_connection_unregister_object_path(
            icon->conn, STRAY_MENU_OBJECT_PATH
        );

        dbus_connection_unref(icon->conn);
        icon->conn = NULL;
    }

    safe_free(&icon->app_id);
    safe_free(&icon->service_name);
    safe_free(&icon->icon_name);
    safe_free(&icon->title);
    free(icon);
}

#endif /* STRAY_IMPL */

#ifdef __cplusplus
}
#endif

#endif /* STRAY_H */
