# (C) Copyright 2014 HGST, a Western Digital Company, 
# San Jose Research Center All Rights Reserved
#
# All information contained herein is proprietary and confidential to HGST. 
# Any use, reproduction, or disclosure without written permission to HGST 
# is prohibted. 

AUTOMAKE_OPTIONS = gnu subdir-objects
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = $(subdirs) src
EXTRA_DIST = autogen.sh

check-local:
	@cd src/test && $(MAKE) check

if WITH_PYBIND
pybind:
	@cd src/pybind && $(MAKE) pybind
pybind-clean:
	@cd src/pybind && $(MAKE) clean
endif

if WITH_JAVABIND
javabind:
	@cd src/javabind && $(MAKE) javabind
javabind-clean:
	@cd src/javabind && $(MAKE) clean
endif
