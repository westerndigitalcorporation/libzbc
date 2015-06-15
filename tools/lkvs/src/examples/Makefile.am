
AUTOMAKE_OPTIONS = gnu subdir-objects
AM_CPPFLAGS = -I../liblkvs -I../../../../include 
bin_PROGRAMS = lkvsput lkvsget lkvsmultiput lkvsmultiget lkvsfileput
lkvsput_SOURCES = lkvsput.cc 
lkvsget_SOURCES = lkvsget.cc 
lkvsmultiput_SOURCES = lkvsmultiput.cc
lkvsmultiget_SOURCES = lkvsmultiget.cc
lkvsfileput_SOURCES = lkvsfileput.cc
LDADD = ../liblkvs/liblkvs.la ../../../../libzbc.la
