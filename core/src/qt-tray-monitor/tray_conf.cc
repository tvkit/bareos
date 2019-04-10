/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Main configuration file parser for Bareos Tray Monitor.
 * Adapted from dird_conf.c
 *
 * Note, the configuration file parser consists of three parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config  scanner in lib/parse_config.c and
 *    lib/parse_config.h. These files contain the parser code,
 *    some utility routines, and the common store routines
 *    (name, int, string).
 *
 * 3. The daemon specific file, which contains the Resource
 *    definitions as well as any specific store routines
 *    for the resource records.
 *
 * Nicolas Boichat, August MMIV
 */

#include "include/bareos.h"
#define NEED_JANSSON_NAMESPACE 1
#include "lib/output_formatter.h"
#include "tray_conf.h"

#include "lib/parse_conf.h"
#include "lib/resource_item.h"
#include "lib/tls_resource_items.h"

static const std::string default_config_filename("tray-monitor.conf");

static BareosResource* sres_head[R_LAST - R_FIRST + 1];
static BareosResource** res_head = sres_head;

static bool SaveResource(int type, ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);
/*
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
static MonitorResource res_monitor;
static DirectorResource res_dir;
static ClientResource res_client;
static StorageResource res_store;
static ConsoleFontResource res_font;

/* clang-format off */

/*
 * Monitor Resource
 *
 * name handler value code flags default_value
 */
static ResourceItem mon_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_monitor,resource_name_), 0, CFG_ITEM_REQUIRED, 0, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_monitor,description_), 0, 0, 0, NULL, NULL},
  {"Password", CFG_TYPE_MD5PASSWORD, ITEM(res_monitor,password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"RefreshInterval", CFG_TYPE_TIME, ITEM(res_monitor,RefreshInterval), 0, CFG_ITEM_DEFAULT, "60", NULL, NULL},
  {"FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_monitor,FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "10", NULL, NULL},
  {"SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_monitor,SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "10", NULL, NULL},
  {"DirConnectTimeout", CFG_TYPE_TIME, ITEM(res_monitor,DIRConnectTimeout), 0, CFG_ITEM_DEFAULT, "10", NULL, NULL},
    TLS_COMMON_CONFIG(res_monitor),
    TLS_CERT_CONFIG(res_monitor),
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}
};

/*
 * Director's that we can contact
 *
 * name handler value code flags default_value
 */
static ResourceItem dir_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_dir,resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_dir,description_), 0, 0, NULL, NULL, NULL},
  {"DirPort", CFG_TYPE_PINT32, ITEM(res_dir,DIRport), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL},
  {"Address", CFG_TYPE_STR, ITEM(res_dir,address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
    TLS_COMMON_CONFIG(res_dir),
    TLS_CERT_CONFIG(res_dir),
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}
};

/*
 * Client or File daemon resource
 *
 * name handler value code flags default_value
 */
static ResourceItem cli_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_client,resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_client,description_), 0, 0, NULL, NULL, NULL},
  {"Address", CFG_TYPE_STR, ITEM(res_client,address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"FdPort", CFG_TYPE_PINT32, ITEM(res_client,FDport), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"Password", CFG_TYPE_MD5PASSWORD, ITEM(res_client,password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
    TLS_COMMON_CONFIG(res_client),
    TLS_CERT_CONFIG(res_client),
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}
};

/*
 * Storage daemon resource
 *
 * name handler value code flags default_value
 */
static ResourceItem store_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_store,resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_store,description_), 0, 0, NULL, NULL, NULL},
  {"SdPort", CFG_TYPE_PINT32, ITEM(res_store,SDport), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL},
  {"Address", CFG_TYPE_STR, ITEM(res_store,address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"SdAddress", CFG_TYPE_STR, ITEM(res_store,address), 0, 0, NULL, NULL, NULL},
  {"Password", CFG_TYPE_MD5PASSWORD, ITEM(res_store,password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"SdPassword", CFG_TYPE_MD5PASSWORD, ITEM(res_store,password), 0, 0, NULL, NULL, NULL},
    TLS_COMMON_CONFIG(res_store),
    TLS_CERT_CONFIG(res_store),
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}
};

