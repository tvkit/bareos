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

cmake_minimum_required(VERSION 3.0)
project(bareos-regress)


# set variable values to be replaced by configure_file
set(BAREOS_CONFIG_DIR ${PROJECT_BINARY_DIR})
set(bin ${PROJECT_BINARY_DIR})
set(conf ${PROJECT_BINARY_DIR})
set(confdir ${PROJECT_BINARY_DIR})
set(configs ${PROJECT_BINARY_DIR})
set(dumps ${PROJECT_BINARY_DIR})
set(plugindir ${PROJECT_BINARY_DIR})
set(plugindirtmp ${PROJECT_BINARY_DIR})
set(rscripts ${PROJECT_BINARY_DIR})
set(scripts ${PROJECT_BINARY_DIR})
set(src ${PROJECT_BINARY_DIR})
set(tmp ${PROJECT_BINARY_DIR})
set(tmpsrc ${PROJECT_BINARY_DIR})
set(working ${PROJECT_BINARY_DIR})


MESSAGE (STATUS "BAREOS_CONFIG_DIR: ${BAREOS_CONFIG_DIR}")
MESSAGE (STATUS "scripts: ${scripts}")


function(ConfigureFilesToRegress srcdir subdirectory globexpression configure_option)
  MESSAGE(STATUS "ConfigureFilesToRegress from ${srcdir} to ${subdirectory} with glob ${globexpression} ${configure_option}" )
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${subdirectory})
  file(GLOB_RECURSE ALL_FILES "${CMAKE_SOURCE_DIR}/${srcdir}/${subdirectory}/${globexpression}")
  foreach(CURRENT_FILE ${ALL_FILES})
    string(REGEX REPLACE ".in\$" "" TARGETFILE ${CURRENT_FILE})
    get_filename_component(TARGETFILE ${TARGETFILE} NAME)
    configure_file(${CURRENT_FILE} ${PROJECT_BINARY_DIR}/${subdirectory}/${TARGETFILE} ${configure_option})
    MESSAGE(STATUS "created ${PROJECT_BINARY_DIR}/${subdirectory}/${TARGETFILE}" )
  endforeach()
endfunction()

ConfigureFilesToRegress("core" "scripts" "*.in" @ONLY)
ConfigureFilesToRegress("core/src" "defaultconfigs" "*.in" @ONLY)
ConfigureFilesToRegress("regress"  "tests" "*" @ONLY)
ConfigureFilesToRegress("regress"  "scripts" "functions" @ONLY)
ConfigureFilesToRegress("regress"  "." "config" @ONLY)
ConfigureFilesToRegress("regress"  "data" "*.tgz" COPYONLY)
