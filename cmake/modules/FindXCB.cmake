# - Try to find libxcb
# Once done this will define
#
#  XCB_FOUND - system has libxcb
#  XCB_LIBRARIES - Link these to use libxcb
#  XCB_INCLUDE_DIR - the libxcb include dir
#  XCB_DEFINITIONS - compiler switches required for using libxcb

# Copyright (c) 2015, Jason McCarver, <slam@parasite.cc>
# Copyright (c) 2008 Helio Chissini de Castro, <helio@kde.org>
# Copyright (c) 2007, Matthias Kretz, <kretz@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products 
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


IF (NOT WIN32)
  IF (XCB_INCLUDE_DIR AND XCB_LIBRARIES)
    # in cache already
    SET(XCB_FIND_QUIETLY TRUE)
  ENDIF (XCB_INCLUDE_DIR AND XCB_LIBRARIES)

  FIND_PACKAGE(PkgConfig)
  PKG_CHECK_MODULES(PKG_XCB xcb xcb-util xcb-ewmh x11-xcb)

  SET(XCB_DEFINITIONS ${PKG_XCB_CFLAGS})

  include(FindPackageHandleStandardArgs)

  FIND_PATH(XCB_INCLUDE_DIR xcb/xcb.h ${PKG_XCB_INCLUDE_DIRS})

  #FIND_LIBRARY(XCB_LIBRARIES NAMES xcb libxcb PATHS ${PKG_XCB_LIBRARY_DIRS})

  FIND_LIBRARY(XCB_LIBRARY NAMES xcb libxcb PATHS ${PKG_XCB_LIBRARY_DIRS})
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XCB DEFAULT_MSG XCB_LIBRARY XCB_INCLUDE_DIR)

  FIND_LIBRARY(X11XCB_LIBRARY NAMES X11-xcb libX11-xcb PATHS ${PKG_XCB_LIBRARY_DIRS})
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XCB DEFAULT_MSG X11XCB_LIBRARY XCB_INCLUDE_DIR)

  FIND_LIBRARY(XCBUTIL_LIBRARY NAMES xcb-util libxcb-util PATHS ${PKG_XCB_LIBRARY_DIRS})
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XCB DEFAULT_MSG XCBUTIL_LIBRARY XCB_INCLUDE_DIR)

  FIND_LIBRARY(XCBEWMH_LIBRARY NAMES xcb-ewmh libxcb-ewmh PATHS ${PKG_XCB_LIBRARY_DIRS})
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XCB DEFAULT_MSG XCBEWMH_LIBRARY XCB_INCLUDE_DIR)

  SET(XCB_LIBRARIES ${XCB_LIBRARY} ${X11XCB_LIBRARY} ${XCBUTIL_LIBRARY} ${XCBEWMH_LIBRARY})
  #message(STATUS "XCB_LIBRARIES = ${XCB_LIBRARIES}")

  #FIND_PACKAGE_HANDLE_STANDARD_ARGS(XCB DEFAULT_MSG XCB_LIBRARIES XCB_INCLUDE_DIR)

  MARK_AS_ADVANCED(XCB_INCLUDE_DIR XCB_LIBRARIES XCBPROC_EXECUTABLE)
ENDIF (NOT WIN32)
