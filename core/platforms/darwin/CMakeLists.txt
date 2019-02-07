#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.


MESSAGE(STATUS  "installing darwin files to ${sysconfdir}")

SET (INSTALLFILES
	files/installer.pmdoc/01destdir.xml
	files/installer.pmdoc/index.xml
	files/org.bareos.bareos-fd.plist
	files/uninstall.command
	installer-gencontents.py
	Makefile
	Makefile-standalone
	README
	resources
	resources/postinstall
	resources/preinstall
	resources/ReadMe.html
)

FOREACH (FILE ${INSTALLFILES})
	INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/${FILE}" DESTINATION "${DESTSTARTDIR}/"
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE)
ENDFOREACH()
