/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/**
 * Kern Sibbald, January MM
 */
/***
 * @file
 * Main configuration file parser for BAREOS Directors,
 * some parts may be split into separate files such as
 * the schedule configuration (run_config.c).
 *
 * Note, the configuration file parser consists of three parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config  scanner in lib/parse_config.c and
 *    lib/parse_config.h.
 *
 *    These files contain the parser code, some utility
 *    routines, and the common store routines (name, int,
 *    string).
 *
 * 3. The daemon specific file, which contains the Resource
 *    definitions as well as any specific store routines
 *    for the resource records.
 *
 */

#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "dird.h"
#include "dird/inc_conf.h"
#include "dird/dird_globals.h"
#include "lib/berrno.h"
#include "lib/tls_conf.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/parse_conf.h"
#include "lib/keyword_table_s.h"
#include "lib/util.h"
#include "lib/tls_resource_items.h"

namespace directordaemon {

/**
 * Used by print_config_schema_json
 */
extern struct s_kw RunFields[];

/**
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static BareosResource* sres_head[R_LAST - R_FIRST + 1];
static BareosResource** res_head = sres_head;
static PoolMem* configure_usage_string = NULL;

extern void StoreInc(LEX* lc, ResourceItem* item, int index, int pass);
extern void StoreRun(LEX* lc, ResourceItem* item, int index, int pass);

static void CreateAndAddUserAgentConsoleResource(
    ConfigurationParser& my_config);
static bool SaveResource(int type, ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* ures,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/* the first configuration pass uses this static memory */
static DirectorResource res_dir;
static ConsoleResource res_con;
static ProfileResource res_profile;
static ClientResource res_client;
static StorageResource res_store;
static CatalogResource res_cat;
static JobResource res_job;
static FilesetResource res_fs;
static ScheduleResource res_sch;
static PoolResource res_pool;
static MessagesResource res_msgs;
static CounterResource res_counter;
static DeviceResource res_dev;


/* clang-format off */

static ResourceItem dir_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dir,  resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_dir,  description_), 0, 0, NULL, NULL, NULL },
  { "Messages", CFG_TYPE_RES, ITEM(res_dir,  messages), R_MSGS, 0, NULL, NULL, NULL },
  { "DirPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_dir,  DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
  { "DirAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir,  DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
  { "DirAddresses", CFG_TYPE_ADDRESSES, ITEM(res_dir,  DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
  { "DirSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir,  DIRsrc_addr), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "QueryFile", CFG_TYPE_DIR, ITEM(res_dir, query_file), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_dir, working_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_WORKINGDIR, NULL, NULL },
  { "PidDirectory", CFG_TYPE_DIR, ITEM(res_dir, pid_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_PIDDIR, NULL, NULL },
  { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_dir, plugin_directory), 0, 0, NULL,
     "14.2.0-", "Plugins are loaded from this directory. To load only specific plugins, use 'Plugin Names'." },
  { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_dir, plugin_names), 0, 0, NULL,
      "14.2.0-", "List of plugins, that should get loaded from 'Plugin Directory' (only basenames, '-dir.so' is added automatically). If empty, all plugins will get loaded." },
  { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_dir, scripts_directory), 0, 0, NULL, NULL, "This directive is currently unused." },
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  { "BackendDirectory", CFG_TYPE_ALIST_DIR, ITEM(res_dir, backend_directories), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_BACKENDDIR, NULL, NULL },
#endif
  { "Subscriptions", CFG_TYPE_PINT32, ITEM(res_dir, subscriptions), 0, CFG_ITEM_DEFAULT, "0", "12.4.4-", NULL },
  { "SubSysDirectory", CFG_TYPE_DIR, ITEM(res_dir, subsys_directory), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dir, MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
  { "MaximumConnections", CFG_TYPE_PINT32, ITEM(res_dir, MaxConnections), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL },
  { "MaximumConsoleConnections", CFG_TYPE_PINT32, ITEM(res_dir, MaxConsoleConnections), 0, CFG_ITEM_DEFAULT, "20", NULL, NULL },
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir, FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "180" /* 3 minutes */, NULL, NULL },
  { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir, SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir, heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "StatisticsRetention", CFG_TYPE_TIME, ITEM(res_dir, stats_retention), 0, CFG_ITEM_DEFAULT, "160704000" /* 5 years */, NULL, NULL },
  { "StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_dir, stats_collect_interval), 0, CFG_ITEM_DEFAULT, "150", "14.2.0-", NULL },
  { "VerId", CFG_TYPE_STR, ITEM(res_dir, verid), 0, 0, NULL, NULL, NULL },
  { "OptimizeForSize", CFG_TYPE_BOOL, ITEM(res_dir, optimize_for_size), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "OptimizeForSpeed", CFG_TYPE_BOOL, ITEM(res_dir, optimize_for_speed), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "OmitDefaults", CFG_TYPE_BOOL, ITEM(res_dir, omit_defaults), 0, CFG_ITEM_DEFAULT | CFG_ITEM_DEPRECATED, "true",
      NULL, "Omit config variables with default values when dumping the config." },
  { "KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, keyencrkey), 1, 0, NULL, NULL, NULL },
  { "NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_dir, ndmp_snooping), 0, 0, NULL, "13.2.0-", NULL },
  { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_dir, ndmp_loglevel), 0, CFG_ITEM_DEFAULT, "4", "13.2.0-", NULL },
  { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_dir, jcr_watchdog_time), 0, 0, NULL, "14.2.0-", NULL },
  { "Auditing", CFG_TYPE_BOOL, ITEM(res_dir, auditing), 0, CFG_ITEM_DEFAULT, "false", "14.2.0-", NULL },
  { "AuditEvents", CFG_TYPE_AUDIT, ITEM(res_dir, audit_events), 0, 0, NULL, "14.2.0-", NULL },
  { "SecureEraseCommand", CFG_TYPE_STR, ITEM(res_dir, secure_erase_cmdline), 0, 0, NULL, "15.2.1-",
     "Specify command that will be called when bareos unlinks files." },
  { "LogTimestampFormat", CFG_TYPE_STR, ITEM(res_dir, log_timestamp_format), 0, 0, NULL, "15.2.3-", NULL },
   TLS_COMMON_CONFIG(res_dir),
   TLS_CERT_CONFIG(res_dir),
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem profile_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_profile, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_profile, description_), 0, 0, NULL, NULL,
     "Additional information about the resource. Only used for UIs." },
  { "JobACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Job_ACL, 0, NULL, NULL,
     "Lists the Job resources, this resource has access to. The special keyword *all* allows access to all Job resources." },
  { "ClientACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Client_ACL, 0, NULL, NULL,
     "Lists the Client resources, this resource has access to. The special keyword *all* allows access to all Client resources." },
  { "StorageACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Storage_ACL, 0, NULL, NULL,
     "Lists the Storage resources, this resource has access to. The special keyword *all* allows access to all Storage resources." },
  { "ScheduleACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Schedule_ACL, 0, NULL, NULL,
     "Lists the Schedule resources, this resource has access to. The special keyword *all* allows access to all Schedule resources." },
  { "PoolACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Pool_ACL, 0, NULL, NULL,
     "Lists the Pool resources, this resource has access to. The special keyword *all* allows access to all Pool resources." },
  { "CommandACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Command_ACL, 0, NULL, NULL,
     "Lists the commands, this resource has access to. The special keyword *all* allows using commands." },
  { "FileSetACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), FileSet_ACL, 0, NULL, NULL,
     "Lists the File Set resources, this resource has access to. The special keyword *all* allows access to all File Set resources." },
  { "CatalogACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Catalog_ACL, 0, NULL, NULL,
     "Lists the Catalog resources, this resource has access to. The special keyword *all* allows access to all Catalog resources." },
  { "WhereACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), Where_ACL, 0, NULL, NULL,
     "Specifies the base directories, where files could be restored. An empty string allows restores to all directories." },
  { "PluginOptionsACL", CFG_TYPE_ACL, ITEM(res_profile, ACL_lists), PluginOptions_ACL, 0, NULL, NULL,
     "Specifies the allowed plugin options. An empty strings allows all Plugin Options." },
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem con_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_con, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "Description", CFG_TYPE_STR, ITEM(res_con, description_), 0, 0, NULL, NULL, NULL },
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_con, password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "JobACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Job_ACL, 0, NULL, NULL, NULL },
  { "ClientACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Client_ACL, 0, NULL, NULL, NULL },
  { "StorageACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Storage_ACL, 0, NULL, NULL, NULL },
  { "ScheduleACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Schedule_ACL, 0, NULL, NULL, NULL },
  { "RunACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Run_ACL, 0, NULL, NULL, NULL },
  { "PoolACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Pool_ACL, 0, NULL, NULL, NULL },
  { "CommandACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Command_ACL, 0, NULL, NULL, NULL },
  { "FileSetACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), FileSet_ACL, 0, NULL, NULL, NULL },
  { "CatalogACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Catalog_ACL, 0, NULL, NULL, NULL },
  { "WhereACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), Where_ACL, 0, NULL, NULL, NULL },
  { "PluginOptionsACL", CFG_TYPE_ACL, ITEM(res_con, ACL_lists), PluginOptions_ACL, 0, NULL, NULL, NULL },
  { "Profile", CFG_TYPE_ALIST_RES, ITEM(res_con, profiles), R_PROFILE, 0, NULL, "14.2.3-",
     "Profiles can be assigned to a Console. ACL are checked until either a deny ACL is found or an allow ACL. "
     "First the console ACL is checked then any profile the console is linked to." },
  { "UsePamAuthentication", CFG_TYPE_BOOL, ITEM(res_con, use_pam_authentication_), 0, CFG_ITEM_DEFAULT,
     "false", "18.2.4-", NULL },
   TLS_COMMON_CONFIG(res_con),
   TLS_CERT_CONFIG(res_con),
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem cli_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_client, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_client, description_), 0, 0, NULL, NULL, NULL },
  { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_client, Protocol), 0, CFG_ITEM_DEFAULT, "Native", "13.2.0-", NULL },
  { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_client, AuthType), 0, CFG_ITEM_DEFAULT, "None", NULL, NULL },
  { "Address", CFG_TYPE_STR, ITEM(res_client, address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "LanAddress", CFG_TYPE_STR, ITEM(res_client, lanaddress), 0, CFG_ITEM_DEFAULT, NULL, "16.2.6-",
     "Sets additional address used for connections between Client and Storage Daemon inside separate network."},
  { "FdAddress", CFG_TYPE_STR, ITEM(res_client, address), 0, CFG_ITEM_ALIAS, NULL, NULL, "Alias for Address." },
  { "Port", CFG_TYPE_PINT32, ITEM(res_client, FDport), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL },
  { "FdPort", CFG_TYPE_PINT32, ITEM(res_client, FDport), 0, CFG_ITEM_DEFAULT | CFG_ITEM_ALIAS, FD_DEFAULT_PORT, NULL, NULL },
  { "Username", CFG_TYPE_STR, ITEM(res_client, username), 0, 0, NULL, NULL, NULL },
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_client, password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "FdPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_client, password_), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
  { "Catalog", CFG_TYPE_RES, ITEM(res_client, catalog), R_CATALOG, 0, NULL, NULL, NULL },
  { "Passive", CFG_TYPE_BOOL, ITEM(res_client, passive), 0, CFG_ITEM_DEFAULT, "false", "13.2.0-",
     "If enabled, the Storage Daemon will initiate the network connection to the Client. If disabled, the Client will initiate the network connection to the Storage Daemon." },
  { "ConnectionFromDirectorToClient", CFG_TYPE_BOOL, ITEM(res_client, conn_from_dir_to_fd), 0, CFG_ITEM_DEFAULT, "true", "16.2.2",
     "Let the Director initiate the network connection to the Client." },
  { "AllowClientConnect", CFG_TYPE_BOOL, ITEM(res_client, conn_from_fd_to_dir), 0, CFG_ITEM_DEPRECATED | CFG_ITEM_ALIAS, NULL, NULL,
     "Alias of \"Connection From Client To Director\"." },
  { "ConnectionFromClientToDirector", CFG_TYPE_BOOL, ITEM(res_client, conn_from_fd_to_dir), 0, CFG_ITEM_DEFAULT, "false", "16.2.2",
     "The Director will accept incoming network connection from this Client." },
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_client, enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
  { "HardQuota", CFG_TYPE_SIZE64, ITEM(res_client, HardQuota), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "SoftQuota", CFG_TYPE_SIZE64, ITEM(res_client, SoftQuota), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "SoftQuotaGracePeriod", CFG_TYPE_TIME, ITEM(res_client, SoftQuotaGracePeriod), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "StrictQuotas", CFG_TYPE_BOOL, ITEM(res_client, StrictQuotas), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "QuotaIncludeFailedJobs", CFG_TYPE_BOOL, ITEM(res_client, QuotaIncludeFailedJobs), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "FileRetention", CFG_TYPE_TIME, ITEM(res_client, FileRetention), 0, CFG_ITEM_DEFAULT, "5184000" /* 60 days */, NULL, NULL },
  { "JobRetention", CFG_TYPE_TIME, ITEM(res_client, JobRetention), 0, CFG_ITEM_DEFAULT, "15552000" /* 180 days */, NULL, NULL },
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client, heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "AutoPrune", CFG_TYPE_BOOL, ITEM(res_client, AutoPrune), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client, MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client, max_bandwidth), 0, 0, NULL, NULL, NULL },
  { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_client, ndmp_loglevel), 0, CFG_ITEM_DEFAULT, "4", NULL, NULL },
  { "NdmpBlockSize", CFG_TYPE_SIZE32, ITEM(res_client, ndmp_blocksize), 0, CFG_ITEM_DEFAULT, "64512", NULL, NULL },
  { "NdmpUseLmdb", CFG_TYPE_BOOL, ITEM(res_client, ndmp_use_lmdb), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   TLS_COMMON_CONFIG(res_client),
   TLS_CERT_CONFIG(res_client),
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem store_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_store, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
      "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_store, description_), 0, 0, NULL, NULL, NULL },
  { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_store, Protocol), 0, CFG_ITEM_DEFAULT, "Native", NULL, NULL },
  { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_store, AuthType), 0, CFG_ITEM_DEFAULT, "None", NULL, NULL },
  { "Address", CFG_TYPE_STR, ITEM(res_store, address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "LanAddress", CFG_TYPE_STR, ITEM(res_store, lanaddress), 0, CFG_ITEM_DEFAULT, NULL, "16.2.6-",
     "Sets additional address used for connections between Client and Storage Daemon inside separate network."},
  { "SdAddress", CFG_TYPE_STR, ITEM(res_store, address), 0, CFG_ITEM_ALIAS, NULL, NULL, "Alias for Address." },
  { "Port", CFG_TYPE_PINT32, ITEM(res_store, SDport), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL },
  { "SdPort", CFG_TYPE_PINT32, ITEM(res_store, SDport), 0, CFG_ITEM_DEFAULT | CFG_ITEM_ALIAS, SD_DEFAULT_PORT, NULL, "Alias for Port." },
  { "Username", CFG_TYPE_STR, ITEM(res_store, username), 0, 0, NULL, NULL, NULL },
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_store, password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "SdPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_store, password_), 0, CFG_ITEM_ALIAS, NULL, NULL, "Alias for Password." },
  { "Device", CFG_TYPE_DEVICE, ITEM(res_store, device), R_DEVICE, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "MediaType", CFG_TYPE_STRNAME, ITEM(res_store, media_type), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "AutoChanger", CFG_TYPE_BOOL, ITEM(res_store, autochanger), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_store, enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
  { "AllowCompression", CFG_TYPE_BOOL, ITEM(res_store, AllowCompress), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store, heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "CacheStatusInterval", CFG_TYPE_TIME, ITEM(res_store, cache_status_interval), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL },
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store, MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
  { "MaximumConcurrentReadJobs", CFG_TYPE_PINT32, ITEM(res_store, MaxConcurrentReadJobs), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "SddPort", CFG_TYPE_PINT32, ITEM(res_store, SDDport), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "PairedStorage", CFG_TYPE_RES, ITEM(res_store, paired_storage), R_STORAGE, 0, NULL, NULL, NULL },
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store, max_bandwidth), 0, 0, NULL, NULL, NULL },
  { "CollectStatistics", CFG_TYPE_BOOL, ITEM(res_store, collectstats), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "NdmpChangerDevice", CFG_TYPE_STRNAME, ITEM(res_store, ndmp_changer_device), 0, 0, NULL, "16.2.4-",
     "Allows direct control of a Storage Daemon Auto Changer device by the Director. Only used in NDMP_NATIVE environments." },
   // TapeDevice has never been used in production and intended for NDMP_NATIVE only.
   // Instead of TapeDevice, the required Devices should be configured using the Device directive.
   //{ "TapeDevice", CFG_TYPE_ALIST_STR, ITEM(res_store, tape_devices), 0, CFG_ITEM_ALIAS | CFG_ITEM_DEPRECATED, NULL, "16.2.4-17.2.2",
   //  "Allows direct control of Storage Daemon Tape devices by the Director. Only used in NDMP_NATIVE environments." },
   TLS_COMMON_CONFIG(res_store),
   TLS_CERT_CONFIG(res_store),
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem cat_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_cat, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_cat, description_), 0, 0, NULL, NULL, NULL },
  { "Address", CFG_TYPE_STR, ITEM(res_cat, db_address), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
  { "DbAddress", CFG_TYPE_STR, ITEM(res_cat, db_address), 0, 0, NULL, NULL, NULL },
  { "DbPort", CFG_TYPE_PINT32, ITEM(res_cat, db_port), 0, 0, NULL, NULL, NULL },
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat, db_password), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
  { "DbPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat, db_password), 0, 0, NULL, NULL, NULL },
  { "DbUser", CFG_TYPE_STR, ITEM(res_cat, db_user), 0, 0, NULL, NULL, NULL },
  { "User", CFG_TYPE_STR, ITEM(res_cat, db_user), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
  { "DbName", CFG_TYPE_STR, ITEM(res_cat, db_name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
#ifdef HAVE_DYNAMIC_CATS_BACKENDS
  { "DbDriver", CFG_TYPE_STR, ITEM(res_cat, db_driver), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
#else
  { "DbDriver", CFG_TYPE_STR, ITEM(res_cat, db_driver), 0, 0, NULL, NULL, NULL },
#endif
  { "DbSocket", CFG_TYPE_STR, ITEM(res_cat, db_socket), 0, 0, NULL, NULL, NULL },
   /* Turned off for the moment */
  { "MultipleConnections", CFG_TYPE_BIT, ITEM(res_cat, mult_db_connections), 0, 0, NULL, NULL, NULL },
  { "DisableBatchInsert", CFG_TYPE_BOOL, ITEM(res_cat, disable_batch_insert), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "Reconnect", CFG_TYPE_BOOL, ITEM(res_cat, try_reconnect), 0, CFG_ITEM_DEFAULT, "false",
     "15.1.0-", "Try to reconnect a database connection when its dropped" },
  { "ExitOnFatal", CFG_TYPE_BOOL, ITEM(res_cat, exit_on_fatal), 0, CFG_ITEM_DEFAULT, "false",
     "15.1.0-", "Make any fatal error in the connection to the database exit the program" },
  { "MinConnections", CFG_TYPE_PINT32, ITEM(res_cat, pooling_min_connections), 0, CFG_ITEM_DEFAULT, "1", NULL,
     "This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the minimum number of connections to a database to keep in this database pool." },
  { "MaxConnections", CFG_TYPE_PINT32, ITEM(res_cat, pooling_max_connections), 0, CFG_ITEM_DEFAULT, "5", NULL,
     "This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the maximum number of connections to a database to keep in this database pool." },
  { "IncConnections", CFG_TYPE_PINT32, ITEM(res_cat, pooling_increment_connections), 0, CFG_ITEM_DEFAULT, "1", NULL,
    "This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the number of connections to add to a database pool when not enough connections are available on the pool anymore." },
  { "IdleTimeout", CFG_TYPE_PINT32, ITEM(res_cat, pooling_idle_timeout), 0, CFG_ITEM_DEFAULT, "30", NULL,
     "This directive is used by the experimental database pooling functionality. Only use this for non production sites.  This sets the idle time after which a database pool should be shrinked." },
  { "ValidateTimeout", CFG_TYPE_PINT32, ITEM(res_cat, pooling_validate_timeout), 0, CFG_ITEM_DEFAULT, "120", NULL,
     "This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the validation timeout after which the database connection is polled to see if its still alive." },
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

ResourceItem job_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_job, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_job, description_), 0, 0, NULL, NULL, NULL },
  { "Type", CFG_TYPE_JOBTYPE, ITEM(res_job, JobType), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "Protocol", CFG_TYPE_PROTOCOLTYPE, ITEM(res_job, Protocol), 0, CFG_ITEM_DEFAULT, "Native", NULL, NULL },
  { "BackupFormat", CFG_TYPE_STR, ITEM(res_job, backup_format), 0, CFG_ITEM_DEFAULT, "Native", NULL, NULL },
  { "Level", CFG_TYPE_LEVEL, ITEM(res_job, JobLevel), 0, 0, NULL, NULL, NULL },
  { "Messages", CFG_TYPE_RES, ITEM(res_job, messages), R_MSGS, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_job, storage), R_STORAGE, 0, NULL, NULL, NULL },
  { "Pool", CFG_TYPE_RES, ITEM(res_job, pool), R_POOL, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "FullBackupPool", CFG_TYPE_RES, ITEM(res_job, full_pool), R_POOL, 0, NULL, NULL, NULL },
  { "VirtualFullBackupPool", CFG_TYPE_RES, ITEM(res_job, vfull_pool), R_POOL, 0, NULL, NULL, NULL },
  { "IncrementalBackupPool", CFG_TYPE_RES, ITEM(res_job, inc_pool), R_POOL, 0, NULL, NULL, NULL },
  { "DifferentialBackupPool", CFG_TYPE_RES, ITEM(res_job, diff_pool), R_POOL, 0, NULL, NULL, NULL },
  { "NextPool", CFG_TYPE_RES, ITEM(res_job, next_pool), R_POOL, 0, NULL, NULL, NULL },
  { "Client", CFG_TYPE_RES, ITEM(res_job, client), R_CLIENT, 0, NULL, NULL, NULL },
  { "FileSet", CFG_TYPE_RES, ITEM(res_job, fileset), R_FILESET, 0, NULL, NULL, NULL },
  { "Schedule", CFG_TYPE_RES, ITEM(res_job, schedule), R_SCHEDULE, 0, NULL, NULL, NULL },
  { "VerifyJob", CFG_TYPE_RES, ITEM(res_job, verify_job), R_JOB, CFG_ITEM_ALIAS, NULL, NULL, NULL },
  { "JobToVerify", CFG_TYPE_RES, ITEM(res_job, verify_job), R_JOB, 0, NULL, NULL, NULL },
  { "Catalog", CFG_TYPE_RES, ITEM(res_job, catalog), R_CATALOG, 0, NULL, "13.4.0-", NULL },
  { "JobDefs", CFG_TYPE_RES, ITEM(res_job, jobdefs), R_JOBDEFS, 0, NULL, NULL, NULL },
  { "Run", CFG_TYPE_ALIST_STR, ITEM(res_job, run_cmds), 0, 0, NULL, NULL, NULL },
   /* Root of where to restore files */
  { "Where", CFG_TYPE_DIR, ITEM(res_job, RestoreWhere), 0, 0, NULL, NULL, NULL },
  { "RegexWhere", CFG_TYPE_STR, ITEM(res_job, RegexWhere), 0, 0, NULL, NULL, NULL },
  { "StripPrefix", CFG_TYPE_STR, ITEM(res_job, strip_prefix), 0, 0, NULL, NULL, NULL },
  { "AddPrefix", CFG_TYPE_STR, ITEM(res_job, add_prefix), 0, 0, NULL, NULL, NULL },
  { "AddSuffix", CFG_TYPE_STR, ITEM(res_job, add_suffix), 0, 0, NULL, NULL, NULL },
   /* Where to find bootstrap during restore */
  { "Bootstrap", CFG_TYPE_DIR, ITEM(res_job, RestoreBootstrap), 0, 0, NULL, NULL, NULL },
   /* Where to write bootstrap file during backup */
  { "WriteBootstrap", CFG_TYPE_DIR, ITEM(res_job, WriteBootstrap), 0, 0, NULL, NULL, NULL },
  { "WriteVerifyList", CFG_TYPE_DIR, ITEM(res_job, WriteVerifyList), 0, 0, NULL, NULL, NULL },
  { "Replace", CFG_TYPE_REPLACE, ITEM(res_job, replace), 0, CFG_ITEM_DEFAULT, "Always", NULL, NULL },
  { "MaximumBandwidth", CFG_TYPE_SPEED, ITEM(res_job, max_bandwidth), 0, 0, NULL, NULL, NULL },
  { "MaxRunSchedTime", CFG_TYPE_TIME, ITEM(res_job, MaxRunSchedTime), 0, 0, NULL, NULL, NULL },
  { "MaxRunTime", CFG_TYPE_TIME, ITEM(res_job, MaxRunTime), 0, 0, NULL, NULL, NULL },
  { "FullMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job, FullMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "IncrementalMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job, IncMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "DifferentialMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job, DiffMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "FullMaxRuntime", CFG_TYPE_TIME, ITEM(res_job, FullMaxRunTime), 0, 0, NULL, NULL, NULL },
  { "IncrementalMaxRuntime", CFG_TYPE_TIME, ITEM(res_job, IncMaxRunTime), 0, 0, NULL, NULL, NULL },
  { "DifferentialMaxRuntime", CFG_TYPE_TIME, ITEM(res_job, DiffMaxRunTime), 0, 0, NULL, NULL, NULL },
  { "MaxWaitTime", CFG_TYPE_TIME, ITEM(res_job, MaxWaitTime), 0, 0, NULL, NULL, NULL },
  { "MaxStartDelay", CFG_TYPE_TIME, ITEM(res_job, MaxStartDelay), 0, 0, NULL, NULL, NULL },
  { "MaxFullInterval", CFG_TYPE_TIME, ITEM(res_job, MaxFullInterval), 0, 0, NULL, NULL, NULL },
  { "MaxVirtualFullInterval", CFG_TYPE_TIME, ITEM(res_job, MaxVFullInterval), 0, 0, NULL, "14.4.0-", NULL },
  { "MaxDiffInterval", CFG_TYPE_TIME, ITEM(res_job, MaxDiffInterval), 0, 0, NULL, NULL, NULL },
  { "PrefixLinks", CFG_TYPE_BOOL, ITEM(res_job, PrefixLinks), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "PruneJobs", CFG_TYPE_BOOL, ITEM(res_job, PruneJobs), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "PruneFiles", CFG_TYPE_BOOL, ITEM(res_job, PruneFiles), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "PruneVolumes", CFG_TYPE_BOOL, ITEM(res_job, PruneVolumes), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "PurgeMigrationJob", CFG_TYPE_BOOL, ITEM(res_job, PurgeMigrateJob), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_job, enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
  { "SpoolAttributes", CFG_TYPE_BOOL, ITEM(res_job, SpoolAttributes), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "SpoolData", CFG_TYPE_BOOL, ITEM(res_job, spool_data), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "SpoolSize", CFG_TYPE_SIZE64, ITEM(res_job, spool_size), 0, 0, NULL, NULL, NULL },
  { "RerunFailedLevels", CFG_TYPE_BOOL, ITEM(res_job, rerun_failed_levels), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "PreferMountedVolumes", CFG_TYPE_BOOL, ITEM(res_job, PreferMountedVolumes), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "RunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), 0, 0, NULL, NULL, NULL },
  { "RunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), 0, 0, NULL, NULL, NULL },
  { "RunAfterFailedJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), 0, 0, NULL, NULL, NULL },
  { "ClientRunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), 0, 0, NULL, NULL, NULL },
  { "ClientRunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), 0, 0, NULL, NULL, NULL },
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_job, MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
  { "RescheduleOnError", CFG_TYPE_BOOL, ITEM(res_job, RescheduleOnError), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "RescheduleInterval", CFG_TYPE_TIME, ITEM(res_job, RescheduleInterval), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
  { "RescheduleTimes", CFG_TYPE_PINT32, ITEM(res_job, RescheduleTimes), 0, CFG_ITEM_DEFAULT, "5", NULL, NULL },
  { "Priority", CFG_TYPE_PINT32, ITEM(res_job, Priority), 0, CFG_ITEM_DEFAULT, "10", NULL, NULL },
  { "AllowMixedPriority", CFG_TYPE_BOOL, ITEM(res_job, allow_mixed_priority), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "WritePartAfterJob", CFG_TYPE_BOOL, ITEM(res_job, write_part_after_job), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "SelectionPattern", CFG_TYPE_STR, ITEM(res_job, selection_pattern), 0, 0, NULL, NULL, NULL },
  { "RunScript", CFG_TYPE_RUNSCRIPT, ITEM(res_job, RunScripts), 0, CFG_ITEM_NO_EQUALS, NULL, NULL, NULL },
  { "SelectionType", CFG_TYPE_MIGTYPE, ITEM(res_job, selection_type), 0, 0, NULL, NULL, NULL },
  { "Accurate", CFG_TYPE_BOOL, ITEM(res_job, accurate), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "AllowDuplicateJobs", CFG_TYPE_BOOL, ITEM(res_job, AllowDuplicateJobs), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "AllowHigherDuplicates", CFG_TYPE_BOOL, ITEM(res_job, AllowHigherDuplicates), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "CancelLowerLevelDuplicates", CFG_TYPE_BOOL, ITEM(res_job, CancelLowerLevelDuplicates), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "CancelQueuedDuplicates", CFG_TYPE_BOOL, ITEM(res_job, CancelQueuedDuplicates), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "CancelRunningDuplicates", CFG_TYPE_BOOL, ITEM(res_job, CancelRunningDuplicates), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "SaveFileHistory", CFG_TYPE_BOOL, ITEM(res_job, SaveFileHist), 0, CFG_ITEM_DEFAULT, "true", "14.2.0-", NULL },
  { "FileHistorySize", CFG_TYPE_SIZE64, ITEM(res_job, FileHistSize), 0, CFG_ITEM_DEFAULT, "10000000", "15.2.4-", NULL },
  { "PluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, FdPluginOptions), 0, CFG_ITEM_DEPRECATED | CFG_ITEM_ALIAS, NULL, "-12.4.0", NULL },
  { "FdPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, FdPluginOptions), 0, 0, NULL, NULL, NULL },
  { "SdPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, SdPluginOptions), 0, 0, NULL, NULL, NULL },
  { "DirPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, DirPluginOptions), 0, 0, NULL, NULL, NULL },
  { "Base", CFG_TYPE_ALIST_RES, ITEM(res_job, base), R_JOB, 0, NULL, NULL, NULL },
  { "MaxConcurrentCopies", CFG_TYPE_PINT32, ITEM(res_job, MaxConcurrentCopies), 0, CFG_ITEM_DEFAULT, "100", NULL, NULL },
   /* Settings for always incremental */
  { "AlwaysIncremental", CFG_TYPE_BOOL, ITEM(res_job, AlwaysIncremental), 0, CFG_ITEM_DEFAULT, "false", "16.2.4-",
     "Enable/disable always incremental backup scheme." },
  { "AlwaysIncrementalJobRetention", CFG_TYPE_TIME, ITEM(res_job, AlwaysIncrementalJobRetention), 0, CFG_ITEM_DEFAULT, "0", "16.2.4-",
     "Backup Jobs older than the specified time duration will be merged into a new Virtual backup." },
  { "AlwaysIncrementalKeepNumber", CFG_TYPE_PINT32, ITEM(res_job, AlwaysIncrementalKeepNumber), 0, CFG_ITEM_DEFAULT, "0", "16.2.4-",
     "Guarantee that at least the specified number of Backup Jobs will persist, even if they are older than \"Always Incremental Job Retention\"."},
  { "AlwaysIncrementalMaxFullAge", CFG_TYPE_TIME, ITEM(res_job, AlwaysIncrementalMaxFullAge), 0, 0, NULL, "16.2.4-",
     "If \"AlwaysIncrementalMaxFullAge\" is set, during consolidations only incremental backups will be considered while the Full Backup remains to reduce the amount of data being consolidated. Only if the Full Backup is older than \"AlwaysIncrementalMaxFullAge\", the Full Backup will be part of the consolidation to avoid the Full Backup becoming too old ." },
  { "MaxFullConsolidations", CFG_TYPE_PINT32, ITEM(res_job, MaxFullConsolidations), 0, CFG_ITEM_DEFAULT, "0", "16.2.4-",
     "If \"AlwaysIncrementalMaxFullAge\" is configured, do not run more than \"MaxFullConsolidations\" consolidation jobs that include the Full backup."},
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem fs_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_fs, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_fs, description_), 0, 0, NULL, NULL, NULL },
  { "Include", CFG_TYPE_INCEXC, ITEMC(res_fs), 0, CFG_ITEM_NO_EQUALS, NULL, NULL, NULL },
  { "Exclude", CFG_TYPE_INCEXC, ITEMC(res_fs), 1, CFG_ITEM_NO_EQUALS, NULL, NULL, NULL },
  { "IgnoreFileSetChanges", CFG_TYPE_BOOL, ITEM(res_fs, ignore_fs_changes), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "EnableVSS", CFG_TYPE_BOOL, ITEM(res_fs, enable_vss), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem sch_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_sch, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_sch, description_), 0, 0, NULL, NULL, NULL },
  { "Run", CFG_TYPE_RUN, ITEM(res_sch, run), 0, 0, NULL, NULL, NULL },
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_sch, enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem pool_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_pool, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_pool, description_), 0, 0, NULL, NULL, NULL },
  { "PoolType", CFG_TYPE_POOLTYPE, ITEM(res_pool, pool_type), 0, CFG_ITEM_DEFAULT, "Backup", NULL, NULL },
  { "LabelFormat", CFG_TYPE_STRNAME, ITEM(res_pool, label_format), 0, 0, NULL, NULL, NULL },
  { "LabelType", CFG_TYPE_LABEL, ITEM(res_pool, LabelType), 0, 0, NULL, NULL, NULL },
  { "CleaningPrefix", CFG_TYPE_STRNAME, ITEM(res_pool, cleaning_prefix), 0, CFG_ITEM_DEFAULT, "CLN", NULL, NULL },
  { "UseCatalog", CFG_TYPE_BOOL, ITEM(res_pool, use_catalog), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "UseVolumeOnce", CFG_TYPE_BOOL, ITEM(res_pool, use_volume_once), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
  { "PurgeOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool, purge_oldest_volume), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "ActionOnPurge", CFG_TYPE_ACTIONONPURGE, ITEM(res_pool, action_on_purge), 0, 0, NULL, NULL, NULL },
  { "RecycleOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool, recycle_oldest_volume), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "RecycleCurrentVolume", CFG_TYPE_BOOL, ITEM(res_pool, recycle_current_volume), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
  { "MaximumVolumes", CFG_TYPE_PINT32, ITEM(res_pool, max_volumes), 0, 0, NULL, NULL, NULL },
  { "MaximumVolumeJobs", CFG_TYPE_PINT32, ITEM(res_pool, MaxVolJobs), 0, 0, NULL, NULL, NULL },
  { "MaximumVolumeFiles", CFG_TYPE_PINT32, ITEM(res_pool, MaxVolFiles), 0, 0, NULL, NULL, NULL },
  { "MaximumVolumeBytes", CFG_TYPE_SIZE64, ITEM(res_pool, MaxVolBytes), 0, 0, NULL, NULL, NULL },
  { "CatalogFiles", CFG_TYPE_BOOL, ITEM(res_pool, catalog_files), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "VolumeRetention", CFG_TYPE_TIME, ITEM(res_pool, VolRetention), 0, CFG_ITEM_DEFAULT, "31536000" /* 365 days */, NULL, NULL },
  { "VolumeUseDuration", CFG_TYPE_TIME, ITEM(res_pool, VolUseDuration), 0, 0, NULL, NULL, NULL },
  { "MigrationTime", CFG_TYPE_TIME, ITEM(res_pool, MigrationTime), 0, 0, NULL, NULL, NULL },
  { "MigrationHighBytes", CFG_TYPE_SIZE64, ITEM(res_pool, MigrationHighBytes), 0, 0, NULL, NULL, NULL },
  { "MigrationLowBytes", CFG_TYPE_SIZE64, ITEM(res_pool, MigrationLowBytes), 0, 0, NULL, NULL, NULL },
  { "NextPool", CFG_TYPE_RES, ITEM(res_pool, NextPool), R_POOL, 0, NULL, NULL, NULL },
  { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_pool, storage), R_STORAGE, 0, NULL, NULL, NULL },
  { "AutoPrune", CFG_TYPE_BOOL, ITEM(res_pool, AutoPrune), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "Recycle", CFG_TYPE_BOOL, ITEM(res_pool, Recycle), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
  { "RecyclePool", CFG_TYPE_RES, ITEM(res_pool, RecyclePool), R_POOL, 0, NULL, NULL, NULL },
  { "ScratchPool", CFG_TYPE_RES, ITEM(res_pool, ScratchPool), R_POOL, 0, NULL, NULL, NULL },
  { "Catalog", CFG_TYPE_RES, ITEM(res_pool, catalog), R_CATALOG, 0, NULL, NULL, NULL },
  { "FileRetention", CFG_TYPE_TIME, ITEM(res_pool, FileRetention), 0, 0, NULL, NULL, NULL },
  { "JobRetention", CFG_TYPE_TIME, ITEM(res_pool, JobRetention), 0, 0, NULL, NULL, NULL },
  { "MinimumBlockSize", CFG_TYPE_SIZE32, ITEM(res_pool, MinBlocksize), 0, 0, NULL, NULL, NULL },
  { "MaximumBlockSize", CFG_TYPE_SIZE32, ITEM(res_pool, MaxBlocksize), 0, 0, NULL, "14.2.0-", NULL },
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

static ResourceItem counter_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_counter, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_counter, description_), 0, 0, NULL, NULL, NULL },
  { "Minimum", CFG_TYPE_INT32, ITEM(res_counter, MinValue), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  { "Maximum", CFG_TYPE_PINT32, ITEM(res_counter, MaxValue), 0, CFG_ITEM_DEFAULT, "2147483647" /* INT32_MAX */, NULL, NULL },
  { "WrapCounter", CFG_TYPE_RES, ITEM(res_counter, WrapCounter), R_COUNTER, 0, NULL, NULL, NULL },
  { "Catalog", CFG_TYPE_RES, ITEM(res_counter, Catalog), R_CATALOG, 0, NULL, NULL, NULL },
  {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

#include "lib/messages_resource_items.h"
#include "lib/json.h"

/**
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 *  NOTE!!! keep it in the same order as the R_codes
 *    or eliminate all resources[rindex].name
 *
 * name handler value code flags default_value
 */
static ResourceTable resources[] = {
  { "Director", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      [] (){ return new(&res_dir) DirectorResource(); }, &res_dir },
  { "Client", cli_items, R_CLIENT, sizeof(ClientResource),
      [] (){ return new(&res_client) ClientResource(); }, &res_client  },
  { "JobDefs", job_items, R_JOBDEFS, sizeof(JobResource),
      [] (){ return new(&res_job) JobResource(); }, &res_job },
  { "Job", job_items, R_JOB, sizeof(JobResource),
      [] (){ return new(&res_job) JobResource(); }, &res_job },
  { "Storage", store_items, R_STORAGE, sizeof(StorageResource),
      [] (){ return new(&res_store) StorageResource(); }, &res_store },
  { "Catalog", cat_items, R_CATALOG, sizeof(CatalogResource),
      [] (){ return new(&res_cat) CatalogResource(); }, &res_cat },
  { "Schedule", sch_items, R_SCHEDULE, sizeof(ScheduleResource),
      [] (){ return new(&res_sch) ScheduleResource(); }, &res_sch },
  { "FileSet", fs_items, R_FILESET, sizeof(FilesetResource),
      [] (){ return new(&res_fs) FilesetResource(); }, &res_fs },
  { "Pool", pool_items, R_POOL, sizeof(PoolResource),
      [] (){ return new(&res_pool) PoolResource(); }, &res_pool },
  { "Messages", msgs_items, R_MSGS, sizeof(MessagesResource),
      [] (){ return new(&res_msgs) MessagesResource(); }, &res_msgs },
  { "Counter", counter_items, R_COUNTER, sizeof(CounterResource),
      [] (){ return new(&res_counter) CounterResource(); }, &res_counter },
  { "Profile", profile_items, R_PROFILE, sizeof(ProfileResource),
      [] (){ return new(&res_profile) ProfileResource(); }, &res_profile },
  { "Console", con_items, R_CONSOLE, sizeof(ConsoleResource),
      [] (){ return new(&res_con) ConsoleResource(); }, &res_con },
  { "User", con_items, R_CONSOLE, sizeof(ConsoleResource),
      [] (){ return new(&res_con) ConsoleResource(); }, &res_con },
  { "Device", NULL, R_DEVICE, sizeof(DeviceResource),
      [] (){ return new(&res_dev) DeviceResource(); }, &res_dev },/* info obtained from SD */
  {nullptr, nullptr, 0, 0, nullptr, nullptr}
};

/**
 * Note, when this resource is used, we are inside a Job
 * resource. We treat the RunScript like a sort of
 * mini-resource within the Job resource. As such we
 * don't use the UnionOfResources union because that contains
 * a Job resource (and it would corrupt the Job resource)
 * but use a global separate resource for holding the
 * runscript data.
 */
static RunScript res_runscript;

/**
 * new RunScript items
 * name handler value code flags default_value
 */
static ResourceItem runscript_items[] = {
 { "Command", CFG_TYPE_RUNSCRIPT_CMD, ITEMC(res_runscript), SHELL_CMD, 0, NULL, NULL, NULL },
 { "Console", CFG_TYPE_RUNSCRIPT_CMD, ITEMC(res_runscript), CONSOLE_CMD, 0, NULL, NULL, NULL },
 { "Target", CFG_TYPE_RUNSCRIPT_TARGET, ITEMC(res_runscript), 0, 0, NULL, NULL, NULL },
 { "RunsOnSuccess", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript,on_success), 0, 0, NULL, NULL, NULL },
 { "RunsOnFailure", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript,on_failure), 0, 0, NULL, NULL, NULL },
 { "FailJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript,fail_on_error), 0, 0, NULL, NULL, NULL },
 { "AbortJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript,fail_on_error), 0, 0, NULL, NULL, NULL },
 { "RunsWhen", CFG_TYPE_RUNSCRIPT_WHEN, ITEM(res_runscript,when), 0, 0, NULL, NULL, NULL },
 { "RunsOnClient", CFG_TYPE_RUNSCRIPT_TARGET, ITEMC(res_runscript), 0, 0, NULL, NULL, NULL }, /* TODO */
 {nullptr, 0, {nullptr}, nullptr, 0, 0, nullptr, nullptr, nullptr}
};

/* clang-format on */
/**
 * The following arrays are referenced from else where and
 * used for display to the user so the keyword are pretty
 * printed with additional capitals. As the code uses
 * strcasecmp anyhow this doesn't matter.
 */

/**
 * Keywords (RHS) permitted in Job Level records
 *
 * name level job_type
 */
struct s_jl joblevels[] = {
    {"Full", L_FULL, JT_BACKUP},
    {"Base", L_BASE, JT_BACKUP},
    {"Incremental", L_INCREMENTAL, JT_BACKUP},
    {"Differential", L_DIFFERENTIAL, JT_BACKUP},
    {"Since", L_SINCE, JT_BACKUP},
    {"VirtualFull", L_VIRTUAL_FULL, JT_BACKUP},
    {"Catalog", L_VERIFY_CATALOG, JT_VERIFY},
    {"InitCatalog", L_VERIFY_INIT, JT_VERIFY},
    {"VolumeToCatalog", L_VERIFY_VOLUME_TO_CATALOG, JT_VERIFY},
    {"DiskToCatalog", L_VERIFY_DISK_TO_CATALOG, JT_VERIFY},
    {"Data", L_VERIFY_DATA, JT_VERIFY},
    {"Full", L_FULL, JT_COPY},
    {"Incremental", L_INCREMENTAL, JT_COPY},
    {"Differential", L_DIFFERENTIAL, JT_COPY},
    {"Full", L_FULL, JT_MIGRATE},
    {"Incremental", L_INCREMENTAL, JT_MIGRATE},
    {"Differential", L_DIFFERENTIAL, JT_MIGRATE},
    {" ", L_NONE, JT_ADMIN},
    {" ", L_NONE, JT_ARCHIVE},
    {" ", L_NONE, JT_RESTORE},
    {" ", L_NONE, JT_CONSOLIDATE},
    {NULL, 0, 0}};

/** Keywords (RHS) permitted in Job type records
 *
 * type_name job_type
 */
struct s_jt jobtypes[] = {{"Backup", JT_BACKUP},
                          {"Admin", JT_ADMIN},
                          {"Archive", JT_ARCHIVE},
                          {"Verify", JT_VERIFY},
                          {"Restore", JT_RESTORE},
                          {"Migrate", JT_MIGRATE},
                          {"Copy", JT_COPY},
                          {"Consolidate", JT_CONSOLIDATE},
                          {NULL, 0}};

/** Keywords (RHS) permitted in Protocol type records
 *
 * name token
 */
static struct s_kw backupprotocols[] = {
    {"Native", PT_NATIVE},
    {"NDMP_BAREOS", PT_NDMP_BAREOS}, /* alias for PT_NDMP */
    {"NDMP", PT_NDMP_BAREOS},
    {"NDMP_NATIVE", PT_NDMP_NATIVE},
    {NULL, 0}};

/** Keywords (RHS) permitted in AuthProtocol type records
 *
 * name token
 */
static struct s_kw authprotocols[] = {{"Native", APT_NATIVE},
                                      {"NDMPV2", APT_NDMPV2},
                                      {"NDMPV3", APT_NDMPV3},
                                      {"NDMPV4", APT_NDMPV4},
                                      {NULL, 0}};

/**
 * Keywords (RHS) permitted in Authentication type records
 *
 * name token
 */
static struct s_kw authmethods[] = {{"None", AT_NONE},
                                    {"Clear", AT_CLEAR},
                                    {"MD5", AT_MD5},
                                    {NULL, 0}};

/**
 * Keywords (RHS) permitted in Selection type records
 *
 * type_name job_type
 */
static struct s_jt migtypes[] = {{"SmallestVolume", MT_SMALLEST_VOL},
                                 {"OldestVolume", MT_OLDEST_VOL},
                                 {"PoolOccupancy", MT_POOL_OCCUPANCY},
                                 {"PoolTime", MT_POOL_TIME},
                                 {"PoolUncopiedJobs", MT_POOL_UNCOPIED_JOBS},
                                 {"Client", MT_CLIENT},
                                 {"Volume", MT_VOLUME},
                                 {"Job", MT_JOB},
                                 {"SqlQuery", MT_SQLQUERY},
                                 {NULL, 0}};

/**
 * Keywords (RHS) permitted in Restore replace type records
 *
 * name token
 */
struct s_kw ReplaceOptions[] = {{"Always", REPLACE_ALWAYS},
                                {"IfNewer", REPLACE_IFNEWER},
                                {"IfOlder", REPLACE_IFOLDER},
                                {"Never", REPLACE_NEVER},
                                {NULL, 0}};

/**
 * Keywords (RHS) permitted in ActionOnPurge type records
 *
 * name token
 */
struct s_kw ActionOnPurgeOptions[] = {{"None", ON_PURGE_NONE},
                                      {"Truncate", ON_PURGE_TRUNCATE},
                                      {NULL, 0}};

/**
 * Keywords (RHS) permitted in Volume status type records
 *
 * token is set to zero for all items as this
 * is not a mapping but a filter table.
 *
 * name token
 */
struct s_kw VolumeStatus[] = {{"Append", 0},  {"Full", 0},   {"Used", 0},
                              {"Recycle", 0}, {"Purged", 0}, {"Cleaning", 0},
                              {"Error", 0},   {NULL, 0}};

/**
 * Keywords (RHS) permitted in Pool type records
 *
 * token is set to zero for all items as this
 * is not a mapping but a filter table.
 *
 * name token
 */
struct s_kw PoolTypes[] = {{"Backup", 0},  {"Copy", 0},      {"Cloned", 0},
                           {"Archive", 0}, {"Migration", 0}, {"Scratch", 0},
                           {NULL, 0}};

#ifdef HAVE_JANSSON
json_t* json_item(s_jl* item)
{
  json_t* json = json_object();

  json_object_set_new(json, "level", json_integer(item->level));
  json_object_set_new(json, "type", json_integer(item->job_type));

  return json;
}

json_t* json_item(s_jt* item)
{
  json_t* json = json_object();

  json_object_set_new(json, "type", json_integer(item->job_type));

  return json;
}

json_t* json_datatype_header(const int type, const char* typeclass)
{
  json_t* json = json_object();
  const char* description = DatatypeToDescription(type);

  json_object_set_new(json, "number", json_integer(type));

  if (description) {
    json_object_set_new(json, "description", json_string(description));
  }

  if (typeclass) { json_object_set_new(json, "class", json_string(typeclass)); }

  return json;
}

json_t* json_datatype(const int type)
{
  return json_datatype_header(type, NULL);
}

json_t* json_datatype(const int type, s_kw items[])
{
  json_t* json = json_datatype_header(type, "keyword");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(values, items[i].name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

json_t* json_datatype(const int type, s_jl items[])
{
  // FIXME: level_name keyword is not unique
  json_t* json = json_datatype_header(type, "keyword");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].level_name; i++) {
      json_object_set_new(values, items[i].level_name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

json_t* json_datatype(const int type, s_jt items[])
{
  json_t* json = json_datatype_header(type, "keyword");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].type_name; i++) {
      json_object_set_new(values, items[i].type_name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

json_t* json_datatype(const int type, ResourceItem items[])
{
  json_t* json = json_datatype_header(type, "sub");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(values, items[i].name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

/**
 * Print configuration file schema in json format
 */
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  DatatypeName* datatype;
  ResourceTable* resources = my_config->resources_;

  InitializeJson();

  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-dir"));
  json_object_set_new(json, "version", json_string(VERSION));

  /*
   * Resources
   */
  json_t* resource = json_object();
  json_object_set(json, "resource", resource);
  json_t* bareos_dir = json_object();
  json_object_set(resource, "bareos-dir", bareos_dir);

  for (int r = 0; resources[r].name; r++) {
    ResourceTable resource = my_config->resources_[r];
    json_object_set(bareos_dir, resource.name, json_items(resource.items));
  }

  /*
   * Datatypes
   */
  json_t* json_datatype_obj = json_object();
  json_object_set(json, "datatype", json_datatype_obj);

  int d = 0;
  while (GetDatatype(d)->name != NULL) {
    datatype = GetDatatype(d);

    switch (datatype->number) {
      case CFG_TYPE_RUNSCRIPT:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_RUNSCRIPT, runscript_items));
        break;
      case CFG_TYPE_INCEXC:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_incexc(CFG_TYPE_INCEXC));
        break;
      case CFG_TYPE_OPTIONS:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_options(CFG_TYPE_OPTIONS));
        break;
      case CFG_TYPE_PROTOCOLTYPE:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_PROTOCOLTYPE, backupprotocols));
        break;
      case CFG_TYPE_AUTHPROTOCOLTYPE:
        json_object_set(
            json_datatype_obj, DatatypeToString(datatype->number),
            json_datatype(CFG_TYPE_AUTHPROTOCOLTYPE, authprotocols));
        break;
      case CFG_TYPE_AUTHTYPE:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_AUTHTYPE, authmethods));
        break;
      case CFG_TYPE_LEVEL:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_LEVEL, joblevels));
        break;
      case CFG_TYPE_JOBTYPE:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_JOBTYPE, jobtypes));
        break;
      case CFG_TYPE_MIGTYPE:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_MIGTYPE, migtypes));
        break;
      case CFG_TYPE_REPLACE:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_REPLACE, ReplaceOptions));
        break;
      case CFG_TYPE_ACTIONONPURGE:
        json_object_set(
            json_datatype_obj, DatatypeToString(datatype->number),
            json_datatype(CFG_TYPE_ACTIONONPURGE, ActionOnPurgeOptions));
        break;
      case CFG_TYPE_POOLTYPE:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_POOLTYPE, PoolTypes));
        break;
      case CFG_TYPE_RUN:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(CFG_TYPE_RUN, RunFields));
        break;
      default:
        json_object_set(json_datatype_obj, DatatypeToString(datatype->number),
                        json_datatype(datatype->number));
        break;
    }
    d++;
  }

  /*
   * following datatypes are ignored:
   * - VolumeStatus: only used in ua_dotcmds, not a datatype
   * - FS_option_kw: from inc_conf. Replaced by CFG_TYPE_OPTIONS",
   * options_items.
   */

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

