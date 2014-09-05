
## LKVS Dev Tests

TESTS =
check_PROGRAMS = 
TESTS += lkvsdev_test
check_PROGRAMS += lkvsdev_test
lkvsdev_test_SOURCES = lkvsdev_test.cc
lkvsdev_test_CPPFLAGS = -I$(top_srcdir)/src/liblkvs \
						-I$(top_srcdir)/gtest/include \
						-I../../../../include
lkvsdev_test_LDFLAGS = -static
lkvsdev_test_LDADD = \
	$(top_srcdir)/gtest/lib/libgtest_main.la \
	$(top_srcdir)/gtest/lib/libgtest.la \
	$(top_srcdir)/src/liblkvs/liblkvs.la \
	../../../../libzbc.la