/*
 * Font resource
 *
 * name handler value code flags default_value
 */
static ResourceItem con_font_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_font,resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_font,description_), 0, 0, NULL, NULL, NULL},
  {"Font", CFG_TYPE_STR, ITEM(res_font,fontface), 0, 0, NULL, NULL, NULL},
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 * NOTE!!! keep it in the same order as the R_codes
 *   or eliminate all resources[rindex].name
 *
 *  name items rcode res_head
 */
static ResourceTable resources[] = {
  {"Monitor", mon_items, R_MONITOR, sizeof(MonitorResource),
      []() { return new (&res_monitor) MonitorResource(); }, &res_monitor},
  {"Director", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      []() { return new (&res_dir) DirectorResource(); }, &res_dir},
  {"Client", cli_items, R_CLIENT, sizeof(ClientResource),
      []() { return new (&res_client) ClientResource(); }, &res_client},
  {"Storage", store_items, R_STORAGE, sizeof(StorageResource),
      []() { return new (&res_store) StorageResource(); }, &res_store},
  {"ConsoleFont", con_font_items, R_CONSOLE_FONT, sizeof(ConsoleFontResource),
      []() { return new (&res_font) ConsoleFontResource(); }, &res_font},
  {NULL, NULL, 0, 0}
};

/* clang-format on */

/*
 * Dump contents of resource
 */
static void DumpResource(int type,
                         BareosResource* res,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  PoolMem buf;
  bool recurse = true;

  if (res == NULL) {
    sendit(sock, _("Warning: no \"%s\" resource (%d) defined.\n"),
           my_config->ResToStr(type), type);
    return;
  }
  if (type < 0) { /* no recursion */
    type = -type;
    recurse = false;
  }
  switch (type) {
    default:
      res->PrintConfig(buf, *my_config);
      break;
  }
  sendit(sock, "%s", buf.c_str());

  if (recurse && res->next_) {
    DumpResource(type, res->next_, sendit, sock, hide_sensitive_data, verbose);
  }
}

static void FreeResource(BareosResource* res, int type)
{
  if (res == NULL) return;

  BareosResource* next_resource = (BareosResource*)res->next_;

  if (res->resource_name_) { free(res->resource_name_); }
  if (res->description_) { free(res->description_); }

  switch (type) {
    case R_MONITOR:
      break;
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(res);
      if (p->address) { free(p->address); }
      break;
    }
    case R_CLIENT: {
      ClientResource* p = dynamic_cast<ClientResource*>(res);
      if (p->address) { free(p->address); }
      if (p->password.value) { free(p->password.value); }
      break;
    }
    case R_STORAGE: {
      StorageResource* p = dynamic_cast<StorageResource*>(res);
      if (p->address) { free(p->address); }
      if (p->password.value) { free(p->password.value); }
      break;
    }
    case R_CONSOLE_FONT: {
      ConsoleFontResource* p = dynamic_cast<ConsoleFontResource*>(res);
      if (p->fontface) { free(p->fontface); }
      break;
    }
    default:
      printf(_("Unknown resource type %d in FreeResource.\n"), type);
      break;
  }

  if (next_resource) { FreeResource(next_resource, type); }
}

/*
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers because they may not have been defined until
 * later in pass 1.
 */
