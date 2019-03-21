/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#include "lib/messages_resource.h"

pthread_mutex_t MessagesResource::fides_mutex = PTHREAD_MUTEX_INITIALIZER;

MessagesResource::MessagesResource()
    : mail_cmd(nullptr)
    , operator_cmd(nullptr)
    , timestamp_format(nullptr)
    , dest_chain(nullptr)
    , SendMsg{0}

    , in_use_(false)
    , closing_(false)
{
  return;
}

MessagesResource::~MessagesResource()
{
  DEST *d, *old;
  for (d = dest_chain; d;) {
    if (d->where) { free(d->where); }
    if (d->mail_cmd) { free(d->mail_cmd); }
    if (d->timestamp_format) { free(d->timestamp_format); }
    old = d;     /* save pointer to release */
    d = d->next; /* point to next buffer */
    free(old);   /* free the destination item */
  }
  dest_chain = NULL;
}

void MessagesResource::lock() { P(fides_mutex); }

void MessagesResource::unlock() { V(fides_mutex); }

void MessagesResource::WaitNotInUse()
{
  /* leaves fides_mutex set */
  lock();
  while (in_use_ || closing_) {
    unlock();
    Bmicrosleep(0, 200);
    lock();
  }
}

void MessagesResource::ClearInUse()
{
  lock();
  in_use_ = false;
  unlock();
}
void MessagesResource::SetInUse()
{
  WaitNotInUse();
  in_use_ = true;
  unlock();
}

void MessagesResource::SetClosing() { closing_ = true; }

bool MessagesResource::GetClosing() { return closing_; }

void MessagesResource::ClearClosing()
{
  lock();
  closing_ = false;
  unlock();
}

bool MessagesResource::IsClosing()
{
  lock();
  bool rtn = closing_;
  unlock();
  return rtn;
}