static bool CmdlineItem(PoolMem* buffer, ResourceItem* item)
{
  PoolMem temp;
  PoolMem key;
  const char* nomod = "";
  const char* mod_start = nomod;
  const char* mod_end = nomod;

  if (item->flags & CFG_ITEM_ALIAS) { return false; }

  if (item->flags & CFG_ITEM_DEPRECATED) { return false; }

  if (item->flags & CFG_ITEM_NO_EQUALS) {
    /* TODO: currently not supported */
    return false;
  }

  if (!(item->flags & CFG_ITEM_REQUIRED)) {
    mod_start = "[";
    mod_end = "]";
  }

  /*
   * Tab completion only supports lower case keywords.
   */
  key.strcat(item->name);
  key.toLower();

  temp.bsprintf(" %s%s=<%s>%s", mod_start, key.c_str(),
                DatatypeToString(item->type), mod_end);
  PmStrcat(buffer, temp.c_str());

  return true;
}

static bool CmdlineItems(PoolMem* buffer, ResourceItem items[])
{
  if (!items) { return false; }

  for (int i = 0; items[i].name; i++) { CmdlineItem(buffer, &items[i]); }

  return true;
}

/**
 * Get the usage string for the console "configure" command.
 *
 * This will be all available resource directives.
 * They are formated in a way to be usable for command line completion.
 */