static bool SaveResource(int type, ResourceItem* items, int pass)
{
  int rindex = type - R_FIRST;
  int i;
  int error = 0;

  /*
   * Ensure that all required items are present
   */
  for (i = 0; items[i].name; i++) {
    if (items[i].flags & CFG_ITEM_REQUIRED) {
      if (!BitIsSet(i, items[i].static_resource->item_present_)) {
        Emsg2(M_ERROR_TERM, 0,
              _("%s item is required in %s resource, but not found.\n"),
              items[i].name, resources[rindex].name);
      }
    }
    /* If this triggers, take a look at lib/parse_conf.h */
    if (i >= MAX_RES_ITEMS) {
      Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"),
            resources[rindex].name);
    }
  }

  /*
   * During pass 2 in each "store" routine, we looked up pointers
   * to all the resources referrenced in the current resource, now we
   * must copy their addresses from the static record to the allocated
   * record.
   */
  if (pass == 2) {
    switch (type) {
      case R_MONITOR:
      case R_CLIENT:
      case R_STORAGE:
      case R_DIRECTOR:
      case R_CONSOLE_FONT:
        // Resources not containing a resource
        break;
      default:
        Emsg1(M_ERROR, 0, _("Unknown resource type %d in SaveResource.\n"),
              type);
        error = 1;
        break;
    }

    /* resource_name_ was already deep copied during 1. pass
     * as matter of fact the remaining allocated memory is
     * redundant and would not be freed in the dynamic resources;
     *
     * currently, this is the best place to free that */

    BareosResource* res = items[0].static_resource;

    if (res) {
      if (res->resource_name_) {
        free(res->resource_name_);
        res->resource_name_ = nullptr;
      }
      if (res->description_) {
        free(res->description_);
        res->description_ = nullptr;
      }
    }
    return (error == 0);
  }

  if (!error) {
    BareosResource* new_resource;
    switch (type) {
      case R_MONITOR: {
        MonitorResource* p = new MonitorResource;
        *p = res_monitor;
        new_resource = p;
        break;
      }
      case R_CLIENT: {
        ClientResource* p = new ClientResource;
        *p = res_client;
        new_resource = p;
        break;
      }
      case R_STORAGE: {
        StorageResource* p = new StorageResource;
        *p = res_store;
        new_resource = p;
        break;
      }
      case R_DIRECTOR: {
        DirectorResource* p = new DirectorResource;
        *p = res_dir;
        new_resource = p;
        break;
      }
      case R_CONSOLE_FONT: {
        ConsoleFontResource* p = new ConsoleFontResource;
        *p = res_font;
        new_resource = p;
        break;
      }
      default:
        ASSERT(false);
        break;
    }
    error = my_config->AppendToResourcesChain(new_resource, type) ? 0 : 1;
  }
  return (error == 0);
}

static void ConfigReadyCallback(ConfigurationParser& my_config)
{
  std::map<int, std::string> map{
      {R_MONITOR, "R_MONITOR"}, {R_DIRECTOR, "R_DIRECTOR"},
      {R_CLIENT, "R_CLIENT"},   {R_STORAGE, "R_STORAGE"},
      {R_CONSOLE, "R_CONSOLE"}, {R_CONSOLE_FONT, "R_CONSOLE_FONT"}};
  my_config.InitializeQualifiedResourceNameTypeConverter(map);
}

ConfigurationParser* InitTmonConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, nullptr, nullptr, nullptr, exit_code,
      R_FIRST, R_LAST, resources, res_head, default_config_filename.c_str(),
      "tray-monitor.d", ConfigReadyCallback, SaveResource, DumpResource,
      FreeResource);
  if (config) { config->r_own_ = R_MONITOR; }
  return config;
}

/*
 * Print configuration file schema in json format
 */
#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  ResourceTable* resources = my_config->resources_;

  InitializeJson();

  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-tray-monitor"));
  json_object_set_new(json, "version", json_string(VERSION));

  /*
   * Resources
   */
  json_t* resource = json_object();
  json_object_set(json, "resource", resource);
  json_t* bareos_tray_monitor = json_object();
  json_object_set(resource, "bareos-tray-monitor", bareos_tray_monitor);

  for (int r = 0; resources[r].name; r++) {
    ResourceTable resource = my_config->resources_[r];
    json_object_set(bareos_tray_monitor, resource.name,
                    json_items(resource.items));
  }

  PmStrcat(buffer, json_dumps(json, JSON_INDENT(2)));
  json_decref(json);

  return true;
}
#else
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  PmStrcat(buffer, "{ \"success\": false, \"message\": \"not available\" }");
  return false;
}
#endif
