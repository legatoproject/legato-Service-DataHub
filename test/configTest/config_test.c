//--------------------------------------------------------------------------------------------------
/**
 * @file config_test.c
 *
 * tests for the config.api
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "config_test.h"

//--------------------------------------------------------------------------------------------------
/**
 * Component initializer.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("Config Test Started.");

#if LE_CONFIG_LINUX
   const char *action = le_arg_GetArg(0);
   if (action == NULL)
   {
       LE_FATAL("No action specified");
   }
   else if (strcmp(action, "parser") == 0)
   {
       config_parser_test();
   }
   else if (strcmp(action, "destination") == 0)
   {
       config_destinationPush_test();
   }
   else
   {
       LE_FATAL("unknown action: \"%s\"", action);
   }
#else
   config_destinationPush_test();
#endif
}
