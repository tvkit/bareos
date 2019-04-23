/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

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
 * Tray Monitor specific configuration and defines
 *
 * Adapted from dird_conf.c
 *
 * Nicolas Boichat, August MMIV
 */

/* NOTE:  #includes at the end of this file */

/*
 * Resource codes -- they must be sequential for indexing
 */

#ifndef TRAY_CONF_H_INCLUDED
#define TRAY_CONF_H_INCLUDED

#include "lib/tls_conf.h"

extern ConfigurationParser* my_config;

enum Rescode
{
  R_UNKNOWN = 0,
  R_MONITOR = 1001,
  R_DIRECTOR,
  R_CLIENT,
  R_STORAGE,
  R_CONSOLE,
  R_CONSOLE_FONT,
  R_FIRST = R_MONITOR,
  R_LAST = R_CONSOLE_FONT /* keep this updated */
};

/*
 * Some resource attributes
 */
enum
{
  R_NAME = 1020,
  R_ADDRESS,
  R_PASSWORD,
  R_TYPE,
  R_BACKUP
};

/*
 * Director Resource
 */
class DirectorResource
    : public BareosResource
    , public TlsResource {
 public:
  DirectorResource() = default;
  virtual ~DirectorResource() = default;

  void ShallowCopyTo(BareosResource* p) const override
  {
    DirectorResource* r = dynamic_cast<DirectorResource*>(p);
    if (r) { *r = *this; }
  };

  uint32_t DIRport = 0;    /* UA server port */
  char* address = nullptr; /* UA server address */
};

/*
 * Tray Monitor Resource
 */
class MonitorResource
    : public BareosResource
    , public TlsResource {
 public:
  MonitorResource() = default;
  virtual ~MonitorResource() = default;

  void ShallowCopyTo(BareosResource* p) const override
  {
    MonitorResource* r = dynamic_cast<MonitorResource*>(p);
    if (r) { *r = *this; }
  };

  MessagesResource* messages = nullptr; /* Daemon message handler */
  s_password password;                  /* UA server password */
  utime_t RefreshInterval = {0};        /* Status refresh interval */
  utime_t FDConnectTimeout = {0};       /* timeout for connect in seconds */
  utime_t SDConnectTimeout = {0};       /* timeout in seconds */
  utime_t DIRConnectTimeout = {0};      /* timeout in seconds */
};

/*
 * Client Resource
 */
class ClientResource
    : public BareosResource
    , public TlsResource {
 public:
  ClientResource() = default;
  virtual ~ClientResource() = default;

  void ShallowCopyTo(BareosResource* p) const override
  {
    ClientResource* r = dynamic_cast<ClientResource*>(p);
    if (r) { *r = *this; }
  };

  uint32_t FDport = 0; /* Where File daemon listens */
  char* address = nullptr;
  s_password password;
};

/*
 * Store Resource
 */
class StorageResource
    : public BareosResource
    , public TlsResource {
 public:
  StorageResource() = default;
  virtual ~StorageResource() = default;

  void ShallowCopyTo(BareosResource* p) const override
  {
    StorageResource* r = dynamic_cast<StorageResource*>(p);
    if (r) { *r = *this; }
  };

  uint32_t SDport = 0; /* port where Directors connect */
  char* address = nullptr;
  s_password password;
};

class ConsoleFontResource
    : public BareosResource
    , public TlsResource {
 public:
  ConsoleFontResource() = default;
  virtual ~ConsoleFontResource() = default;

  void ShallowCopyTo(BareosResource* p) const override
  {
    ConsoleFontResource* r = dynamic_cast<ConsoleFontResource*>(p);
    if (r) { *r = *this; }
  };

  char* fontface = nullptr; /* Console Font specification */
};

ConfigurationParser* InitTmonConfig(const char* configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem& buffer);

#endif /* TRAY_CONF_H_INCLUDED */
