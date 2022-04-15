# Makefile for building the unit tests and run them
# Copyright (C) Sierra Wireless Inc.
# Requires Legato and cmocka - https://cmocka.org

# Default to wp77xx for backwards compatibility.
export LEGATO_TARGET ?= wp77xx

TEST_BUILD_DIR = build/test

# Liblegato information for building unit tests ("3rd party" source code)
LIBLEGATO_INC=-I${LEGATO_ROOT}/framework/include \
	-I${LEGATO_ROOT}/framework/liblegato/ \
	-I${LEGATO_ROOT}/framework/daemons/linux/ \
	-I${LEGATO_ROOT}/build/$(LEGATO_TARGET)/framework/include/ \
	-I${LEGATO_ROOT}/build/$(LEGATO_TARGET)/3rdParty/inc

TEST_CFLAGS= \
            -g \
            -m32 \
            -Wall \
            -Werror
            # -Wextra DataHUb does not compile with -Wextra

# 3rd party compilation options (Legato, libcbor)
TEST_CFLAGS_3RD_PARTY = -g -m32

TEST_LDFLAGS=-lpthread -ldl -lcmocka -lm
LIBLEGATO = $(TEST_BUILD_DIR)/liblegato.a
LIBCBOR = $(TEST_BUILD_DIR)/libcbor.a

LIBLEGATO_SRC=${LEGATO_ROOT}/framework/liblegato/*.c
LIBLEGATO_LINUX_SRC=${LEGATO_ROOT}/framework/liblegato/linux/*.c

LIBLEGATO_OBJ=$(TEST_BUILD_DIR)/liblegato/*.o $(TEST_BUILD_DIR)/liblegato/linux/*.o
$(LIBLEGATO): $(LIBLEGATO_SRC) $(LIBLEGATO_LINUX_SRC)
	mkdir -p $(TEST_BUILD_DIR)/liblegato/
	mkdir -p $(TEST_BUILD_DIR)/liblegato/linux
	mkdir -p $(TEST_BUILD_DIR)/cov
	rm -f $(TEST_BUILD_DIR)/*.o
	cd $(TEST_BUILD_DIR)/liblegato && cc $(TEST_CFLAGS_3RD_PARTY) -c $(LIBLEGATO_SRC) $(LIBLEGATO_INC)
	cd $(TEST_BUILD_DIR)/liblegato/linux && cc $(TEST_CFLAGS_3RD_PARTY) -c $(LIBLEGATO_LINUX_SRC) $(LIBLEGATO_INC)
	ar rcs $(LIBLEGATO) $(LIBLEGATO_OBJ)

DATAHUB_PATH=../../components/dataHub
DATAHUB_JSON_PATH=../../components/json
DATAHUB_JSONFORMATTER_PATH=../../components/jsonFormatter
DATAHUB_PARSER_PATH=../../components/parser
DATAHUB_SRC=$(wildcard $(DATAHUB_PATH)/*.c) $(wildcard $(DATAHUB_JSON_PATH)/*.c) $(wildcard $(DATAHUB_JSONFORMATTER_PATH)/*.c) $(wildcard $(DATAHUB_PARSER_PATH)/*.c)

ADMINTEST_SRC=$(wildcard *.c)

.PHONY: tests clean
tests: $(ADMINTEST_SRC) $(LIBLEGATO)
	cc $(TEST_CFLAGS) -o $(TEST_BUILD_DIR)/admintest $(ADMINTEST_SRC) $(DATAHUB_SRC) $(LIBLEGATO_OBJ) -I. -I$(DATAHUB_PATH) -I$(DATAHUB_JSON_PATH) -I$(DATAHUB_JSONFORMATTER_PATH) -I$(DATAHUB_PARSER_PATH) $(LIBLEGATO_INC) -DUNIT_TEST $(TEST_LDFLAGS)
	build/test/admintest

clean:
	rm -rf build