const char* get_configure_usage_string()
{
  PoolMem resourcename;

  if (!configure_usage_string) {
    configure_usage_string = new PoolMem(PM_BSOCK);
  }

  /*
   * Only fill the configure_usage_string once. The content is static.
   */
  if (configure_usage_string->strlen() == 0) {
    /*
     * subcommand: add
     */
    for (int r = 0; resources[r].name; r++) {
      /*
       * Only one Director is allowed.
       * If the resource have not items, there is no need to add it.
       */
      if ((resources[r].rcode != R_DIRECTOR) && (resources[r].items)) {
        configure_usage_string->strcat("add ");
        resourcename.strcpy(resources[r].name);
        resourcename.toLower();
        configure_usage_string->strcat(resourcename);
        CmdlineItems(configure_usage_string, resources[r].items);
        configure_usage_string->strcat(" |\n");
      }
    }
    /*
     * subcommand: export
     */
    configure_usage_string->strcat("export client=<client>");
  }

  return configure_usage_string->c_str();
}

void DestroyConfigureUsageString()
{
  if (configure_usage_string) {
    delete configure_usage_string;
    configure_usage_string = NULL;
  }
}

/**
 * Propagate the settings from source BareosResource to dest BareosResource
 * using the RES_ITEMS array.
 */
