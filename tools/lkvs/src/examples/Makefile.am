# (C) Copyright 2014 HGST, a Western Digital Company, 
# San Jose Research Center All Rights Reserved
#
# All information contained herein is proprietary and confidential to HGST. 
# Any use, reproduction, or disclosure without written permission to HGST 
# is prohibted. 

AUTOMAKE_OPTIONS = gnu subdir-objects
AM_CPPFLAGS = -I../liblkvs -I../../../../include 
bin_PROGRAMS = lkvsput lkvsget lkvsmultiput lkvsmultiget lkvsfileput
lkvsput_SOURCES = lkvsput.cc 
lkvsget_SOURCES = lkvsget.cc 
lkvsmultiput_SOURCES = lkvsmultiput.cc
lkvsmultiget_SOURCES = lkvsmultiget.cc
lkvsfileput_SOURCES = lkvsfileput.cc
LDADD = ../liblkvs/liblkvs.la ../../../../libzbc.la
