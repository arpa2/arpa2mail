#
# CMAKE SETUP (version, build style)
#
# SPDX-License-Identifier: ISC
# License-Filename: LICENSE
cmake_minimum_required (VERSION 3.3)
project ("ARPA2 Service milter" C)

#set (CMAKE_C_STANDARD 90)
#set (CMAKE_C_STANDARD_REQUIRED ON)

#include (FeatureSummary)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

#
# XXX don't require ARPA2CM just yet, we're still trying to grasp CMake in
# general.
#

#find_package (ARPA2CM 0.5 NO_MODULE)
#set_package_properties (ARPA2CM PROPERTIES
#	DESCRIPTION "CMake modules for ARPA2 projects"
#	TYPE REQUIRED
#	URL "https://github.com/arpa2/arpa2cm/"
#	PURPOSE "Required for the CMake build system of libarpa2service"
#)
#
#if (ARPA2CM_FOUND)
#	set (CMAKE_MODULE_PATH
#		${CMAKE_MODULE_PATH}
#		${CMAKE_SOURCE_DIR}/cmake
#		${ARPA2CM_MODULE_PATH}
#	)
#else()
#        feature_summary (WHAT ALL)
#	message (FATAL_ERROR "ARPA2CM is required.")
#endif()

#include (MacroEnsureOutOfSourceBuild)
#include (MacroAddUninstallTarget)
#include (MacroGitVersionInfo)
#include (MacroCreateConfigFiles)
#include (CMakeDependentOption)

#macro_ensure_out_of_source_build("Do not build Quick-DER in the source directory.")

#
# OPTIONS / BUILD SETTINGS
#
#get_version_from_git (libarpa2service 0.0)

option (NO_TESTING "Disable testing" OFF)
if (NOT NO_TESTING)
	enable_testing()
endif()

#
# Build the executables and locate libraries
#

# SRC

# In debug configurations, add compile flag.
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_definitions(-Wall -Wextra -pedantic)

#find_package(lmdb)

#if (LMDB_FOUND)
#	set(acldb_SRC src/a2acl_dblmdb.c)
#	set(acldb_INCLUDES ${LMDB_INCLUDE_DIR})
#	set(acldb_LIBS ${LMDB_LIBRARY})
#else()
#	set(acldb_SRC src/a2acl_dbm.c)
#	set(acldb_INCLUDES "")
#	set(acldb_LIBS "")
#endif()

find_library(A2ID_LIBRARY NAMES a2id HINTS "${CMAKE_PREFIX_PATH}/usr/local/lib")
find_library(A2ACL_LIBRARY NAMES a2acl HINTS "${CMAKE_PREFIX_PATH}/usr/local/lib")
find_library(MILTER_LIBRARY NAMES milter HINTS "${CMAKE_PREFIX_PATH}/usr/local/lib")

if (A2ID_LIBRARY)
	message(STATUS "${A2ID_LIBRARY}")
else()
	message(FATAL_ERROR "ARPA2 ID library is required.")
endif()

if (A2ACL_LIBRARY)
	message(STATUS "${A2ACL_LIBRARY}")
else()
	message(FATAL_ERROR "ARPA2 ACL library is required.")
endif()

if (MILTER_LIBRARY)
	message(STATUS "${MILTER_LIBRARY}")
else()
	message(FATAL_ERROR "Sendmail milter library is required.")
endif()

add_executable(a2aclmilter a2aclmilter.c util.c)

link_directories(a2aclmilter PRIVATE /usr/local/include)
target_include_directories(a2aclmilter PRIVATE /usr/local/include)
target_link_libraries(a2aclmilter "${A2ID_LIBRARY}" "${A2ACL_LIBRARY}" "${MILTER_LIBRARY}")

# starting from CMake 3.14, drop DESTINATION and use platform defaults
install(TARGETS a2aclmilter DESTINATION sbin)
# starting from CMake 3.14, use TYPE MAN... instead of DESTINATION
install(FILES a2aclmilter.8 DESTINATION share/man/man8)

# TEST

#add_uninstall_target ()

#
# PACKAGING
#

set (CPACK_PACKAGE_NAME "ARPA2-ServiceMilter")
set (CPACK_PACKAGE_VERSION ${ARPA2-ServiceMilter_VERSION})
set (CPACK_PACKAGE_VENDOR "ARPA2.net")
set (CPACK_PACKAGE_CONTACT "Tim Kuijsten")
# License information for packaging. This uses the SPDX license
# identifiers from https://spdx.org/licenses/
set (CPACK_PACKAGE_LICENSE "ISC")

#include (PackAllPossible)
include (CPack)

#
# CMAKE CONFIGURATION FILES
#
# These make it simple to use find_package(Quick-DER) in other
# projects, because Quick-DER can be found (and version information
# obtained) automatically.
#
#TODO_NEED_PC_IN# create_config_files (ARPA2-ServiceLibs)
