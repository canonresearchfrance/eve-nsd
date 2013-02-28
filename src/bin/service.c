#include <glib-object.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <string.h>
#include <EWebKit.h>

#include "service.h"

typedef enum {
   NETWORKSERVICE_TYPE_UPNP,
   NETWORKSERVICE_TYPE_ZEROCONF,
   NETWORKSERVICE_TYPE_UNKNOWN
} Eve_NetworkService_Type;

struct _Service_Item {
   char *text;
#define MAX_SUBTEXT_LEN 128
   char *sub_text;
   struct {
      SoupSession *soup_session;
      SoupMessage *soup_msg;
      char *url;
      const void *data;
      size_t size;
   } icon;
   void *widget;
   Eina_List *services;
};


static char*
service_get_sub_name(Ewk_NetworkService *service, Eve_NetworkService_Type type)
{
   if (type == NETWORKSERVICE_TYPE_UPNP) {
      const char *name = ewk_network_service_name_get(service);
      char *ptr;
      name = strstr(name, ":serviceId:");
      name+= strlen(":serviceId:");
      ptr = strchr(name, ':');
      return ptr ? strndup(name, (ptr-name)) : strdup(name);
   
   } else if (type == NETWORKSERVICE_TYPE_ZEROCONF) {
      const char *type = ewk_network_service_type_get(service);
      char *off;
      size_t len;
      type += strlen("zeroconf:_");
      off = strchr(type, '.');
      len = off - type;
      return strndup(type, len);
   }

   return NULL;
}

static char*
service_get_name(Ewk_NetworkService *service, Eve_NetworkService_Type type)
{
   if (type == NETWORKSERVICE_TYPE_ZEROCONF) {
      const char *name = ewk_network_service_name_get(service);
      return strdup(name);
   }

   return NULL;
}

static char*
service_get_hash_key(Ewk_NetworkService *service, Eve_NetworkService_Type *type)
{
    const char *type_str = ewk_network_service_type_get(service);

    if (!strncmp(type_str, "upnp:", 5)) {
        const char *srv_id = ewk_network_service_id_get(service);
        char *id;
        char *ptr;
        
        id = strdup(srv_id); 
        *type = NETWORKSERVICE_TYPE_UPNP;
        strtok_r(id, ":", &ptr);
        memmove(id, ptr, strlen(ptr));
        return strtok(id, ":");

   } else if (!strncmp(type_str, "zeroconf:", 9)) {
       const char *srv_name = ewk_network_service_name_get(service);
       char *name;

       name = strdup(srv_name);
       *type = NETWORKSERVICE_TYPE_ZEROCONF;
       return name;
   }

   return NULL;
}

static void
parse_upnp_service_config()
{
}

static xmlXPathObjectPtr 
getNodeSet(xmlDocPtr doc, xmlChar *xpath)
{
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;

	context = xmlXPathNewContext(doc);
	if (context) {
	    result = xmlXPathEvalExpression(xpath, context);
	    xmlXPathFreeContext(context);

	    if (result && xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		    xmlXPathFreeObject(result);
            return 0;
        }
	}
	return result;
}

