/**
 * @file main.c
 *
 * unit test admin API functions:
 *  CreateInput, CreateOutput, DeleteResource, SetJsonExample and MarkOptional
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>
#include <limits.h>
#include "interfaces.h"

extern void initDataHub(void);

static int setup(void **state) {
    // Init Data Hub component
    initDataHub();

    return 0;
}
static int teardown(void **state) {
    return 0;
}

/* Input resources */
#define TEST_RESOURCES_NB 5
static const char* ResourceName[] = {
    "/app/app1/resource1",
    "/app/appfoo/resourcezzzzz",
    "/app/appfoo/resourcezzzzzz",
    "/app/router/sensor/value",
    "/app/dataOrchestration/trigger/value"
};

static io_DataType_t ResourceType[] = {
    IO_DATA_TYPE_NUMERIC,
    IO_DATA_TYPE_STRING,
    IO_DATA_TYPE_BOOLEAN,
    IO_DATA_TYPE_JSON,
    IO_DATA_TYPE_TRIGGER
};

static const char* ResourceJsonExample[] = {
    "{}",
    "null",
    "[]",
    "{ \"a\" : 456}",
    "{ \"a\" : 456, \"b\" : { \"c\" : {}}}"
};

static void test_admin_create_delete_input
(
    void** state
)
{
    (void)state;
    io_DataType_t dataType = IO_DATA_TYPE_TRIGGER;

    // Create inputs via admin API. Check it returns OK
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_OK == admin_CreateInput(ResourceName[i], ResourceType[i], ""));
    }

    // Check the input entry exists.
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(ADMIN_ENTRY_TYPE_INPUT == admin_GetEntryType(ResourceName[i]));
    }
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_OK == query_GetDataType(ResourceName[i], &dataType));
        assert_true(ResourceType[i] == dataType);
    }

    // Delete the resources
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        admin_DeleteResource(ResourceName[i]);
    }

    // Query them to confirm they are deleted
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(ADMIN_ENTRY_TYPE_NONE == admin_GetEntryType(ResourceName[i]));
    }
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_NOT_FOUND == query_GetDataType(ResourceName[i], &dataType));
    }
}

static void test_admin_create_input_bad_path
(
    void** state
)
{
    (void)state;

    assert_true(LE_FAULT == admin_CreateInput("app/toto/value", IO_DATA_TYPE_NUMERIC, ""));
    assert_true(LE_FAULT == admin_CreateInput("//app/toto/value", IO_DATA_TYPE_NUMERIC, ""));
    assert_true(LE_FAULT == admin_CreateInput("/toto/value", IO_DATA_TYPE_NUMERIC, ""));
    assert_true(LE_FAULT == admin_CreateInput("toto/value", IO_DATA_TYPE_NUMERIC, ""));
}

static void test_admin_create_input_duplicate
(
    void** state
)
{
    (void)state;

    // Create one input resource
    assert_true(LE_OK == admin_CreateInput(ResourceName[3], ResourceType[3], ""));

    // Create it again. Data Hub strategy is to return LE_OK when the resource config is identical.
    assert_true(LE_OK == admin_CreateInput(ResourceName[3], ResourceType[3], ""));

    // Create it again, but with different data type. Data Hub strategy is then to return LE_DUPLICATE
    assert_true(LE_DUPLICATE == admin_CreateInput(ResourceName[3], ResourceType[4], ""));

    // Delete resource to leave the test in a clean state
    admin_DeleteResource(ResourceName[3]);
}

static void test_admin_create_delete_output
(
    void** state
)
{
    (void)state;
    io_DataType_t dataType = IO_DATA_TYPE_TRIGGER;

    // Create inputs via admin API. Check it returns OK
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_OK == admin_CreateOutput(ResourceName[i], ResourceType[i], ""));
    }

    // Check the input entry exists.
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(ADMIN_ENTRY_TYPE_OUTPUT == admin_GetEntryType(ResourceName[i]));
    }
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_OK == query_GetDataType(ResourceName[i], &dataType));
        assert_true(ResourceType[i] == dataType);
    }

    // Delete the resources
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        admin_DeleteResource(ResourceName[i]);
    }

    // Query them to confirm they are deleted
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(ADMIN_ENTRY_TYPE_NONE == admin_GetEntryType(ResourceName[i]));
    }
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_NOT_FOUND == query_GetDataType(ResourceName[i], &dataType));
    }
}