static void PropagateResource(ResourceItem* items,
                              JobResource* source,
                              JobResource* dest)
{
  uint32_t offset;

  for (int i = 0; items[i].name; i++) {
    if (!BitIsSet(i, dest->item_present_) &&
        BitIsSet(i, source->item_present_)) {
      offset = (char*)(items[i].value) - (char*)&res_job;
      switch (items[i].type) {
        case CFG_TYPE_STR:
        case CFG_TYPE_DIR: {
          char **def_svalue, **svalue;

          /*
           * Handle strings and directory strings
           */
          def_svalue = (char**)((char*)(source) + offset);
          svalue = (char**)((char*)dest + offset);
          if (*svalue) { free(*svalue); }
          *svalue = bstrdup(*def_svalue);
          SetBit(i, dest->item_present_);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_RES: {
          char **def_svalue, **svalue;

          /*
           * Handle resources
           */
          def_svalue = (char**)((char*)(source) + offset);
          svalue = (char**)((char*)dest + offset);
          if (*svalue) {
            Pmsg1(000, _("Hey something is wrong. p=0x%lu\n"), *svalue);
          }
          *svalue = *def_svalue;
          SetBit(i, dest->item_present_);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_ALIST_STR: {
          const char* str = nullptr;
          alist *orig_list, **new_list;

          /*
           * Handle alist strings
           */
          orig_list = *(alist**)((char*)(source) + offset);

          /*
           * See if there is anything on the list.
           */
          if (orig_list && orig_list->size()) {
            new_list = (alist**)((char*)(dest) + offset);

            if (!*new_list) { *new_list = New(alist(10, owned_by_alist)); }

            foreach_alist (str, orig_list) {
              (*new_list)->append(bstrdup(str));
            }

            SetBit(i, dest->item_present_);
            SetBit(i, dest->inherit_content_);
          }
          break;
        }
        case CFG_TYPE_ALIST_RES: {
          BareosResource* res = nullptr;
          alist *orig_list, **new_list;

          /*
           * Handle alist resources
           */
          orig_list = *(alist**)((char*)(source) + offset);

          /*
           * See if there is anything on the list.
           */
          if (orig_list && orig_list->size()) {
            new_list = (alist**)((char*)(dest) + offset);

            if (!*new_list) { *new_list = New(alist(10, not_owned_by_alist)); }

            foreach_alist (res, orig_list) {
              (*new_list)->append(res);
            }

            SetBit(i, dest->item_present_);
            SetBit(i, dest->inherit_content_);
          }
          break;
        }
        case CFG_TYPE_ACL: {
          const char* str = nullptr;
          alist *orig_list, **new_list;

          /*
           * Handle ACL lists.
           */
          orig_list = ((alist**)((char*)(source) + offset))[items[i].code];

          /*
           * See if there is anything on the list.
           */
          if (orig_list && orig_list->size()) {
            new_list = &(((alist**)((char*)(dest) + offset))[items[i].code]);

            if (!*new_list) { *new_list = New(alist(10, owned_by_alist)); }

            foreach_alist (str, orig_list) {
              (*new_list)->append(bstrdup(str));
            }

            SetBit(i, dest->item_present_);
            SetBit(i, dest->inherit_content_);
          }
          break;
        }
        case CFG_TYPE_BIT:
        case CFG_TYPE_PINT32:
        case CFG_TYPE_JOBTYPE:
        case CFG_TYPE_PROTOCOLTYPE:
        case CFG_TYPE_LEVEL:
        case CFG_TYPE_INT32:
        case CFG_TYPE_SIZE32:
        case CFG_TYPE_MIGTYPE:
        case CFG_TYPE_REPLACE: {
          uint32_t *def_ivalue, *ivalue;

          /*
           * Handle integer fields
           *    Note, our StoreBit does not handle bitmaped fields
           */
          def_ivalue = (uint32_t*)((char*)(source) + offset);
          ivalue = (uint32_t*)((char*)dest + offset);
          *ivalue = *def_ivalue;
          SetBit(i, dest->item_present_);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_TIME:
        case CFG_TYPE_SIZE64:
        case CFG_TYPE_INT64:
        case CFG_TYPE_SPEED: {
          int64_t *def_lvalue, *lvalue;

          /*
           * Handle 64 bit integer fields
           */
          def_lvalue = (int64_t*)((char*)(source) + offset);
          lvalue = (int64_t*)((char*)dest + offset);
          *lvalue = *def_lvalue;
          SetBit(i, dest->item_present_);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_BOOL: {
          bool *def_bvalue, *bvalue;

          /*
           * Handle bool fields
           */
          def_bvalue = (bool*)((char*)(source) + offset);
          bvalue = (bool*)((char*)dest + offset);
          *bvalue = *def_bvalue;
          SetBit(i, dest->item_present_);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_AUTOPASSWORD: {
          s_password *s_pwd, *d_pwd;

          /*
           * Handle password fields
           */
          s_pwd = (s_password*)((char*)(source) + offset);
          d_pwd = (s_password*)((char*)(dest) + offset);

          d_pwd->encoding = s_pwd->encoding;
          d_pwd->value = bstrdup(s_pwd->value);
          SetBit(i, dest->item_present_);
          SetBit(i, dest->inherit_content_);
          break;
        }
        default:
          Dmsg2(200,
                "Don't know how to propagate resource %s of configtype %d\n",
                items[i].name, items[i].type);
          break;
      }
    }
  }
}


/**
 * Ensure that all required items are present
 */
bool ValidateResource(int res_type, ResourceItem* items, BareosResource* res)
{
  if (res_type == R_JOBDEFS) {
    /*
     * a jobdef don't have to be fully defined.
     */
    return true;
  } else if (res_type == R_JOB) {
    if (!((JobResource*)res)->validate()) { return false; }
  }

  for (int i = 0; items[i].name; i++) {
    if (items[i].flags & CFG_ITEM_REQUIRED) {
      if (!BitIsSet(i, res->item_present_)) {
        Jmsg(NULL, M_ERROR, 0,
             _("\"%s\" directive in %s \"%s\" resource is required, but not "
               "found.\n"),
             items[i].name, my_config->ResToStr(res_type), res->resource_name_);
        return false;
      }
    }

    /*
     * If this triggers, take a look at lib/parse_conf.h
     */
    if (i >= MAX_RES_ITEMS) {
      Emsg1(M_ERROR, 0, _("Too many items in %s resource\n"),
            my_config->ResToStr(res_type));
      return false;
    }
  }

  return true;
}

bool JobResource::validate()
{
  /*
   * For Copy and Migrate we can have Jobs without a client or fileset.
   * As for a copy we use the original Job as a reference for the Read storage
   * we also don't need to check if there is an explicit storage definition in
   * either the Job or the Read pool.
   */
  switch (JobType) {
    case JT_COPY:
    case JT_MIGRATE:
      break;
    default:
      /*
       * All others must have a client and fileset.
       */
      if (!client) {
        Jmsg(NULL, M_ERROR, 0,
             _("\"client\" directive in Job \"%s\" resource is required, but "
               "not found.\n"),
             resource_name_);
        return false;
      }

      if (!fileset) {
        Jmsg(NULL, M_ERROR, 0,
             _("\"fileset\" directive in Job \"%s\" resource is required, but "
               "not found.\n"),
             resource_name_);
        return false;
      }

      if (!storage && (!pool || !pool->storage)) {
        Jmsg(NULL, M_ERROR, 0,
             _("No storage specified in Job \"%s\" nor in Pool.\n"),
             resource_name_);
        return false;
      }
      break;
  }

  return true;
}

char* CatalogResource::display(POOLMEM* dst)
{
  Mmsg(dst,
       "catalog=%s\ndb_name=%s\ndb_driver=%s\ndb_user=%s\n"
       "db_password=%s\ndb_address=%s\ndb_port=%i\n"
       "db_socket=%s\n",
       resource_name_, NPRTB(db_name), NPRTB(db_driver), NPRTB(db_user),
       NPRTB(db_password.value), NPRTB(db_address), db_port, NPRTB(db_socket));

  return dst;
}

static void PrintConfigRunscript(ResourceItem* item, PoolMem& cfg_str)
{
  PoolMem temp;
  RunScript* runscript = nullptr;
  alist* list;

  list = *item->alistvalue;
  if (Bstrcasecmp(item->name, "runscript")) {
    if (list != NULL) {
      foreach_alist (runscript, list) {
        PoolMem esc;

        EscapeString(esc, runscript->command, strlen(runscript->command));

        /*
         * Don't print runscript when its inherited from a JobDef.
         */
        if (runscript->from_jobdef) { continue; }

        /*
         * Check if runscript must be written as short runscript
         */
        if (runscript->short_form) {
          if (runscript->when == SCRIPT_Before && /* runbeforejob */
              (bstrcmp(runscript->target, ""))) {
            Mmsg(temp, "run before job = \"%s\"\n", esc.c_str());
          } else if (runscript->when == SCRIPT_After && /* runafterjob */
                     runscript->on_success && !runscript->on_failure &&
                     !runscript->fail_on_error &&
                     bstrcmp(runscript->target, "")) {
            Mmsg(temp, "run after job = \"%s\"\n", esc.c_str());
          } else if (runscript->when ==
                         SCRIPT_After && /* client run after job */
                     runscript->on_success &&
                     !runscript->on_failure && !runscript->fail_on_error &&
                     !bstrcmp(runscript->target, "")) {
            Mmsg(temp, "client run after job = \"%s\"\n", esc.c_str());
          } else if (runscript->when ==
                         SCRIPT_Before && /* client run before job */
                     !bstrcmp(runscript->target, "")) {
            Mmsg(temp, "client run before job = \"%s\"\n", esc.c_str());
          } else if (runscript->when ==
                         SCRIPT_After && /* run after failed job */
                     runscript->on_failure &&
                     !runscript->on_success && !runscript->fail_on_error &&
                     bstrcmp(runscript->target, "")) {
            Mmsg(temp, "run after failed job = \"%s\"\n", esc.c_str());
          }
          IndentConfigItem(cfg_str, 1, temp.c_str());
        } else {
          Mmsg(temp, "runscript {\n");
          IndentConfigItem(cfg_str, 1, temp.c_str());

          char* cmdstring = (char*)"command"; /* '|' */
          if (runscript->cmd_type == '@') { cmdstring = (char*)"console"; }

          Mmsg(temp, "%s = \"%s\"\n", cmdstring, esc.c_str());
          IndentConfigItem(cfg_str, 2, temp.c_str());

          /*
           * Default: never
           */
          char* when = (char*)"never";
          switch (runscript->when) {
            case SCRIPT_Before:
              when = (char*)"before";
              break;
            case SCRIPT_After:
              when = (char*)"after";
              break;
            case SCRIPT_AfterVSS:
              when = (char*)"aftervss";
              break;
            case SCRIPT_Any:
              when = (char*)"always";
              break;
          }

          if (!Bstrcasecmp(when, "never")) { /* suppress default value */
            Mmsg(temp, "runswhen = %s\n", when);
            IndentConfigItem(cfg_str, 2, temp.c_str());
          }

          /*
           * Default: fail_on_error = true
           */
          char* fail_on_error = (char*)"Yes";
          if (!runscript->fail_on_error) {
            fail_on_error = (char*)"No";
            Mmsg(temp, "failonerror = %s\n", fail_on_error);
            IndentConfigItem(cfg_str, 2, temp.c_str());
          }

          /*
           * Default: on_success = true
           */
          char* run_on_success = (char*)"Yes";
          if (!runscript->on_success) {
            run_on_success = (char*)"No";
            Mmsg(temp, "runsonsuccess = %s\n", run_on_success);
            IndentConfigItem(cfg_str, 2, temp.c_str());
          }

          /*
           * Default: on_failure = false
           */
          char* run_on_failure = (char*)"No";
          if (runscript->on_failure) {
            run_on_failure = (char*)"Yes";
            Mmsg(temp, "runsonfailure = %s\n", run_on_failure);
            IndentConfigItem(cfg_str, 2, temp.c_str());
          }

          /* level is not implemented
          Dmsg1(200, "   level = %d\n", runscript->level);
          */

          /*
           * Default: runsonclient = yes
           */
          char* runsonclient = (char*)"Yes";
          if (bstrcmp(runscript->target, "")) {
            runsonclient = (char*)"No";
            Mmsg(temp, "runsonclient = %s\n", runsonclient);
            IndentConfigItem(cfg_str, 2, temp.c_str());
          }

          IndentConfigItem(cfg_str, 1, "}\n");
        }
      }
    } /* foreach runscript */
  }
}

static void PrintConfigRun(ResourceItem* item, PoolMem& cfg_str)
{
  PoolMem temp;
  RunResource* run;
  bool all_set;
  int i, nr_items;
  int interval_start;

  /* clang-format off */

   char *weekdays[] = {
      (char *)"Sun",
      (char *)"Mon",
      (char *)"Tue",
      (char *)"Wed",
      (char *)"Thu",
      (char *)"Fri",
      (char *)"Sat"
   };

   char *months[] = {
      (char *)"Jan",
      (char *)"Feb",
      (char *)"Mar",
      (char *)"Apr",
      (char *)"May",
      (char *)"Jun",
      (char *)"Jul",
      (char *)"Aug",
      (char *)"Sep",
      (char *)"Oct",
      (char *)"Nov",
      (char *)"Dec"
   };

   char *ordinals[] = {
      (char *)"1st",
      (char *)"2nd",
      (char *)"3rd",
      (char *)"4th",
      (char *)"5th"
   };

  /* clang-format on */

  run = (RunResource*)*(item->value);
  if (run != NULL) {
    while (run) {
      PoolMem run_str;  /* holds the complete run= ... line */
      PoolMem interval; /* is one entry of day/month/week etc. */

      IndentConfigItem(cfg_str, 1, "run = ");

      /*
       * Overrides
       */
      if (run->pool) {
        Mmsg(temp, "pool=\"%s\" ", run->pool->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->full_pool) {
        Mmsg(temp, "fullpool=\"%s\" ", run->full_pool->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->vfull_pool) {
        Mmsg(temp, "virtualfullpool=\"%s\" ", run->vfull_pool->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->inc_pool) {
        Mmsg(temp, "incrementalpool=\"%s\" ", run->inc_pool->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->diff_pool) {
        Mmsg(temp, "differentialpool=\"%s\" ", run->diff_pool->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->next_pool) {
        Mmsg(temp, "nextpool=\"%s\" ", run->next_pool->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->level) {
        for (int j = 0; joblevels[j].level_name; j++) {
          if (joblevels[j].level == run->level) {
            PmStrcat(run_str, joblevels[j].level_name);
            PmStrcat(run_str, " ");
            break;
          }
        }
      }

      if (run->storage) {
        Mmsg(temp, "storage=\"%s\" ", run->storage->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->msgs) {
        Mmsg(temp, "messages=\"%s\" ", run->msgs->resource_name_);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->Priority && run->Priority != 10) {
        Mmsg(temp, "priority=%d ", run->Priority);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->MaxRunSchedTime) {
        Mmsg(temp, "maxrunschedtime=%d ", run->MaxRunSchedTime);
        PmStrcat(run_str, temp.c_str());
      }

      if (run->accurate) {
        /*
         * TODO: You cannot distinct if accurate was not set or if it was set to
         * no maybe we need an additional variable like "accurate_set".
         */
        Mmsg(temp, "accurate=\"%s\" ", "yes");
        PmStrcat(run_str, temp.c_str());
      }

      /*
       * Now the time specification
       */

      /*
       * run->mday , output is just the number comma separated
       */
      PmStrcpy(temp, "");

      /*
       * First see if not all bits are set.
       */
      all_set = true;
      nr_items = 31;
      for (i = 0; i < nr_items; i++) {
        if (!BitIsSet(i, run->mday)) { all_set = false; }
      }

      if (!all_set) {
        interval_start = -1;

        for (i = 0; i < nr_items; i++) {
          if (BitIsSet(i, run->mday)) {
            if (interval_start ==
                -1) {             /* bit is set and we are not in an interval */
              interval_start = i; /* start an interval */
              Dmsg1(200, "starting interval at %d\n", i + 1);
              Mmsg(interval, ",%d", i + 1);
              PmStrcat(temp, interval.c_str());
            }
          }

          if (!BitIsSet(i, run->mday)) {
            if (interval_start !=
                -1) { /* bit is unset and we are in an interval */
              if ((i - interval_start) > 1) {
                Dmsg2(200, "found end of interval from %d to %d\n",
                      interval_start + 1, i);
                Mmsg(interval, "-%d", i);
                PmStrcat(temp, interval.c_str());
              }
              interval_start = -1; /* end the interval */
            }
          }
        }

        /*
         * See if we are still in an interval and the last bit is also set then
         * the interval stretches to the last item.
         */
        i = nr_items - 1;
        if (interval_start != -1 && BitIsSet(i, run->mday)) {
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %d to %d\n",
                  interval_start + 1, i + 1);
            Mmsg(interval, "-%d", i + 1);
            PmStrcat(temp, interval.c_str());
          }
        }

        PmStrcat(temp, " ");
        PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
      }

      /*
       * run->wom output is 1st, 2nd... 5th comma separated
       *                    first, second, third... is also allowed
       *                    but we ignore that for now
       */
      all_set = true;
      nr_items = 5;
      for (i = 0; i < nr_items; i++) {
        if (!BitIsSet(i, run->wom)) { all_set = false; }
      }

      if (!all_set) {
        interval_start = -1;

        PmStrcpy(temp, "");
        for (i = 0; i < nr_items; i++) {
          if (BitIsSet(i, run->wom)) {
            if (interval_start ==
                -1) {             /* bit is set and we are not in an interval */
              interval_start = i; /* start an interval */
              Dmsg1(200, "starting interval at %s\n", ordinals[i]);
              Mmsg(interval, ",%s", ordinals[i]);
              PmStrcat(temp, interval.c_str());
            }
          }

          if (!BitIsSet(i, run->wom)) {
            if (interval_start !=
                -1) { /* bit is unset and we are in an interval */
              if ((i - interval_start) > 1) {
                Dmsg2(200, "found end of interval from %s to %s\n",
                      ordinals[interval_start], ordinals[i - 1]);
                Mmsg(interval, "-%s", ordinals[i - 1]);
                PmStrcat(temp, interval.c_str());
              }
              interval_start = -1; /* end the interval */
            }
          }
        }

        /*
         * See if we are still in an interval and the last bit is also set then
         * the interval stretches to the last item.
         */
        i = nr_items - 1;
        if (interval_start != -1 && BitIsSet(i, run->wom)) {
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %s to %s\n",
                  ordinals[interval_start], ordinals[i]);
            Mmsg(interval, "-%s", ordinals[i]);
            PmStrcat(temp, interval.c_str());
          }
        }

        PmStrcat(temp, " ");
        PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
      }

      /*
       * run->wday output is Sun, Mon, ..., Sat comma separated
       */
      all_set = true;
      nr_items = 7;
      for (i = 0; i < nr_items; i++) {
        if (!BitIsSet(i, run->wday)) { all_set = false; }
      }

      if (!all_set) {
        interval_start = -1;

        PmStrcpy(temp, "");
        for (i = 0; i < nr_items; i++) {
          if (BitIsSet(i, run->wday)) {
            if (interval_start ==
                -1) {             /* bit is set and we are not in an interval */
              interval_start = i; /* start an interval */
              Dmsg1(200, "starting interval at %s\n", weekdays[i]);
              Mmsg(interval, ",%s", weekdays[i]);
              PmStrcat(temp, interval.c_str());
            }
          }

          if (!BitIsSet(i, run->wday)) {
            if (interval_start !=
                -1) { /* bit is unset and we are in an interval */
              if ((i - interval_start) > 1) {
                Dmsg2(200, "found end of interval from %s to %s\n",
                      weekdays[interval_start], weekdays[i - 1]);
                Mmsg(interval, "-%s", weekdays[i - 1]);
                PmStrcat(temp, interval.c_str());
              }
              interval_start = -1; /* end the interval */
            }
          }
        }

        /*
         * See if we are still in an interval and the last bit is also set then
         * the interval stretches to the last item.
         */
        i = nr_items - 1;
        if (interval_start != -1 && BitIsSet(i, run->wday)) {
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %s to %s\n",
                  weekdays[interval_start], weekdays[i]);
            Mmsg(interval, "-%s", weekdays[i]);
            PmStrcat(temp, interval.c_str());
          }
        }

        PmStrcat(temp, " ");
        PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
      }

      /*
       * run->month output is Jan, Feb, ..., Dec comma separated
       */
      all_set = true;
      nr_items = 12;
      for (i = 0; i < nr_items; i++) {
        if (!BitIsSet(i, run->month)) { all_set = false; }
      }

      if (!all_set) {
        interval_start = -1;

        PmStrcpy(temp, "");
        for (i = 0; i < nr_items; i++) {
          if (BitIsSet(i, run->month)) {
            if (interval_start ==
                -1) {             /* bit is set and we are not in an interval */
              interval_start = i; /* start an interval */
              Dmsg1(200, "starting interval at %s\n", months[i]);
              Mmsg(interval, ",%s", months[i]);
              PmStrcat(temp, interval.c_str());
            }
          }

          if (!BitIsSet(i, run->month)) {
            if (interval_start !=
                -1) { /* bit is unset and we are in an interval */
              if ((i - interval_start) > 1) {
                Dmsg2(200, "found end of interval from %s to %s\n",
                      months[interval_start], months[i - 1]);
                Mmsg(interval, "-%s", months[i - 1]);
                PmStrcat(temp, interval.c_str());
              }
              interval_start = -1; /* end the interval */
            }
          }
        }

        /*
         * See if we are still in an interval and the last bit is also set then
         * the interval stretches to the last item.
         */
        i = nr_items - 1;
        if (interval_start != -1 && BitIsSet(i, run->month)) {
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %s to %s\n",
                  months[interval_start], months[i]);
            Mmsg(interval, "-%s", months[i]);
            PmStrcat(temp, interval.c_str());
          }
        }

        PmStrcat(temp, " ");
        PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
      }

      /*
       * run->woy output is w00 - w53, comma separated
       */
      all_set = true;
      nr_items = 54;
      for (i = 0; i < nr_items; i++) {
        if (!BitIsSet(i, run->woy)) { all_set = false; }
      }

      if (!all_set) {
        interval_start = -1;

        PmStrcpy(temp, "");
        for (i = 0; i < nr_items; i++) {
          if (BitIsSet(i, run->woy)) {
            if (interval_start ==
                -1) {             /* bit is set and we are not in an interval */
              interval_start = i; /* start an interval */
              Dmsg1(200, "starting interval at w%02d\n", i);
              Mmsg(interval, ",w%02d", i);
              PmStrcat(temp, interval.c_str());
            }
          }

          if (!BitIsSet(i, run->woy)) {
            if (interval_start !=
                -1) { /* bit is unset and we are in an interval */
              if ((i - interval_start) > 1) {
                Dmsg2(200, "found end of interval from w%02d to w%02d\n",
                      interval_start, i - 1);
                Mmsg(interval, "-w%02d", i - 1);
                PmStrcat(temp, interval.c_str());
              }
              interval_start = -1; /* end the interval */
            }
          }
        }

        /*
         * See if we are still in an interval and the last bit is also set then
         * the interval stretches to the last item.
         */
        i = nr_items - 1;
        if (interval_start != -1 && BitIsSet(i, run->woy)) {
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from w%02d to w%02d\n",
                  interval_start, i);
            Mmsg(interval, "-w%02d", i);
            PmStrcat(temp, interval.c_str());
          }
        }

        PmStrcat(temp, " ");
        PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
      }

      /*
       * run->hour output is HH:MM for hour and minute though its a bitfield.
       * only "hourly" sets all bits.
       */
      PmStrcpy(temp, "");
      for (i = 0; i < 24; i++) {
        if
          BitIsSet(i, run->hour)
          {
            Mmsg(temp, "at %02d:%02d\n", i, run->minute);
            PmStrcat(run_str, temp.c_str());
          }
      }

      /*
       * run->minute output is smply the minute in HH:MM
       */
      PmStrcat(cfg_str, run_str.c_str());

      run = run->next;
    } /* loop over runs */
  }
}

bool FilesetResource::PrintConfig(PoolMem& buff,
                                  const ConfigurationParser& /* unused */,
                                  bool hide_sensitive_data,
                                  bool verbose)
{
  PoolMem cfg_str;
  PoolMem temp;
  const char* p;

  Dmsg0(200, "FilesetResource::PrintConfig\n");

  Mmsg(temp, "FileSet {\n");
  PmStrcat(cfg_str, temp.c_str());

  Mmsg(temp, "Name = \"%s\"\n", this->resource_name_);
  IndentConfigItem(cfg_str, 1, temp.c_str());

  if (this->description_ != NULL) {
    Mmsg(temp, "Description = \"%s\"\n", this->description_);
    IndentConfigItem(cfg_str, 1, temp.c_str());
  }

  for (std::size_t i = 0; i < include_items.size(); i++) {
    IncludeExcludeItem* incexe = include_items[i];

    IndentConfigItem(cfg_str, 1, "Include {\n");

    /*
     * Start options block
     */
    for (std::size_t j = 0; j < incexe->opts_list.size(); j++) {
      FileOptions* fo = incexe->opts_list[j];

      IndentConfigItem(cfg_str, 2, "Options {\n");
      for (p = &fo->opts[0]; *p; p++) {
        switch (*p) {
          case '0': /* no option */
            break;
          case 'a': /* alway replace */
            IndentConfigItem(cfg_str, 3, "Replace = Always\n");
            break;
          case 'C': /* */
            IndentConfigItem(cfg_str, 3, "Accurate = ");
            p++; /* skip C */
            for (; *p && *p != ':'; p++) {
              Mmsg(temp, "%c", *p);
              PmStrcat(cfg_str, temp.c_str());
            }
            PmStrcat(cfg_str, "\n");
            break;
          case 'c':
            IndentConfigItem(cfg_str, 3, "CheckFileChanges = Yes\n");
            break;
          case 'd':
            switch (*(p + 1)) {
              case '1':
                IndentConfigItem(cfg_str, 3, "Shadowing = LocalWarn\n");
                p++;
                break;
              case '2':
                IndentConfigItem(cfg_str, 3, "Shadowing = LocalRemove\n");
                p++;
                break;
              case '3':
                IndentConfigItem(cfg_str, 3, "Shadowing = GlobalWarn\n");
                p++;
                break;
              case '4':
                IndentConfigItem(cfg_str, 3, "Shadowing = GlobalRemove\n");
                p++;
                break;
            }
            break;
          case 'e':
            IndentConfigItem(cfg_str, 3, "Exclude = Yes\n");
            break;
          case 'f':
            IndentConfigItem(cfg_str, 3, "OneFS = No\n");
            break;
          case 'h': /* no recursion */
            IndentConfigItem(cfg_str, 3, "Recurse = No\n");
            break;
          case 'H': /* no hard link handling */
            IndentConfigItem(cfg_str, 3, "Hardlinks = No\n");
            break;
          case 'i':
            IndentConfigItem(cfg_str, 3, "IgnoreCase = Yes\n");
            break;
          case 'J': /* Base Job */
            IndentConfigItem(cfg_str, 3, "BaseJob = ");
            p++; /* skip J */
            for (; *p && *p != ':'; p++) {
              Mmsg(temp, "%c", *p);
              PmStrcat(cfg_str, temp.c_str());
            }
            PmStrcat(cfg_str, "\n");
            break;
          case 'M': /* MD5 */
            IndentConfigItem(cfg_str, 3, "Signature = MD5\n");
            break;
          case 'n':
            IndentConfigItem(cfg_str, 3, "Replace = Never\n");
            break;
          case 'p': /* use portable data format */
            IndentConfigItem(cfg_str, 3, "Portable = Yes\n");
            break;
          case 'P': /* strip path */
            IndentConfigItem(cfg_str, 3, "Strip = ");
            p++; /* skip P */
            for (; *p && *p != ':'; p++) {
              Mmsg(temp, "%c", *p);
              PmStrcat(cfg_str, temp.c_str());
            }
            PmStrcat(cfg_str, "\n");
            break;
          case 'R': /* Resource forks and Finder Info */
            IndentConfigItem(cfg_str, 3, "HFSPlusSupport = Yes\n");
            break;
          case 'r': /* read fifo */
            IndentConfigItem(cfg_str, 3, "ReadFifo = Yes\n");
            break;
          case 'S':
            switch (*(p + 1)) {
#ifdef HAVE_SHA2
              case '2':
                IndentConfigItem(cfg_str, 3, "Signature = SHA256\n");
                p++;
                break;
              case '3':
                IndentConfigItem(cfg_str, 3, "Signature = SHA512\n");
                p++;
                break;
#endif
              default:
                IndentConfigItem(cfg_str, 3, "Signature = SHA1\n");
                break;
            }
            break;
          case 's':
            IndentConfigItem(cfg_str, 3, "Sparse = Yes\n");
            break;
          case 'm':
            IndentConfigItem(cfg_str, 3, "MtimeOnly = Yes\n");
            break;
          case 'k':
            IndentConfigItem(cfg_str, 3, "KeepAtime = Yes\n");
            break;
          case 'K':
            IndentConfigItem(cfg_str, 3, "NoAtime = Yes\n");
            break;
          case 'A':
            IndentConfigItem(cfg_str, 3, "AclSupport = Yes\n");
            break;
          case 'V': /* verify options */
            IndentConfigItem(cfg_str, 3, "Verify = ");
            p++; /* skip V */
            for (; *p && *p != ':'; p++) {
              Mmsg(temp, "%c", *p);
              PmStrcat(cfg_str, temp.c_str());
            }
            PmStrcat(cfg_str, "\n");
            break;
          case 'w':
            IndentConfigItem(cfg_str, 3, "Replace = IfNewer\n");
            break;
          case 'W':
            IndentConfigItem(cfg_str, 3, "EnhancedWild = Yes\n");
            break;
          case 'z': /* size */
            IndentConfigItem(cfg_str, 3, "Size = ");
            p++; /* skip z */
            for (; *p && *p != ':'; p++) {
              Mmsg(temp, "%c", *p);
              PmStrcat(cfg_str, temp.c_str());
            }
            PmStrcat(cfg_str, "\n");
            break;
          case 'Z': /* compression */
            IndentConfigItem(cfg_str, 3, "Compression = ");
            p++; /* skip Z */
            switch (*p) {
              case '0':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
              case '8':
              case '9':
                Mmsg(temp, "GZIP%c\n", *p);
                PmStrcat(cfg_str, temp.c_str());
                break;
              case 'o':
                Mmsg(temp, "LZO\n");
                PmStrcat(cfg_str, temp.c_str());
                break;
              case 'f':
                p++; /* skip f */
                switch (*p) {
                  case 'f':
                    Mmsg(temp, "LZFAST\n");
                    PmStrcat(cfg_str, temp.c_str());
                    break;
                  case '4':
                    Mmsg(temp, "LZ4\n");
                    PmStrcat(cfg_str, temp.c_str());
                    break;
                  case 'h':
                    Mmsg(temp, "LZ4HC\n");
                    PmStrcat(cfg_str, temp.c_str());
                    break;
                  default:
                    Emsg1(M_ERROR, 0,
                          _("Unknown compression include/exclude option: "
                            "%c\n"),
                          *p);
                    break;
                }
                break;
              default:
                Emsg1(M_ERROR, 0,
                      _("Unknown compression include/exclude option: %c\n"),
                      *p);
                break;
            }
            break;
          case 'X':
            IndentConfigItem(cfg_str, 3, "XattrSupport = Yes\n");
            break;
          case 'x':
            IndentConfigItem(cfg_str, 3, "AutoExclude = No\n");
            break;
          default:
            Emsg1(M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *p);
            break;
        }
      }

      for (int k = 0; k < fo->regex.size(); k++) {
        Mmsg(temp, "Regex = \"%s\"\n", fo->regex.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->regexdir.size(); k++) {
        Mmsg(temp, "Regex Dir = \"%s\"\n", fo->regexdir.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->regexfile.size(); k++) {
        Mmsg(temp, "Regex File = \"%s\"\n", fo->regexfile.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->wild.size(); k++) {
        Mmsg(temp, "Wild = \"%s\"\n", fo->wild.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->wilddir.size(); k++) {
        Mmsg(temp, "Wild Dir = \"%s\"\n", fo->wilddir.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->wildfile.size(); k++) {
        Mmsg(temp, "Wild File = \"%s\"\n", fo->wildfile.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      /*
       *  Wildbase is WildFile not containing a / or \\
       *  see  void StoreWild() in inc_conf.c
       *  so we need to translate it back to a Wild File entry
       */
      for (int k = 0; k < fo->wildbase.size(); k++) {
        Mmsg(temp, "Wild File = \"%s\"\n", fo->wildbase.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->base.size(); k++) {
        Mmsg(temp, "Base = \"%s\"\n", fo->base.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->fstype.size(); k++) {
        Mmsg(temp, "Fs Type = \"%s\"\n", fo->fstype.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->Drivetype.size(); k++) {
        Mmsg(temp, "Drive Type = \"%s\"\n", fo->Drivetype.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      for (int k = 0; k < fo->meta.size(); k++) {
        Mmsg(temp, "Meta = \"%s\"\n", fo->meta.get(k));
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      if (fo->plugin) {
        Mmsg(temp, "Plugin = \"%s\"\n", fo->plugin);
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      if (fo->reader) {
        Mmsg(temp, "Reader = \"%s\"\n", fo->reader);
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      if (fo->writer) {
        Mmsg(temp, "Writer = \"%s\"\n", fo->writer);
        IndentConfigItem(cfg_str, 3, temp.c_str());
      }

      IndentConfigItem(cfg_str, 2, "}\n");
    } /* end options block */

    /*
     * File = entries.
     */
    if (incexe->name_list.size()) {
      char* entry;
      PoolMem esc;

      for (int l = 0; l < incexe->name_list.size(); l++) {
        entry = (char*)incexe->name_list.get(l);
        EscapeString(esc, entry, strlen(entry));
        Mmsg(temp, "File = \"%s\"\n", esc.c_str());
        IndentConfigItem(cfg_str, 2, temp.c_str());
      }
    }

    /*
     * Plugin = entries.
     */
    if (incexe->plugin_list.size()) {
      char* entry;
      PoolMem esc;

      for (int l = 0; l < incexe->plugin_list.size(); l++) {
        entry = (char*)incexe->plugin_list.get(l);
        EscapeString(esc, entry, strlen(entry));
        Mmsg(temp, "Plugin = \"%s\"\n", esc.c_str());
        IndentConfigItem(cfg_str, 2, temp.c_str());
      }
    }

    /*
     * Exclude Dir Containing = entry.
     */
    if (incexe->ignoredir.size()) {
      for (int l = 0; l < incexe->ignoredir.size(); l++) {
        Mmsg(temp, "Exclude Dir Containing = \"%s\"\n",
             incexe->ignoredir.get(l));
        IndentConfigItem(cfg_str, 2, temp.c_str());
      }
    }

    IndentConfigItem(cfg_str, 1, "}\n");

  } /* loop over all include blocks */

  for (std::size_t j = 0; j < exclude_items.size(); j++) {
    IncludeExcludeItem* incexe = exclude_items[j];

    if (incexe->name_list.size()) {
      char* entry;
      PoolMem esc;

      IndentConfigItem(cfg_str, 1, "Exclude {\n");
      for (int k = 0; k < incexe->name_list.size(); k++) {
        entry = (char*)incexe->name_list.get(k);
        EscapeString(esc, entry, strlen(entry));
        Mmsg(temp, "File = \"%s\"\n", esc.c_str());
        IndentConfigItem(cfg_str, 2, temp.c_str());
      }

      IndentConfigItem(cfg_str, 1, "}\n");
    }
  } /* loop over all exclude blocks */

  PmStrcat(cfg_str, "}\n\n");
  PmStrcat(buff, cfg_str.c_str());

  return true;
}

const char* auth_protocol_to_str(uint32_t auth_protocol)
{
  for (int i = 0; authprotocols[i].name; i++) {
    if (authprotocols[i].token == auth_protocol) {
      return authprotocols[i].name;
    }
  }

  return "Unknown";
}

const char* level_to_str(int level)
{
  static char level_no[30];
  const char* str = level_no;

  Bsnprintf(level_no, sizeof(level_no), "%c (%d)", level,
            level); /* default if not found */
  for (int i = 0; joblevels[i].level_name; i++) {
    if (level == (int)joblevels[i].level) {
      str = joblevels[i].level_name;
      break;
    }
  }

  return str;
}

/**
 * Free all the members of an IncludeExcludeItem structure
 */
static void FreeIncexe(IncludeExcludeItem* incexe)
{
  incexe->name_list.destroy();
  incexe->plugin_list.destroy();
  for (FileOptions* fopt : incexe->opts_list) {
    fopt->regex.destroy();
    fopt->regexdir.destroy();
    fopt->regexfile.destroy();
    fopt->wild.destroy();
    fopt->wilddir.destroy();
    fopt->wildfile.destroy();
    fopt->wildbase.destroy();
    fopt->base.destroy();
    fopt->fstype.destroy();
    fopt->Drivetype.destroy();
    fopt->meta.destroy();
    if (fopt->plugin) { free(fopt->plugin); }
    if (fopt->reader) { free(fopt->reader); }
    if (fopt->writer) { free(fopt->writer); }
    delete fopt;
  }
  incexe->opts_list.clear();
  incexe->ignoredir.destroy();
}

static bool UpdateResourcePointer(int type, ResourceItem* items)
{
  bool result = true;

  switch (type) {
    case R_PROFILE:
    case R_CATALOG:
    case R_MSGS:
    case R_FILESET:
    case R_DEVICE:
      /*
       * Resources not containing a resource
       */
      break;
    case R_POOL: {
      /*
       * Resources containing another resource or alist. First
       * look up the resource which contains another resource. It
       * was written during pass 1.  Then stuff in the pointers to
       * the resources it contains, which were inserted this pass.
       * Finally, it will all be stored back.
       *
       * Find resource saved in pass 1
       */
      PoolResource* p = dynamic_cast<PoolResource*>(
          my_config->GetResWithName(R_POOL, res_pool.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Pool resource %s\n"),
              res_pool.resource_name_);
        return false;
      } else {
        p->NextPool = res_pool.NextPool;
        p->RecyclePool = res_pool.RecyclePool;
        p->ScratchPool = res_pool.ScratchPool;
        p->storage = res_pool.storage;
        if (res_pool.catalog || !res_pool.use_catalog) {
          p->catalog = res_pool.catalog;
        }
      }
      break;
    }
    case R_CONSOLE: {
      ConsoleResource* p = dynamic_cast<ConsoleResource*>(
          my_config->GetResWithName(R_CONSOLE, res_con.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Console resource %s\n"),
              res_con.resource_name_);
        return false;
      } else {
        p->tls_cert_.allowed_certificate_common_names_ =
            std::move(res_con.tls_cert_.allowed_certificate_common_names_);
        p->profiles = res_con.profiles;
      }
      break;
    }
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(
          my_config->GetResWithName(R_DIRECTOR, res_dir.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Director resource %s\n"),
              res_dir.resource_name_);
        return false;
      } else {
        p->plugin_names = res_dir.plugin_names;
        p->messages = res_dir.messages;
        p->backend_directories = res_dir.backend_directories;
        p->tls_cert_.allowed_certificate_common_names_ =
            std::move(res_dir.tls_cert_.allowed_certificate_common_names_);
      }
      break;
    }
    case R_STORAGE: {
      StorageResource* p = dynamic_cast<StorageResource*>(
          my_config->GetResWithName(type, res_store.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Storage resource %s\n"),
              res_dir.resource_name_);
        return false;
      } else {
        int status;

        p->paired_storage = res_store.paired_storage;
        p->tls_cert_.allowed_certificate_common_names_ =
            std::move(res_store.tls_cert_.allowed_certificate_common_names_);

        p->device = res_store.device;

        p->runtime_storage_status = new RuntimeStorageStatus;

        if ((status = pthread_mutex_init(
                 &p->runtime_storage_status->changer_lock, NULL)) != 0) {
          BErrNo be;

          Emsg1(M_ERROR_TERM, 0, _("pthread_mutex_init: ERR=%s\n"),
                be.bstrerror(status));
        }
        if ((status = pthread_mutex_init(
                 &p->runtime_storage_status->ndmp_deviceinfo_lock, NULL)) !=
            0) {
          BErrNo be;

          Emsg1(M_ERROR_TERM, 0, _("pthread_mutex_init: ERR=%s\n"),
                be.bstrerror(status));
        }
      }
      break;
    }
    case R_JOBDEFS:
    case R_JOB: {
      JobResource* p = dynamic_cast<JobResource*>(
          my_config->GetResWithName(type, res_job.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Job resource %s\n"),
              res_job.resource_name_);
        return false;
      } else {
        p->messages = res_job.messages;
        p->schedule = res_job.schedule;
        p->client = res_job.client;
        p->fileset = res_job.fileset;
        p->storage = res_job.storage;
        p->catalog = res_job.catalog;
        p->FdPluginOptions = res_job.FdPluginOptions;
        p->SdPluginOptions = res_job.SdPluginOptions;
        p->DirPluginOptions = res_job.DirPluginOptions;
        p->base = res_job.base;
        p->pool = res_job.pool;
        p->full_pool = res_job.full_pool;
        p->vfull_pool = res_job.vfull_pool;
        p->inc_pool = res_job.inc_pool;
        p->diff_pool = res_job.diff_pool;
        p->next_pool = res_job.next_pool;
        p->verify_job = res_job.verify_job;
        p->jobdefs = res_job.jobdefs;
        p->run_cmds = res_job.run_cmds;
        p->RunScripts = res_job.RunScripts;

        /*
         * TODO: JobDefs where/regexwhere doesn't work well (but this is not
         * very useful) We have to SetBit(index, item_present_); or
         * something like that
         *
         * We take RegexWhere before all other options
         */
        if (!p->RegexWhere &&
            (p->strip_prefix || p->add_suffix || p->add_prefix)) {
          int len = BregexpGetBuildWhereSize(p->strip_prefix, p->add_prefix,
                                             p->add_suffix);
          p->RegexWhere = (char*)bmalloc(len * sizeof(char));
          bregexp_build_where(p->RegexWhere, len, p->strip_prefix,
                              p->add_prefix, p->add_suffix);
          /*
           * TODO: test bregexp
           */
        }

        if (p->RegexWhere && p->RestoreWhere) {
          free(p->RestoreWhere);
          p->RestoreWhere = NULL;
        }

        if (type == R_JOB) {
          p->rjs = (runtime_job_status_t*)malloc(sizeof(runtime_job_status_t));
          memset(p->rjs, 0, sizeof(runtime_job_status_t));
        }
      }
      break;
    }
    case R_COUNTER: {
      CounterResource* p = dynamic_cast<CounterResource*>(
          my_config->GetResWithName(type, res_counter.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Counter resource %s\n"),
              res_counter.resource_name_);
        return false;
      } else {
        p->Catalog = res_counter.Catalog;
        p->WrapCounter = res_counter.WrapCounter;
      }
      break;
    }
    case R_CLIENT: {
      ClientResource* p = dynamic_cast<ClientResource*>(
          my_config->GetResWithName(type, res_client.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Client resource %s\n"),
              res_client.resource_name_);
        return false;
      } else {
        if (res_client.catalog) {
          p->catalog = res_client.catalog;
        } else {
          /*
           * No catalog overwrite given use the first catalog definition.
           */
          p->catalog = (CatalogResource*)my_config->GetNextRes(R_CATALOG, NULL);
        }
        p->tls_cert_.allowed_certificate_common_names_ =
            std::move(res_client.tls_cert_.allowed_certificate_common_names_);

        p->rcs =
            (runtime_client_status_t*)malloc(sizeof(runtime_client_status_t));
        memset(p->rcs, 0, sizeof(runtime_client_status_t));
      }
      break;
    }
    case R_SCHEDULE: {
      /*
       * Schedule is a bit different in that it contains a RunResource record
       * chain which isn't a "named" resource. This chain was linked
       * in by run_conf.c during pass 2, so here we jam the pointer
       * into the Schedule resource.
       */
      ScheduleResource* p = dynamic_cast<ScheduleResource*>(
          my_config->GetResWithName(type, res_sch.resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, _("Cannot find Schedule resource %s\n"),
              res_client.resource_name_);
        return false;
      } else {
        p->run = res_sch.run;
      }
      break;
    }
    default:
      Emsg1(M_ERROR, 0, _("Unknown resource type %d in SaveResource.\n"), type);
      result = false;
      break;
  }
  return result;
}

bool PropagateJobdefs(int res_type, JobResource* res)
{
  JobResource* jobdefs = NULL;

  if (!res->jobdefs) { return true; }

  /*
   * Don't allow the JobDefs pointing to itself.
   */
  if (res->jobdefs == res) { return false; }

  if (res_type == R_JOB) {
    jobdefs = res->jobdefs;

    /*
     * Handle RunScripts alists specifically
     */
    if (jobdefs->RunScripts) {
      RunScript *rs = nullptr, *elt;

      if (!res->RunScripts) {
        res->RunScripts = New(alist(10, not_owned_by_alist));
      }

      foreach_alist (rs, jobdefs->RunScripts) {
        elt = copy_runscript(rs);
        elt->from_jobdef = true;
        res->RunScripts->append(elt); /* we have to free it */
      }
    }
  }

  /*
   * Transfer default items from JobDefs Resource
   */
  PropagateResource(job_items, res->jobdefs, res);

  return true;
}

/**
 * Populate Job Defaults (e.g. JobDefs)
 */
static bool populate_jobdefs()
{
  JobResource *job, *jobdefs;
  bool retval = true;

  /*
   * Propagate the content of a JobDefs to another.
   */
  foreach_res (jobdefs, R_JOBDEFS) {
    PropagateJobdefs(R_JOBDEFS, jobdefs);
  }

  /*
   * Propagate the content of the JobDefs to the actual Job.
   */
  foreach_res (job, R_JOB) {
    PropagateJobdefs(R_JOB, job);

    /*
     * Ensure that all required items are present
     */
    if (!ValidateResource(R_JOB, job_items, job)) {
      retval = false;
      goto bail_out;
    }

  } /* End loop over Job res */

bail_out:
  return retval;
}

bool PopulateDefs() { return populate_jobdefs(); }

static void StorePooltype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  if (pass == 1) {
    for (i = 0; PoolTypes[i].name; i++) {
      if (Bstrcasecmp(lc->str, PoolTypes[i].name)) {
        /*
         * If a default was set free it first.
         */
        if (*(item->value)) { free(*(item->value)); }
        *(item->value) = bstrdup(PoolTypes[i].name);
        i = 0;
        break;
      }
    }

    if (i != 0) {
      scan_err1(lc, _("Expected a Pool Type option, got: %s"), lc->str);
    }
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

static void StoreActiononpurge(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;
  uint32_t* destination = item->ui32value;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both in pass 1 and pass 2
   * Scan ActionOnPurge options
   */
  for (i = 0; ActionOnPurgeOptions[i].name; i++) {
    if (Bstrcasecmp(lc->str, ActionOnPurgeOptions[i].name)) {
      *destination = (*destination) | ActionOnPurgeOptions[i].token;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected an Action On Purge option, got: %s"), lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store Device. Note, the resource is created upon the
 * first reference. The details of the resource are obtained
 * later from the SD.
 */
static void StoreDevice(LEX* lc, ResourceItem* item, int index, int pass)
{
  int rindex = R_DEVICE - R_FIRST;
  bool found = false;

  if (pass == 1) {
    LexGetToken(lc, BCT_NAME);
    if (!res_head[rindex]) {
      DeviceResource* dev = new DeviceResource;
      dev->rcode_ = R_DEVICE;
      dev->resource_name_ = bstrdup(lc->str);
      res_head[rindex] = dev; /* store first entry */
      Dmsg3(900, "Inserting first %s res: %s index=%d\n",
            my_config->ResToStr(R_DEVICE), dev->resource_name_, rindex);
    } else {
      BareosResource* next;
      for (next = res_head[rindex]; next->next_; next = next->next_) {
        if (bstrcmp(next->resource_name_, lc->str)) {
          found = true;  // already defined
          break;
        }
      }
      if (!found) {
        DeviceResource* dev = new DeviceResource;
        dev->rcode_ = R_DEVICE;
        dev->resource_name_ = bstrdup(lc->str);
        next->next_ = dev;
        Dmsg4(900, "Inserting %s res: %s index=%d pass=%d\n",
              my_config->ResToStr(R_DEVICE), dev->resource_name_, rindex, pass);
      }
    }

    ScanToEol(lc);
    SetBit(index, item->static_resource->item_present_);
    ClearBit(index, item->static_resource->inherit_content_);
  } else {
    my_config->StoreResource(CFG_TYPE_ALIST_RES, lc, item, index, pass);
  }
}

/**
 * Store Migration/Copy type
 */
static void StoreMigtype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both in pass 1 and pass 2
   */
  for (i = 0; migtypes[i].type_name; i++) {
    if (Bstrcasecmp(lc->str, migtypes[i].type_name)) {
      *(item->ui32value) = migtypes[i].job_type;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Migration Job Type keyword, got: %s"), lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store JobType (backup, verify, restore)
 */
static void StoreJobtype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both in pass 1 and pass 2
   */
  for (i = 0; jobtypes[i].type_name; i++) {
    if (Bstrcasecmp(lc->str, jobtypes[i].type_name)) {
      *(item->ui32value) = jobtypes[i].job_type;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Job Type keyword, got: %s"), lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store Protocol (Native, NDMP/NDMP_BAREOS, NDMP_NATIVE)
 */
static void StoreProtocoltype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both in pass 1 and pass 2
   */
  for (i = 0; backupprotocols[i].name; i++) {
    if (Bstrcasecmp(lc->str, backupprotocols[i].name)) {
      *(item->ui32value) = backupprotocols[i].token;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Protocol Type keyword, got: %s"), lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

static void StoreReplace(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Scan Replacement options
   */
  for (i = 0; ReplaceOptions[i].name; i++) {
    if (Bstrcasecmp(lc->str, ReplaceOptions[i].name)) {
      *(item->ui32value) = ReplaceOptions[i].token;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Restore replacement option, got: %s"), lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store Auth Protocol (Native, NDMPv2, NDMPv3, NDMPv4)
 */
static void StoreAuthprotocoltype(LEX* lc,
                                  ResourceItem* item,
                                  int index,
                                  int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both in pass 1 and pass 2
   */
  for (i = 0; authprotocols[i].name; i++) {
    if (Bstrcasecmp(lc->str, authprotocols[i].name)) {
      *(item->ui32value) = authprotocols[i].token;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Auth Protocol Type keyword, got: %s"), lc->str);
  }
  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store authentication type (Mostly for NDMP like clear or MD5).
 */
static void StoreAuthtype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the type both in pass 1 and pass 2
   */
  for (i = 0; authmethods[i].name; i++) {
    if (Bstrcasecmp(lc->str, authmethods[i].name)) {
      *(item->ui32value) = authmethods[i].token;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Authentication Type keyword, got: %s"),
              lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store Job Level (Full, Incremental, ...)
 */
static void StoreLevel(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;

  LexGetToken(lc, BCT_NAME);
  /*
   * Store the level pass 2 so that type is defined
   */
  for (i = 0; joblevels[i].level_name; i++) {
    if (Bstrcasecmp(lc->str, joblevels[i].level_name)) {
      *(item->ui32value) = joblevels[i].level;
      i = 0;
      break;
    }
  }

  if (i != 0) {
    scan_err1(lc, _("Expected a Job Level keyword, got: %s"), lc->str);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store password either clear if for NDMP and catalog or MD5 hashed for
 * native.
 */
static void StoreAutopassword(LEX* lc, ResourceItem* item, int index, int pass)
{
  switch (item->static_resource->rcode_) {  // Ueb --> Initialisierung prüfen
    case R_DIRECTOR:
      /*
       * As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1.
       */
      switch (item->code) {
        case 1:
          my_config->StoreResource(CFG_TYPE_CLEARPASSWORD, lc, item, index,
                                   pass);
          break;
        default:
          my_config->StoreResource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_CLIENT:
      switch (res_client.Protocol) {
        case APT_NDMPV2:
        case APT_NDMPV3:
        case APT_NDMPV4:
          my_config->StoreResource(CFG_TYPE_CLEARPASSWORD, lc, item, index,
                                   pass);
          break;
        default:
          my_config->StoreResource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_STORAGE:
      switch (res_store.Protocol) {
        case APT_NDMPV2:
        case APT_NDMPV3:
        case APT_NDMPV4:
          my_config->StoreResource(CFG_TYPE_CLEARPASSWORD, lc, item, index,
                                   pass);
          break;
        default:
          my_config->StoreResource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_CATALOG:
      my_config->StoreResource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
    default:
      my_config->StoreResource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
  }
}

/**
 * Store ACL (access control list)
 */
static void StoreAcl(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token;
  alist* list;

  if (pass == 1) {
    if (!item->alistvalue[item->code]) {
      item->alistvalue[item->code] = New(alist(10, owned_by_alist));
      Dmsg1(900, "Defined new ACL alist at %d\n", item->code);
    }
  }
  list = item->alistvalue[item->code];

  for (;;) {
    LexGetToken(lc, BCT_STRING);
    if (pass == 1) {
      list->append(bstrdup(lc->str));
      Dmsg2(900, "Appended to %d %s\n", item->code, lc->str);
    }
    token = LexGetToken(lc, BCT_ALL);
    if (token == BCT_COMMA) { continue; /* get another ACL */ }
    break;
  }
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * Store Audit event.
 */
static void StoreAudit(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token;
  alist* list;

  if (pass == 1) {
    if (!*item->alistvalue) {
      *(item->alistvalue) = New(alist(10, owned_by_alist));
    }
  }
  list = *item->alistvalue;

  for (;;) {
    LexGetToken(lc, BCT_STRING);
    if (pass == 1) { list->append(bstrdup(lc->str)); }
    token = LexGetToken(lc, BCT_ALL);
    if (token == BCT_COMMA) { continue; }
    break;
  }
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}
/**
 * Store a runscript->when in a bit field
 */
static void StoreRunscriptWhen(LEX* lc, ResourceItem* item, int index, int pass)
{
  LexGetToken(lc, BCT_NAME);

  if (Bstrcasecmp(lc->str, "before")) {
    *(item->ui32value) = SCRIPT_Before;
  } else if (Bstrcasecmp(lc->str, "after")) {
    *(item->ui32value) = SCRIPT_After;
  } else if (Bstrcasecmp(lc->str, "aftervss")) {
    *(item->ui32value) = SCRIPT_AfterVSS;
  } else if (Bstrcasecmp(lc->str, "always")) {
    *(item->ui32value) = SCRIPT_Any;
  } else {
    scan_err2(lc, _("Expect %s, got: %s"), "Before, After, AfterVSS or Always",
              lc->str);
  }
  ScanToEol(lc);
}

/**
 * Store a runscript->target
 */
static void StoreRunscriptTarget(LEX* lc,
                                 ResourceItem* item,
                                 int index,
                                 int pass)
{
  LexGetToken(lc, BCT_STRING);

  if (pass == 2) {
    if (bstrcmp(lc->str, "%c")) {
      ((RunScript*)item->value)->SetTarget(lc->str);
    } else if (Bstrcasecmp(lc->str, "yes")) {
      ((RunScript*)item->value)->SetTarget("%c");
    } else if (Bstrcasecmp(lc->str, "no")) {
      ((RunScript*)item->value)->SetTarget("");
    } else {
      BareosResource* res;

      if (!(res = my_config->GetResWithName(R_CLIENT, lc->str))) {
        scan_err3(lc,
                  _("Could not find config Resource %s referenced on line %d "
                    ": %s\n"),
                  lc->str, lc->line_no, lc->line);
      }

      ((RunScript*)item->value)->SetTarget(lc->str);
    }
  }
  ScanToEol(lc);
}

/**
 * Store a runscript->command as a string and runscript->cmd_type as a pointer
 */
static void StoreRunscriptCmd(LEX* lc, ResourceItem* item, int index, int pass)
{
  LexGetToken(lc, BCT_STRING);

  if (pass == 2) {
    Dmsg2(1, "runscript cmd=%s type=%c\n", lc->str, item->code);
    POOLMEM* c = GetPoolMemory(PM_FNAME);
    /*
     * Each runscript command takes 2 entries in commands list
     */
    PmStrcpy(c, lc->str);
    ((RunScript*)item->value)->commands->prepend(c); /* command line */
    ((RunScript*)item->value)
        ->commands->prepend((void*)(intptr_t)item->code); /* command type */
  }
  ScanToEol(lc);
}

static void StoreShortRunscript(LEX* lc,
                                ResourceItem* item,
                                int index,
                                int pass)
{
  LexGetToken(lc, BCT_STRING);
  alist** runscripts = item->alistvalue;

  if (pass == 2) {
    RunScript* script = NewRunscript();
    script->SetJobCodeCallback(job_code_callback_director);

    script->SetCommand(lc->str);
    if (Bstrcasecmp(item->name, "runbeforejob")) {
      script->when = SCRIPT_Before;
      script->SetTarget("");
    } else if (Bstrcasecmp(item->name, "runafterjob")) {
      script->when = SCRIPT_After;
      script->on_success = true;
      script->on_failure = false;
      script->fail_on_error = false;
      script->SetTarget("");
    } else if (Bstrcasecmp(item->name, "clientrunafterjob")) {
      script->when = SCRIPT_After;
      script->on_success = true;
      script->on_failure = false;
      script->fail_on_error = false;
      script->SetTarget("%c");
    } else if (Bstrcasecmp(item->name, "clientrunbeforejob")) {
      script->when = SCRIPT_Before;
      script->SetTarget("%c");
    } else if (Bstrcasecmp(item->name, "runafterfailedjob")) {
      script->when = SCRIPT_After;
      script->on_failure = true;
      script->on_success = false;
      script->fail_on_error = false;
      script->SetTarget("");
    }

    /*
     * Remember that the entry was configured in the short runscript form.
     */
    script->short_form = true;

    if (!*runscripts) { *runscripts = New(alist(10, not_owned_by_alist)); }

    (*runscripts)->append(script);
    script->debug();
  }

  ScanToEol(lc);
}

/**
 * Store a bool in a bit field without modifing hdr
 * We can also add an option to StoreBool to skip hdr
 */
static void StoreRunscriptBool(LEX* lc, ResourceItem* item, int index, int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (Bstrcasecmp(lc->str, "yes") || Bstrcasecmp(lc->str, "true")) {
    *(item->boolvalue) = true;
  } else if (Bstrcasecmp(lc->str, "no") || Bstrcasecmp(lc->str, "false")) {
    *(item->boolvalue) = false;
  } else {
    scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE",
              lc->str); /* YES and NO must not be translated */
  }
  ScanToEol(lc);
}

/**
 * Store RunScript info
 *
 * Note, when this routine is called, we are inside a Job
 * resource.  We treat the RunScript like a sort of
 * mini-resource within the Job resource.
 */
static void StoreRunscript(LEX* lc, ResourceItem* item, int index, int pass)
{
  char* c;
  int token, i, t;
  alist** runscripts = item->alistvalue;

  Dmsg1(200, "StoreRunscript: begin StoreRunscript pass=%i\n", pass);

  token = LexGetToken(lc, BCT_SKIP_EOL);

  if (token != BCT_BOB) {
    scan_err1(lc, _("Expecting open brace. Got %s"), lc->str);
  }
  /*
   * Setting on_success, on_failure, fail_on_error
   */
  res_runscript.ResetDefault();

  if (pass == 2) {
    res_runscript.commands = New(alist(10, not_owned_by_alist));
  }

  while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
    if (token == BCT_EOB) { break; }

    if (token != BCT_IDENTIFIER) {
      scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
    }

    for (i = 0; runscript_items[i].name; i++) {
      if (Bstrcasecmp(runscript_items[i].name, lc->str)) {
        token = LexGetToken(lc, BCT_SKIP_EOL);
        if (token != BCT_EQUALS) {
          scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
        }

        /*
         * Call item handler
         */
        switch (runscript_items[i].type) {
          case CFG_TYPE_RUNSCRIPT_CMD:
            StoreRunscriptCmd(lc, &runscript_items[i], i, pass);
            break;
          case CFG_TYPE_RUNSCRIPT_TARGET:
            StoreRunscriptTarget(lc, &runscript_items[i], i, pass);
            break;
          case CFG_TYPE_RUNSCRIPT_BOOL:
            StoreRunscriptBool(lc, &runscript_items[i], i, pass);
            break;
          case CFG_TYPE_RUNSCRIPT_WHEN:
            StoreRunscriptWhen(lc, &runscript_items[i], i, pass);
            break;
          default:
            break;
        }
        i = -1;
        break;
      }
    }

    if (i >= 0) {
      scan_err1(lc, _("Keyword %s not permitted in this resource"), lc->str);
    }
  }

  if (pass == 2) {
    /*
     * Run on client by default
     */
    if (!res_runscript.target) { res_runscript.SetTarget("%c"); }
    if (!*runscripts) { *runscripts = New(alist(10, not_owned_by_alist)); }
    /*
     * commands list contains 2 values per command
     * - POOLMEM command string (ex: /bin/true)
     * - int command type (ex: SHELL_CMD)
     */
    res_runscript.SetJobCodeCallback(job_code_callback_director);
    while ((c = (char*)res_runscript.commands->pop()) != NULL) {
      t = (intptr_t)res_runscript.commands->pop();
      RunScript* script = new RunScript;
      *script = res_runscript;
      script->command = c;
      script->cmd_type = t;
      /*
       * target is taken from res_runscript,
       * each runscript object have a copy
       */
      script->target = NULL;
      script->SetTarget(res_runscript.target);

      /*
       * Remember that the entry was configured in the short runscript form.
       */
      script->short_form = false;

      (*runscripts)->append(script);
      script->debug();
    }
    delete res_runscript.commands;
    /*
     * setting on_success, on_failure... cleanup target field
     */
    res_runscript.ResetDefault(true);
  }

  ScanToEol(lc);
  SetBit(index, item->static_resource->item_present_);
  ClearBit(index, item->static_resource->inherit_content_);
}

/**
 * callback function for edit_job_codes
 * See ../lib/util.c, function edit_job_codes, for more remaining codes
 *
 * %f = Filesetname
 * %h = Client Address
 * %p = Poolname
 * %w = Write storage
 * %x = Spooling (yes/no)
 * %C = Cloning (yes/no)
 * %D = Director name
 * %V = Volume name(s) (Destination)
 */
extern "C" char* job_code_callback_director(JobControlRecord* jcr,
                                            const char* param)
{
  static char yes[] = "yes";
  static char no[] = "no";

  switch (param[0]) {
    case 'f':
      if (jcr->res.fileset) { return jcr->res.fileset->resource_name_; }
      break;
    case 'h':
      if (jcr->res.client) { return jcr->res.client->address; }
      break;
    case 'p':
      if (jcr->res.pool) { return jcr->res.pool->resource_name_; }
      break;
    case 'w':
      if (jcr->res.write_storage) {
        return jcr->res.write_storage->resource_name_;
      }
      break;
    case 'x':
      return jcr->spool_data ? yes : no;
    case 'C':
      return jcr->cloned ? yes : no;
    case 'D':
      return my_name;
    case 'V':
      if (jcr) {
        /*
         * If this is a migration/copy we need the volume name from the
         * mig_jcr.
         */
        if (jcr->mig_jcr) { jcr = jcr->mig_jcr; }

        if (jcr->VolumeName) {
          return jcr->VolumeName;
        } else {
          return (char*)_("*None*");
        }
      } else {
        return (char*)_("*None*");
      }
      break;
  }

  return NULL;
}

/**
 * callback function for init_resource
 * See ../lib/parse_conf.c, function InitResource, for more generic handling.
 */
static void InitResourceCb(ResourceItem* item, int pass)
{
  switch (pass) {
    case 1:
      switch (item->type) {
        case CFG_TYPE_REPLACE:
          for (int i = 0; ReplaceOptions[i].name; i++) {
            if (Bstrcasecmp(item->default_value, ReplaceOptions[i].name)) {
              *(item->ui32value) = ReplaceOptions[i].token;
            }
          }
          break;
        case CFG_TYPE_AUTHPROTOCOLTYPE:
          for (int i = 0; authprotocols[i].name; i++) {
            if (Bstrcasecmp(item->default_value, authprotocols[i].name)) {
              *(item->ui32value) = authprotocols[i].token;
            }
          }
          break;
        case CFG_TYPE_AUTHTYPE:
          for (int i = 0; authmethods[i].name; i++) {
            if (Bstrcasecmp(item->default_value, authmethods[i].name)) {
              *(item->ui32value) = authmethods[i].token;
            }
          }
          break;
        case CFG_TYPE_POOLTYPE:
          *(item->value) = bstrdup(item->default_value);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

/**
 * callback function for parse_config
 * See ../lib/parse_conf.c, function ParseConfig, for more generic handling.
 */
static void ParseConfigCb(LEX* lc, ResourceItem* item, int index, int pass)
{
  switch (item->type) {
    case CFG_TYPE_AUTOPASSWORD:
      StoreAutopassword(lc, item, index, pass);
      break;
    case CFG_TYPE_ACL:
      StoreAcl(lc, item, index, pass);
      break;
    case CFG_TYPE_AUDIT:
      StoreAudit(lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHPROTOCOLTYPE:
      StoreAuthprotocoltype(lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHTYPE:
      StoreAuthtype(lc, item, index, pass);
      break;
    case CFG_TYPE_DEVICE:
      StoreDevice(lc, item, index, pass);
      break;
    case CFG_TYPE_JOBTYPE:
      StoreJobtype(lc, item, index, pass);
      break;
    case CFG_TYPE_PROTOCOLTYPE:
      StoreProtocoltype(lc, item, index, pass);
      break;
    case CFG_TYPE_LEVEL:
      StoreLevel(lc, item, index, pass);
      break;
    case CFG_TYPE_REPLACE:
      StoreReplace(lc, item, index, pass);
      break;
    case CFG_TYPE_SHRTRUNSCRIPT:
      StoreShortRunscript(lc, item, index, pass);
      break;
    case CFG_TYPE_RUNSCRIPT:
      StoreRunscript(lc, item, index, pass);
      break;
    case CFG_TYPE_MIGTYPE:
      StoreMigtype(lc, item, index, pass);
      break;
    case CFG_TYPE_INCEXC:
      StoreInc(lc, item, index, pass);
      break;
    case CFG_TYPE_RUN:
      StoreRun(lc, item, index, pass);
      break;
    case CFG_TYPE_ACTIONONPURGE:
      StoreActiononpurge(lc, item, index, pass);
      break;
    case CFG_TYPE_POOLTYPE:
      StorePooltype(lc, item, index, pass);
      break;
    default:
      break;
  }
}

/**
 * callback function for print_config
 * See ../lib/res.c, function BareosResource::PrintConfig, for more generic
 * handling.
 */
static void PrintConfigCb(ResourceItem* items,
                          int i,
                          PoolMem& cfg_str,
                          bool hide_sensitive_data,
                          bool inherited)
{
  PoolMem temp;

  switch (items[i].type) {
    case CFG_TYPE_DEVICE: {
      /*
       * Each member of the list is comma-separated
       */
      int cnt = 0;
      BareosResource* res = nullptr;
      alist* list;
      PoolMem res_names;

      list = *(items[i].alistvalue);
      if (list != NULL) {
        Mmsg(temp, "%s = ", items[i].name);
        IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);

        PmStrcpy(res_names, "");
        foreach_alist (res, list) {
          if (cnt) {
            Mmsg(temp, ",\"%s\"", res->resource_name_);
          } else {
            Mmsg(temp, "\"%s\"", res->resource_name_);
          }
          PmStrcat(res_names, temp.c_str());
          cnt++;
        }

        PmStrcat(cfg_str, res_names.c_str());
        PmStrcat(cfg_str, "\n");
      }
      break;
    }
    case CFG_TYPE_RUNSCRIPT:
      Dmsg0(200, "CFG_TYPE_RUNSCRIPT\n");
      PrintConfigRunscript(&items[i], cfg_str);
      break;
    case CFG_TYPE_SHRTRUNSCRIPT:
      /*
       * We don't get here as this type is converted to a CFG_TYPE_RUNSCRIPT
       * when parsed
       */
      break;
    case CFG_TYPE_ACL: {
      int cnt = 0;
      char* value = nullptr;
      alist* list;
      PoolMem acl;

      list = items[i].alistvalue[items[i].code];
      if (list != NULL) {
        Mmsg(temp, "%s = ", items[i].name);
        IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
        foreach_alist (value, list) {
          if (cnt) {
            Mmsg(temp, ",\"%s\"", value);
          } else {
            Mmsg(temp, "\"%s\"", value);
          }
          PmStrcat(acl, temp.c_str());
          cnt++;
        }

        PmStrcat(cfg_str, acl.c_str());
        PmStrcat(cfg_str, "\n");
      }
      break;
    }
    case CFG_TYPE_RUN:
      PrintConfigRun(&items[i], cfg_str);
      break;
    case CFG_TYPE_JOBTYPE: {
      int32_t jobtype = *(items[i].ui32value);

      if (jobtype) {
        for (int j = 0; jobtypes[j].type_name; j++) {
          if (jobtypes[j].job_type == jobtype) {
            Mmsg(temp, "%s = %s\n", items[i].name, jobtypes[j].type_name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_PROTOCOLTYPE: {
      uint32_t protocol = *(items[i].ui32value);

      if (protocol) {
        for (int j = 0; backupprotocols[j].name; j++) {
          if (backupprotocols[j].token == protocol) {
            /*
             * Suppress printing default value.
             */
            if (items[i].flags & CFG_ITEM_DEFAULT) {
              if (Bstrcasecmp(items[i].default_value,
                              backupprotocols[j].name)) {
                break;
              }
            }

            Mmsg(temp, "%s = %s\n", items[i].name, backupprotocols[j].name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_MIGTYPE: {
      int32_t migtype = *(items[i].ui32value);

      if (migtype) {
        for (int j = 0; migtypes[j].type_name; j++) {
          if (migtypes[j].job_type == migtype) {
            Mmsg(temp, "%s = %s\n", items[i].name, migtypes[j].type_name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_REPLACE: {
      uint32_t replace = *(items[i].ui32value);

      if (replace) {
        for (int j = 0; ReplaceOptions[j].name; j++) {
          if (ReplaceOptions[j].token == replace) {
            /*
             * Supress printing default value.
             */
            if (items[i].flags & CFG_ITEM_DEFAULT) {
              if (Bstrcasecmp(items[i].default_value, ReplaceOptions[j].name)) {
                break;
              }
            }

            Mmsg(temp, "%s = %s\n", items[i].name, ReplaceOptions[j].name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_LEVEL: {
      uint32_t level = *(items[i].ui32value);

      if (level) {
        for (int j = 0; joblevels[j].level_name; j++) {
          if (joblevels[j].level == level) {
            Mmsg(temp, "%s = %s\n", items[i].name, joblevels[j].level_name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_ACTIONONPURGE: {
      uint32_t action = *(items[i].ui32value);

      if (action) {
        for (int j = 0; ActionOnPurgeOptions[j].name; j++) {
          if (ActionOnPurgeOptions[j].token == action) {
            Mmsg(temp, "%s = %s\n", items[i].name,
                 ActionOnPurgeOptions[j].name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_AUTHPROTOCOLTYPE: {
      uint32_t authprotocol = *(items[i].ui32value);

      if (authprotocol) {
        for (int j = 0; authprotocols[j].name; j++) {
          if (authprotocols[j].token == authprotocol) {
            Mmsg(temp, "%s = %s\n", items[i].name, authprotocols[j].name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_AUTHTYPE: {
      uint32_t authtype = *(items[i].ui32value);

      if (authtype) {
        for (int j = 0; authmethods[j].name; j++) {
          if (authprotocols[j].token == authtype) {
            Mmsg(temp, "%s = %s\n", items[i].name, authmethods[j].name);
            IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_AUDIT: {
      /*
       * Each member of the list is comma-separated
       */
      int cnt = 0;
      char* audit_event = nullptr;
      alist* list;
      PoolMem audit_events;

      list = *(items[i].alistvalue);
      if (list != NULL) {
        Mmsg(temp, "%s = ", items[i].name);
        IndentConfigItem(cfg_str, 1, temp.c_str(), inherited);

        PmStrcpy(audit_events, "");
        foreach_alist (audit_event, list) {
          if (cnt) {
            Mmsg(temp, ",\"%s\"", audit_event);
          } else {
            Mmsg(temp, "\"%s\"", audit_event);
          }
          PmStrcat(audit_events, temp.c_str());
          cnt++;
        }

        PmStrcat(cfg_str, audit_events.c_str());
        PmStrcat(cfg_str, "\n");
      }
      break;
    }
    case CFG_TYPE_POOLTYPE:
      Mmsg(temp, "%s = %s\n", items[i].name, *(items[i].value));
      IndentConfigItem(cfg_str, 1, temp.c_str());
      break;
    default:
      Dmsg2(200, "%s is UNSUPPORTED TYPE: %d\n", items[i].name, items[i].type);
      break;
  }
}

static void ResetAllClientConnectionHandshakeModes(
    ConfigurationParser& my_config)
{
  BareosResource* p = my_config.GetNextRes(R_CLIENT, nullptr);
  while (p) {
    ClientResource* client = dynamic_cast<ClientResource*>(p);
    if (client) {
      client->connection_successful_handshake_ =
          ClientConnectionHandshakeMode::kUndefined;
    }
    p = my_config.GetNextRes(R_CLIENT, p);
  };
}

static void ConfigReadyCallback(ConfigurationParser& my_config)
{
  CreateAndAddUserAgentConsoleResource(my_config);

  std::map<int, std::string> map{
      {R_DIRECTOR, "R_DIRECTOR"}, {R_CLIENT, "R_CLIENT"},
      {R_JOBDEFS, "R_JOBDEFS"},   {R_JOB, "R_JOB"},
      {R_STORAGE, "R_STORAGE"},   {R_CATALOG, "R_CATALOG"},
      {R_SCHEDULE, "R_SCHEDULE"}, {R_FILESET, "R_FILESET"},
      {R_POOL, "R_POOL"},         {R_MSGS, "R_MSGS"},
      {R_COUNTER, "R_COUNTER"},   {R_PROFILE, "R_PROFILE"},
      {R_CONSOLE, "R_CONSOLE"},   {R_DEVICE, "R_DEVICE"}};
  my_config.InitializeQualifiedResourceNameTypeConverter(map);

  ResetAllClientConnectionHandshakeModes(my_config);
}

static bool AddResourceCopyToEndOfChain(int type)
{
  int rindex = type - R_FIRST;
  BareosResource* new_resource = nullptr;

  switch (type) {
    case R_DIRECTOR: {
      DirectorResource* p = new DirectorResource;
      *p = res_dir;
      new_resource = p;
      break;
    }
    case R_CLIENT: {
      ClientResource* p = new ClientResource;
      *p = res_client;
      new_resource = p;
      break;
    }
    case R_JOBDEFS:
    case R_JOB: {
      JobResource* p = new JobResource;
      *p = res_job;
      new_resource = p;
      break;
    }
    case R_STORAGE: {
      StorageResource* p = new StorageResource;
      *p = res_store;
      new_resource = p;
      break;
    }
    case R_CATALOG: {
      CatalogResource* p = new CatalogResource;
      *p = res_cat;
      new_resource = p;
      break;
    }
    case R_SCHEDULE: {
      ScheduleResource* p = new ScheduleResource;
      *p = res_sch;
      new_resource = p;
      break;
    }
    case R_FILESET: {
      FilesetResource* p = new FilesetResource;
      *p = res_fs;
      new_resource = p;
      break;
    }
    case R_POOL: {
      PoolResource* p = new PoolResource;
      *p = res_pool;
      new_resource = p;
      break;
    }
    case R_MSGS: {
      MessagesResource* p = new MessagesResource;
      *p = res_msgs;
      new_resource = p;
      break;
    }
    case R_COUNTER: {
      CounterResource* p = new CounterResource;
      *p = res_counter;
      new_resource = p;
      break;
    }
    case R_PROFILE: {
      ProfileResource* p = new ProfileResource;
      *p = res_profile;
      new_resource = p;
      break;
    }
    case R_CONSOLE: {
      ConsoleResource* p = new ConsoleResource;
      *p = res_con;
      new_resource = p;
      break;
    }
    case R_DEVICE: {
      DeviceResource* p = new DeviceResource;
      *p = res_dev;
      new_resource = p;
      break;
    }
    default:
      Dmsg3(100, "Unhandled resource type: %d\n", type);
      return false;
  }
  return my_config->AppendToResourcesChain(new_resource, type);
}

/*
 * Create a special Console named "*UserAgent*" with
 * root console password so that incoming console
 * connections can be handled in unique way
 *
 */
static void CreateAndAddUserAgentConsoleResource(ConfigurationParser& my_config)
{
  DirectorResource* dir_resource =
      (DirectorResource*)my_config.GetNextRes(R_DIRECTOR, NULL);
  if (!dir_resource) { return; }

  new (&res_con) ConsoleResource();
  res_con.password_.encoding = dir_resource->password_.encoding;
  res_con.password_.value = bstrdup(dir_resource->password_.value);
  res_con.tls_enable_ = true;
  res_con.resource_name_ = bstrdup("*UserAgent*");
  res_con.description_ = bstrdup("root console definition");
  res_con.rcode_ = 1013;
  res_con.refcnt_ = 1;

  AddResourceCopyToEndOfChain(R_CONSOLE);
}

ConfigurationParser* InitDirConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, InitResourceCb, ParseConfigCb,
      PrintConfigCb, exit_code, R_FIRST, R_LAST, resources, res_head,
      default_config_filename.c_str(), "bareos-dir.d", ConfigReadyCallback,
      SaveResource, DumpResource, FreeResource);
  if (config) { config->r_own_ = R_DIRECTOR; }
  return config;
}


/**
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
  UaContext* ua = (UaContext*)sock;

  if (!res) {
    sendit(sock, _("No %s resource defined\n"), my_config->ResToStr(type));
    return;
  }

  if (type < 0) { /* no recursion */
    type = -type;
    recurse = false;
  }

  if (ua && !ua->IsResAllowed(res)) { goto bail_out; }

  switch (type) {
    case R_DIRECTOR:
    case R_PROFILE:
    case R_CONSOLE:
    case R_COUNTER:
    case R_CLIENT:
    case R_DEVICE:
    case R_STORAGE:
    case R_CATALOG:
    case R_JOBDEFS:
    case R_JOB:
    case R_SCHEDULE:
    case R_POOL:
      res->PrintConfig(buf, *my_config, hide_sensitive_data, verbose);
      sendit(sock, "%s", buf.c_str());
      break;
    case R_FILESET: {
      FilesetResource* p = dynamic_cast<FilesetResource*>(res);
      ASSERT(p);  // Ueb --> Fehlermeldung
      p->PrintConfig(buf, *my_config, hide_sensitive_data, verbose);
      sendit(sock, "%s", buf.c_str());
      break;
    }
    case R_MSGS: {
      MessagesResource* p = dynamic_cast<MessagesResource*>(res);
      ASSERT(p);  // Ueb --> Fehlermeldung
      p->PrintConfig(buf, *my_config, hide_sensitive_data, verbose);
      sendit(sock, "%s", buf.c_str());
      break;
    }
    default:
      sendit(sock, _("Unknown resource type %d in DumpResource.\n"), type);
      break;
  }

bail_out:
  if (recurse && res->next_) {
    DumpResource(type, res->next_, sendit, sock, hide_sensitive_data, verbose);
  }
}

static void FreeResource(BareosResource* res, int type)
{
  if (!res) return;

  if (res->resource_name_) {
    free(res->resource_name_);
    res->resource_name_ = nullptr;
  }
  if (res->description_) {
    free(res->description_);
    res->description_ = nullptr;
  }

  BareosResource* next_resource = res->next_;

  switch (type) {
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(res);
      if (p->working_directory) { free(p->working_directory); }
      if (p->scripts_directory) { free(p->scripts_directory); }
      if (p->plugin_directory) { free(p->plugin_directory); }
      if (p->plugin_names) { delete p->plugin_names; }
      if (p->pid_directory) { free(p->pid_directory); }
      if (p->subsys_directory) { free(p->subsys_directory); }
      if (p->backend_directories) { delete p->backend_directories; }
      if (p->password_.value) { free(p->password_.value); }
      if (p->query_file) { free(p->query_file); }
      if (p->DIRaddrs) { FreeAddresses(p->DIRaddrs); }
      if (p->DIRsrc_addr) { FreeAddresses(p->DIRsrc_addr); }
      if (p->verid) { free(p->verid); }
      if (p->keyencrkey.value) { free(p->keyencrkey.value); }
      if (p->audit_events) { delete p->audit_events; }
      if (p->secure_erase_cmdline) { free(p->secure_erase_cmdline); }
      if (p->log_timestamp_format) { free(p->log_timestamp_format); }
      delete p;
      break;
    }
    case R_DEVICE: {
      DeviceResource* p = dynamic_cast<DeviceResource*>(res);
      delete p;
      break;
    }
    case R_COUNTER: {
      CounterResource* p = dynamic_cast<CounterResource*>(res);
      delete p;
      break;
    }
    case R_PROFILE: {
      ProfileResource* p = dynamic_cast<ProfileResource*>(res);
      for (int i = 0; i < Num_ACL; i++) {
        if (p->ACL_lists[i]) {
          delete p->ACL_lists[i];
          p->ACL_lists[i] = NULL;
        }
      }
      delete p;
      break;
    }
    case R_CONSOLE: {
      ConsoleResource* p = dynamic_cast<ConsoleResource*>(res);
      if (p->password_.value) { free(p->password_.value); }
      if (p->profiles) { delete p->profiles; }
      for (int i = 0; i < Num_ACL; i++) {
        if (p->ACL_lists[i]) {
          delete p->ACL_lists[i];
          p->ACL_lists[i] = NULL;
        }
      }
      delete p;
      break;
    }
    case R_CLIENT: {
      ClientResource* p = dynamic_cast<ClientResource*>(res);
      if (p->address) { free(p->address); }
      if (p->lanaddress) { free(p->lanaddress); }
      if (p->username) { free(p->username); }
      if (p->password_.value) { free(p->password_.value); }
      if (p->rcs) { free(p->rcs); }
      delete p;
      break;
    }
    case R_STORAGE: {
      StorageResource* p = dynamic_cast<StorageResource*>(res);
      if (p->address) { free(p->address); }
      if (p->lanaddress) { free(p->lanaddress); }
      if (p->username) { free(p->username); }
      if (p->password_.value) { free(p->password_.value); }
      if (p->media_type) { free(p->media_type); }
      if (p->ndmp_changer_device) { free(p->ndmp_changer_device); }
      if (p->device) { delete p->device; }
      if (p->runtime_storage_status) {
        if (p->runtime_storage_status->vol_list) {
          if (p->runtime_storage_status->vol_list->contents) {
            vol_list_t* vl;

            foreach_dlist (vl, p->runtime_storage_status->vol_list->contents) {
              if (vl->VolName) { free(vl->VolName); }
            }
            p->runtime_storage_status->vol_list->contents->destroy();
            delete p->runtime_storage_status->vol_list->contents;
          }
          free(p->runtime_storage_status->vol_list);
        }
        pthread_mutex_destroy(&p->runtime_storage_status->changer_lock);
        pthread_mutex_destroy(&p->runtime_storage_status->ndmp_deviceinfo_lock);
        delete p->runtime_storage_status;
      }
      delete p;
      break;
    }
    case R_CATALOG: {
      CatalogResource* p = dynamic_cast<CatalogResource*>(res);
      if (p->db_address) { free(p->db_address); }
      if (p->db_socket) { free(p->db_socket); }
      if (p->db_user) { free(p->db_user); }
      if (p->db_name) { free(p->db_name); }
      if (p->db_driver) { free(p->db_driver); }
      if (p->db_password.value) { free(p->db_password.value); }
      delete p;
      break;
    }
    case R_FILESET: {
      FilesetResource* p = dynamic_cast<FilesetResource*>(res);
      for (auto q : p->include_items) { FreeIncexe(q); }
      p->include_items.clear();
      for (auto q : p->exclude_items) { FreeIncexe(q); }
      p->exclude_items.clear();
      delete p;
      break;
    }
    case R_POOL: {
      PoolResource* p = dynamic_cast<PoolResource*>(res);
      if (p->pool_type) { free(p->pool_type); }
      if (p->label_format) { free(p->label_format); }
      if (p->cleaning_prefix) { free(p->cleaning_prefix); }
      if (p->storage) { delete p->storage; }
      delete p;
      break;
    }
    case R_SCHEDULE: {
      ScheduleResource* p = dynamic_cast<ScheduleResource*>(res);
      if (p->run) {
        RunResource *nrun, *next;
        nrun = p->run;
        while (nrun) {
          next = nrun->next;
          delete nrun;
          nrun = next;
        }
      }
      delete p;
      break;
    }
    case R_JOBDEFS:
    case R_JOB: {
      JobResource* p = dynamic_cast<JobResource*>(res);
      if (p->backup_format) { free(p->backup_format); }
      if (p->RestoreWhere) { free(p->RestoreWhere); }
      if (p->RegexWhere) { free(p->RegexWhere); }
      if (p->strip_prefix) { free(p->strip_prefix); }
      if (p->add_prefix) { free(p->add_prefix); }
      if (p->add_suffix) { free(p->add_suffix); }
      if (p->RestoreBootstrap) { free(p->RestoreBootstrap); }
      if (p->WriteBootstrap) { free(p->WriteBootstrap); }
      if (p->WriteVerifyList) { free(p->WriteVerifyList); }
      if (p->selection_pattern) { free(p->selection_pattern); }
      if (p->run_cmds) { delete p->run_cmds; }
      if (p->storage) { delete p->storage; }
      if (p->FdPluginOptions) { delete p->FdPluginOptions; }
      if (p->SdPluginOptions) { delete p->SdPluginOptions; }
      if (p->DirPluginOptions) { delete p->DirPluginOptions; }
      if (p->base) { delete p->base; }
      if (p->RunScripts) {
        FreeRunscripts(p->RunScripts);
        delete p->RunScripts;
      }
      if (p->rjs) { free(p->rjs); }
      delete p;
      break;
    }
    case R_MSGS: {
      MessagesResource* p = dynamic_cast<MessagesResource*>(res);
      delete p;
      break;
    }
    default:
      printf(_("Unknown resource type %d in FreeResource.\n"), type);
      break;
  }
  if (next_resource) { my_config->FreeResourceCb_(next_resource, type); }
}

/**
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers because they may not have been defined until
 * later in pass 1.
 */
static bool SaveResource(int type, ResourceItem* items, int pass)
{
  int rindex = type - R_FIRST;
  BareosResource* static_resource = resources[rindex].static_resource_;

  switch (type) {
    case R_JOBDEFS:
      break;
    case R_JOB:
      /*
       * Check Job requirements after applying JobDefs
       * Ensure that the name item is present however.
       */
      if (items[0].flags & CFG_ITEM_REQUIRED) {
        if (!BitIsSet(0, static_resource->item_present_)) {
          Emsg2(M_ERROR, 0,
                _("%s item is required in %s resource, but not found.\n"),
                items[0].name, resources[rindex].name);
          return false;
        }
      }
      break;
    default:
      /*
       * Ensure that all required items are present
       */
      if (!ValidateResource(type, items, static_resource)) { return false; }
      break;
  }

  /*
   * During pass 2 in each "store" routine, we looked up pointers
   * to all the resources referenced in the current resource, now we
   * must copy their addresses from the static record to the allocated
   * record.
   */
  if (pass == 2) {
    bool ret = UpdateResourcePointer(type, items);
    return ret;
  }

  if (!AddResourceCopyToEndOfChain(type)) { return false; }
  return true;
}

} /* namespace directordaemon */
