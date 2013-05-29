#ifndef __SERVICE_H__
#define __SERVICE_H__

#include <Eina.h>
#include "private.h"

typedef enum {
   NETWORK_TYPE_DEVICE_UPNP,
   NETWORK_TYPE_SERVICE_UPNP,
   NETWORK_TYPE_SERVICE_ZEROCONF,
   NETWORK_TYPE_SERVICE_UNKNOWN
} Eve_Network_Type;

typedef struct _Service_Item Service_Item;

typedef void (*Service_Item_Callback)(void *data, Service_Item *item);

/* Service_Item */
void service_items_clear(Eina_List **service_items);
int service_item_compare(const void *item1, const void *item2);

Eve_Network_Type service_item_type_get(const Service_Item *item);
char *service_item_text_get(const Service_Item *item);
char *service_item_subtext_get(const Service_Item *item);
Eina_Bool service_item_have_service(const Service_Item *item);
Eina_Bool service_item_have_double_label(const Service_Item *item);
void service_item_allowed_set(Service_Item *item, Eina_Bool allowed);
Eina_Bool service_item_allowed_get(const Service_Item *item);
Eina_Bool service_item_widget_allowed_get(const Service_Item *item);
void service_item_widget_set(Service_Item *item, void *obj);
void *service_item_parent_widget_get(const Service_Item *item);
Eina_List *service_item_children_get(const Service_Item *item);
void service_item_icon_data_get(Service_Item *item, const void **data, size_t *size);

Eina_Bool service_list_set(Eina_List *service_iter, Network *network, Eina_List **service_items, Service_Item_Callback func, void *data);
void service_config_filename_set(const char *filename);
const char *service_config_filename_get();
void service_config_register_services(Service_Item *item, Eina_Bool allowed);

extern void service_item_widget_update(const void *widget); 
extern void *service_icon_from_data_get(const void *widget, const void *data, size_t size);

#endif /* __SERVICE_H__ */