static void 
service_item_config_get(Service_Item *item, Ewk_NetworkService *service)
{
    const char *config = ewk_network_service_config_get(service);
    const char *service_url = ewk_network_service_url_get(service);
    char *ptr;
    int base_url_len;
    xmlDocPtr doc;
    xmlChar* xpathFriendlyName = (xmlChar*) "//friendlyName";    
    xmlChar* xpathIconList = (xmlChar*) "///icon";
    xmlXPathObjectPtr friendlyNameObj;
    xmlXPathObjectPtr iconList;

    ptr = strchr(&service_url[8], '/'); /* offset "https://" */
    base_url_len = ptr - service_url;

    doc = xmlParseMemory(config, strlen(config));
    if (!doc)
        return;

    friendlyNameObj = getNodeSet(doc, xpathFriendlyName);
    if (friendlyNameObj) {
        xmlNodePtr node = friendlyNameObj->nodesetval->nodeTab[0];

        /* store friendlyName attribute */
        item->text = strdup(node->children->content);

        xmlXPathFreeObject(friendlyNameObj);
    }
    
    iconList = getNodeSet(doc, xpathIconList);
    if (iconList) {
        int i;
#define MIN_ICON_WIDTH 32
        int icon_width = 0;
        char *icon_url;
        xmlNodeSetPtr nodeSet = iconList->nodesetval;

        for (i=0; i < nodeSet->nodeNr; i++) {
            xmlNodePtr iconNode = nodeSet->nodeTab[i];
            xmlNodePtr childNode = iconNode->xmlChildrenNode; 
            xmlNodePtr next = childNode;
            char *mimetype = 0;
            char *width_str = 0;
            char *url = 0;
            int width;

            while (next) {
                char *value = (char*)xmlNodeListGetString(doc, next->xmlChildrenNode, 1);
                
                if (!strcmp((const char*)next->name, "mimetype")) {
                    if (strcmp(value, "image/png"))
                        break;
                    mimetype = strdup(value);
                } else if (!strcmp((const char*)next->name, "width"))
                    width_str = strdup(value);
                else if (!strcmp((const char*)next->name, "url"))
                    url = strdup(value);

                next = xmlNextElementSibling(next);
                xmlFree(value);
            }

            if (!mimetype)
                continue;

            width = atoi(width_str);

            if (!icon_width) {
                icon_width = width;
                icon_url = strdup(url);
            } 
            else if (( (width < icon_width) && (width >= MIN_ICON_WIDTH) ) ||
                ( (width > icon_width) && (icon_width < MIN_ICON_WIDTH) )) {
                free(icon_url);
                icon_url = strdup(url);
            }
                
            /*printf("Icon : \n"
                    "\t mimetype : %s\n"
                    "\t width    : %d\n"
                    "\t url      : %s\n",
                    mimetype, width, url);*/

            if (mimetype) free(mimetype);
            if (width) free(width_str);
            if (url) free(url);
        }
        
        /* store icon url */
        ptr = malloc(base_url_len + strlen(icon_url) + 1);
        strncpy(ptr, service_url, base_url_len);
        strcpy(&ptr[base_url_len], icon_url);
        item->icon.url = ptr;

        xmlXPathFreeObject(iconList);
    }
}

static void
soup_session_callback(SoupSession *session, SoupMessage *msg, gpointer userData)
{
    const gchar* header;    
    Service_Item *item = (Service_Item*)userData;

    if (SOUP_STATUS_IS_REDIRECTION(msg->status_code)) {
        header = soup_message_headers_get_one(msg->response_headers, "Location");
        if (header) {
            msg = soup_message_new("GET", header);
            soup_session_queue_message(session, msg, &soup_session_callback, 0);
        }
        return;
    }
    
    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
        return;
    
    if (msg->response_body->length > 0) {
        item->icon.data = msg->response_body->data;
        item->icon.size = msg->response_body->length;

        if (item->widget)
            service_item_widget_update(item->widget);
    }
}

static void 
service_item_icon_data_load(Service_Item *item)
{
    static SoupSession *session = NULL;
    SoupMessage *msg;
    
    if (!session)
        session = soup_session_async_new();

    msg = soup_message_new("GET", item->icon.url);

    item->icon.soup_session = session;
    item->icon.soup_msg = msg;
    g_object_ref(msg);

    soup_session_queue_message(session, msg, soup_session_callback, item);
}

