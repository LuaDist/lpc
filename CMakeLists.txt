# Copyright (C) 2007-2009 LuaDist.
# Created by Peter Kapec
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
# Please note that the package source code is licensed under its own license.

PROJECT(lpc C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
INCLUDE(dist.cmake)

# Build
ADD_LUA_MODULE(lpc lpc.c)

# Install all files and documentation
INSTALL(TARGETS lpc DESTINATION ${INSTALL_CMOD})
INSTALL(FILES COPYING DESTINATION ${INSTALL_DATA})
