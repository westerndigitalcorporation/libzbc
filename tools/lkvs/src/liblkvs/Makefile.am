# (C) Copyright 2014 HGST, a Western Digital Company, 
# San Jose Research Center All Rights Reserved
#
# All information contained herein is proprietary and confidential to HGST. 
# Any use, reproduction, or disclosure without written permission to HGST 
# is prohibted. 

AUTOMAKE_OPTIONS = gnu subdir-objects
AM_CPPFLAGS = -I ../../../../include
lib_LTLIBRARIES = liblkvs.la
liblkvs_la_SOURCES = liblkvs.cc sha256.c 
liblkvs_la_LDFLAGS = --version-info 0:0:0
LD_ADD = ../../../../libzbc.la
