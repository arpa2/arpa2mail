#
# CMAKE SETUP (version, build style)
#
# SPDX-License-Identifier: ISC
# License-Filename: LICENSE
cmake_minimum_required (VERSION 3.3)
project ("ARPA2 Service milter" C)

set(CMAKE_C_STANDARD 90)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

#
# XXX don't require ARPA2CM just yet, we're still trying to grasp CMake in
# general.
#

#
# Build the executables and locate libraries
#

# SRC

# In debug configurations, add compile flag.
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_definitions(-Wall -Wextra -pedantic)

find_library(A2ID_LIBRARY NAMES a2id PATHS "/usr/local/lib")
find_library(A2ACL_LIBRARY NAMES a2acl PATHS "/usr/local/lib")
find_library(MILTER_LIBRARY NAMES milter PATHS "/usr/local/lib")

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

target_include_directories(a2aclmilter PRIVATE /usr/local/include)
target_link_libraries(a2aclmilter "${A2ID_LIBRARY}" "${A2ACL_LIBRARY}" "${MILTER_LIBRARY}")

# starting from CMake 3.14, drop DESTINATION and use platform defaults
install(TARGETS a2aclmilter DESTINATION sbin)
# starting from CMake 3.14, use TYPE MAN... instead of DESTINATION
install(FILES a2aclmilter.8 DESTINATION share/man/man8)

#
# PACKAGING
#

set(CPACK_PACKAGE_NAME "ARPA2-ServiceMilter")
set(CPACK_PACKAGE_VERSION ${ARPA2-ServiceMilter_VERSION})
set(CPACK_PACKAGE_VENDOR "ARPA2.net")
set(CPACK_PACKAGE_CONTACT "Tim Kuijsten")
set(CPACK_PACKAGE_LICENSE "ISC")

include(CPack)
