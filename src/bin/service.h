#ifndef __SERVICE_H__
#define __SERVICE_H__

#include <Eina.h>

typedef struct _Service_Item Service_Item;

typedef void (*Service_Item_Callback)(void *data, Service_Item *item);

/* Service_Item */
void service_item_free(Service_Item *item);

char *service_item_text_get(const Service_Item *item);
char *service_item_subtext_get(const Service_Item *item);
Eina_Bool service_item_have_service(Service_Item *item);
void service_item_allowed_set(Service_Item *item, Eina_Bool allowed);
void service_item_widget_set(Service_Item *item, void *obj);
void service_item_icon_data_get(Service_Item *item, const void **data, size_t *size);

void service_list_set(Eina_List *service_iter, Service_Item_Callback func, void *data);

extern void service_item_widget_update(void *data); 

#endif /* __SERVICE_H__ */