static void test_admin_create_output_bad_path
(
    void** state
)
{
    (void)state;

    assert_true(LE_FAULT == admin_CreateOutput("app/toto/value", IO_DATA_TYPE_NUMERIC, ""));
    assert_true(LE_FAULT == admin_CreateOutput("//app/toto/value", IO_DATA_TYPE_NUMERIC, ""));
    assert_true(LE_FAULT == admin_CreateOutput("/toto/value", IO_DATA_TYPE_NUMERIC, ""));
    assert_true(LE_FAULT == admin_CreateOutput("toto/value", IO_DATA_TYPE_NUMERIC, ""));
}

static void test_admin_create_output_duplicate
(
    void** state
)
{
    (void)state;

    // Create one input resource
    assert_true(LE_OK == admin_CreateOutput(ResourceName[3], ResourceType[3], ""));

    // Create it again. Data Hub strategy is to return LE_OK when the resource config is identical.
    assert_true(LE_OK == admin_CreateOutput(ResourceName[3], ResourceType[3], ""));

    // Create it again, but with different data type. Data Hub strategy is then to return LE_DUPLICATE
    assert_true(LE_DUPLICATE == admin_CreateOutput(ResourceName[3], ResourceType[4], ""));

    // Delete resource to leave the test in a clean state
    admin_DeleteResource(ResourceName[3]);
}

static void test_admin_mark_optional
(
    void** state
)
{
    (void)state;

    // Works only with output
    // If there is a an issue, it calls LE_KILL_CLIENT, test fails.
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_OK == admin_CreateOutput(ResourceName[i], ResourceType[i], ""));
        admin_MarkOptional(ResourceName[i]);
    }

    // Delete resources to leave the test in a clean state
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        admin_DeleteResource(ResourceName[i]);
    }
}

static void test_admin_set_json_example
(
    void** state
)
{
    (void)state;

    // Works only with Input resources.
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        assert_true(LE_OK == admin_CreateInput(ResourceName[i], IO_DATA_TYPE_JSON, ""));
        admin_SetJsonExample(ResourceName[i], ResourceJsonExample[i]);
    }

    // Delete resources to leave the test in a clean state
    for (int i = 0 ; i < TEST_RESOURCES_NB ; i++)
    {
        admin_DeleteResource(ResourceName[i]);
    }
}

static void DummyHandler
(
    double timestamp,
    const char *value,
    void *context
)
{
    (void)timestamp;
    (void)value;
    (void)context;
}

// LE-16330
static void test_admin_create_delete_observation
(
    void** state
)
{
    (void)state;

    admin_JsonPushHandlerRef_t handlerRef;

    // Create an observation
    LE_ASSERT(admin_CreateObs("myObs") == LE_OK);

    // Set a handler
    handlerRef = admin_AddJsonPushHandler("/obs/myObs", DummyHandler, NULL);
    LE_ASSERT(handlerRef);

    // Delete Observation
    admin_DeleteObs("myObs");

    // Delete handler, data hub should not crash
    admin_RemoveJsonPushHandler(handlerRef);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(test_admin_create_delete_input),
        cmocka_unit_test(test_admin_create_input_bad_path),
        cmocka_unit_test(test_admin_create_input_duplicate),
        cmocka_unit_test(test_admin_create_delete_output),
        cmocka_unit_test(test_admin_create_output_bad_path),
        cmocka_unit_test(test_admin_create_output_duplicate),
        cmocka_unit_test(test_admin_mark_optional),
        cmocka_unit_test(test_admin_set_json_example),
        cmocka_unit_test(test_admin_create_delete_observation)
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}