static Service_Item *
service_item_new(char *text, char *sub_text, Ewk_NetworkService *service, Eve_NetworkService_Type type)
{
    Service_Item *item;

    item = (Service_Item*)malloc(sizeof(Service_Item));

    memset(item, 0, sizeof(Service_Item));

    switch(type) 
    {
    case NETWORKSERVICE_TYPE_UPNP:
        /* resolve UpNp xml config */
        service_item_config_get(item, service);

        /* queue an icon request */
        service_item_icon_data_load(item);

        break;

    case NETWORKSERVICE_TYPE_ZEROCONF:
        item->text = service_get_name(service, type);
        break;

    default:
        item->text = text;
    }

    if (service)
        item->services = eina_list_append(item->services, service);
    
    if (sub_text) {
        item->sub_text = (char*)malloc(MAX_SUBTEXT_LEN);
        strncpy(item->sub_text, sub_text, MAX_SUBTEXT_LEN-1);
    }

    return item;
}

void service_item_free(Service_Item *item)
{
    free(item->text);

    if (item->icon.soup_msg) {
        soup_session_cancel_message(item->icon.soup_session, item->icon.soup_msg, SOUP_STATUS_CANCELLED);
        g_object_unref(item->icon.soup_msg);
    }

    if (item->sub_text)
       free(item->sub_text);

    item->widget = NULL;
    free(item);
}

char *service_item_text_get(const Service_Item *item)
{
    return strdup(item->text);
}

char *service_item_subtext_get(const Service_Item *item)
{
    return item->sub_text ? strdup(item->sub_text) : NULL;
}

Eina_Bool service_item_have_service(Service_Item *item)
{
    return item->services ? EINA_TRUE : EINA_FALSE;
}

void service_list_set(Eina_List *service_iter, Service_Item_Callback list_append_func, void *user_data)
{
   unsigned long i, j; 
   unsigned long length;
   Ewk_NetworkServices *services;
   Eina_List *l;

   EINA_LIST_FOREACH(service_iter, l, services)
   {
      const char *origin;
      char *text;
      char *sub_text;
      Service_Item *item = NULL;
      Eina_Hash *service_items = eina_hash_string_superfast_new(NULL);

      length = ewk_network_services_length_get(services);
      origin = ewk_network_services_origin_get(services);
      text = strdup(origin);

      item = service_item_new(text, NULL, NULL, NETWORKSERVICE_TYPE_UNKNOWN);

      list_append_func(user_data, item);
      
      for (i=0; i<length; i++)
      {
         Ewk_NetworkService *srv = ewk_network_services_item_get(services, i);        
         Eina_Bool online = ewk_network_service_is_online(srv);
         Eve_NetworkService_Type type;
         char *key;

         if (online == EINA_FALSE)
             continue;

         key = service_get_hash_key(srv, &type);
         item = eina_hash_find(service_items, key);

         sub_text = service_get_sub_name(srv, type);

         if (item) {
            /* Concatenate UPnp or Zeroconf service "type" */
            if (sub_text && (strlen(item->sub_text) + strlen(sub_text)) < (MAX_SUBTEXT_LEN - 2)) { 
                strcat(item->sub_text, ", ");
                strcat(item->sub_text, sub_text);
                free(sub_text);
            }
            
            item->services = eina_list_append(item->services, srv);

         } else {
            item = service_item_new(NULL, sub_text, srv, type);
            eina_hash_add(service_items, key, item);           
            
            list_append_func(user_data, item);
         }

         free(key);
      }

      eina_hash_free(service_items);
   }
}

void service_list_update(Eina_List *service_iter, Service_Item_Callback list_append_func, void *user_data)
{

}

void service_item_allowed_set(Service_Item *item, Eina_Bool allowed)
{
   Eina_List *l;
   Ewk_NetworkService *service;

   EINA_LIST_FOREACH(item->services, l, service)
      ewk_network_service_allowed_set(service, allowed);
}

void service_item_widget_set(Service_Item *item, void *obj)
{
    item->widget = obj;
}

void service_item_icon_data_get(Service_Item *item, const void **data, size_t *size)
{
    *data = item->icon.data;
    *size = item->icon.size;
}
