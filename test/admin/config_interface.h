

/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */

/**
 * @page c_dataHubConfig Data Hub Config API
 *
 * @ref config_interface.h "API Reference"
 *
 * @section config_schema Configuration File schema:
 *
 * For JSON encoding, the following schema is expected:
 *
 * {
 *    "o":{                                        // observations
 *        "<observation name>":{                   // name, given to admin_CreateObs
 *                "r":"<path to be observed>",     // source, given to admin_SetSource
 *                "d":"<destination>",             // destination, see bellow.
 *                // Optional Parameters
 *                "p":<period>,                    // minimum period, given to admin_SetMinPeriod
 *                "st":<change by>,                // change by, given to admin_SetChangeBy
 *                "lt":<greater than>,             // high limit, given to admin_SetHighLimit
 *                "gt":<less than>,                // low limit, given to admin_SetLowLimit
 *                "b":<buffer length>,             // maximum buffer count,
 *                                                 // given to admin_SetBufferMaxCount
 *                "f":"<transform name>"           // transform function,
 *                                                 // given to admin_SetTransform, see below.
 *                "s":"<JSON sub-component>"       // json extraction,
 *                                                 // given to admin_SetJsonExtraction
 *            },
 *            ...
 *    },
 *    "s":{                                        // state values
 *        "<resource path>":{                      // absolute path of resource.
 *                "v":"<value>",                   // value, given to admin_Set<type>Default and
 *                                                 // admin_Push<type>, where type can be Boolean,
 *                                                 // Numeric, String or Json.
 *                "dt":"<data type>"               // Used to differentiate strings from
 *                                                 // string-encoded JSON
 *            },
 *            ...
 *    },
 * }
 *
 *
 *
 * States:
 * States are values which are pushed to resources and set as default value of those resources.
 * dataHub will set the value as default using admin_Set*Default functions and then push the value
 * to the resource using admin_Push* APIs. The resource may not exist at the time that the state is
 * being parsed in which case the act of setting the default will create a placeholder resource for
 * it.
 * Return code of both setting the default and pushing the value will be ignored.
 *
 * State Data Type:
 * The data type of a state is first determined by the type of the JSON value for the "v" key. If
 * the type is boolean or numeric, then the data type is assumed to be boolean or numeric
 * respectively. If the type of value is string, then data type is assumed to be string unless the
 * "dt" : "json" pair is also present in the state.
 *
 * Observation Destination:
 * This is the place where the output of an observation will be directed. It can either be external,
 * a key only known to client or internal, a path to a resource within dataHub. If destination
 * string does not begin with "/" , dataHub will consider it external, and if it does, it is assumed
 * to the path to an internal resource. For internal destinations, dataHub will set the source of
 * the path as if calling: admin_SetSource("<destination>", "/obs/<observation name>"). For external
 * strings, dataHub will record the destination string in the observation, to be used later for
 * calling the DestinationPushHandler.
 *
 * Optional Fields in Observation Object:
 * If an optional property is present, it will be set using the appropriate admin_ API. If an
 * optional property is absent the behavior depends on whether this observation already existed or
 * not. If an observation did not exist and is being first created by this configuration file, then
 * no admin_ API will be called for observation properties that are absent from the observation
 * object.
 * If the observation already existed, then a default value will be given to the corresponding
 * admin_ API for each missing property according to below:
 *  for minPeriod,changeBy, lowerThan, and greaterThan: NAN
 *  for bufferMaxCount: 0
 *  for transform: ADMIN_OBS_TRANSFORM_TYPE_NONE
 *  for jsonExtraction: '/0'
 *
 * Observation Transform Name:
 * The below table shows the enum value given to admin_SetTransform depending on the transform name:
 *@verbatim
 ┌────────────────┬────────────────────────────────────────┐
 │Transform String│ value given to  admin_SetTransform     │
 ├────────────────┼────────────────────────────────────────┤
 │                │                                        │
 │     "mean"     │ ADMIN_OBS_TRANSFORM_TYPE_MEAN          │
 │                │                                        │
 ├────────────────┼────────────────────────────────────────┤
 │                │                                        │
 │    "stddev"    │ ADMIN_OBS_TRANSFORM_TYPE_STDDEV        │
 │                │                                        │
 ├────────────────┼────────────────────────────────────────┤
 │                │                                        │
 │      "min"     │ ADMIN_OBS_TRANSFORM_TYPE_MIN           │
 │                │                                        │
 ├────────────────┼────────────────────────────────────────┤
 │                │                                        │
 │      "max"     │ ADMIN_OBS_TRANSFORM_TYPE_MAX           │
 │                │                                        │
 ├────────────────┼────────────────────────────────────────┤
 │                │                                        │
 │ anything else  │ ADMIN_OBS_TRANSFORM_TYPE_NONE          │
 │                │                                        │
 └────────────────┴────────────────────────────────────────┘
@endverbatim
 *
 * Validating the configuration file:
 * The file is validate for:
 *
 *     - Each set of elements are checked for properly formatted JSON
 *     - Resource paths are checked for proper format and namespace
 *     - Other options in each element, such as buffer size, period, data type, etc. are checked
 *     - Observations and state must have all the mandatory fields.
 *
 * Note:
 * String values that hold JSON, like the JSON value for a state, are not validated for valid JSON.
 *
 *
 * Comparing with previously applied configuration files:
 *
 * When applying a new configuration file, current set of observations that are created by a
 * previous configuration file will be compared with observations outlined in the configuration
 * file. Below is the behavior of dataHub in different circumstances. Observations previously
 * created by a configuration file are referred to by "config observation" for simplicity.
 *
 * If a config observation is absent form the current configuration file, it will be marked for
 * removal.
 *
 * Copyright (C) Sierra Wireless Inc. *
 * @file config_interface.h
 */

#ifndef CONFIG_INTERFACE_H_INCLUDE_GUARD
#define CONFIG_INTERFACE_H_INCLUDE_GUARD


#include "legato.h"

// Interface specific includes
#include "io_interface.h"
#include "admin_interface.h"

// Internal includes for this interface
#include "config_common.h"
//--------------------------------------------------------------------------------------------------
/**
 * Type for handler called when a server disconnects.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*config_DisconnectHandler_t)(void *);

//--------------------------------------------------------------------------------------------------
/**
 *
 * Connect the current client thread to the service providing this API. Block until the service is
 * available.
 *
 * For each thread that wants to use this API, either ConnectService or TryConnectService must be
 * called before any other functions in this API.  Normally, ConnectService is automatically called
 * for the main thread, but not for any other thread. For details, see @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void config_ConnectService
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 *
 * Try to connect the current client thread to the service providing this API. Return with an error
 * if the service is not available.
 *
 * For each thread that wants to use this API, either ConnectService or TryConnectService must be
 * called before any other functions in this API.  Normally, ConnectService is automatically called
 * for the main thread, but not for any other thread. For details, see @ref apiFilesC_client.
 *
 * This function is created automatically.
 *
 * @return
 *  - LE_OK if the client connected successfully to the service.
 *  - LE_UNAVAILABLE if the server is not currently offering the service to which the client is
 *    bound.
 *  - LE_NOT_PERMITTED if the client interface is not bound to any service (doesn't have a binding).
 *  - LE_COMM_ERROR if the Service Directory cannot be reached.
 */
//--------------------------------------------------------------------------------------------------
le_result_t config_TryConnectService
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Set handler called when server disconnection is detected.
 *
 * When a server connection is lost, call this handler then exit with LE_FATAL.  If a program wants
 * to continue without exiting, it should call longjmp() from inside the handler.
 */
//--------------------------------------------------------------------------------------------------
LE_FULL_API void config_SetServerDisconnectHandler
(
    config_DisconnectHandler_t disconnectHandler,
    void *contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 *
 * Disconnect the current client thread from the service providing this API.
 *
 * Normally, this function doesn't need to be called. After this function is called, there's no
 * longer a connection to the service, and the functions in this API can't be used. For details, see
 * @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void config_DisconnectService
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Handler to pass the result of a configuration load request back to the caller
 * The result argument may have the following values:
 * - LE_OK            : Configuration was valid and was successfully applied.
 * - LE_FORMAT_ERROR  : Configuration is not valid due to a format error.
 * - LE_BAD_PARAMETER : A parameter in the configuration file is not valid.
 * - LE_FAULT         : An error has happened during the apply phase. Datahub has deleted all
 *  resources marked as configuration.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 ** Callback function for observations in a configuration. The Datahub will call the registered
 ** handler when data is received by an observation AND the observation's destination field in the
 ** configuration matches the destination string which was passed in AddDestinationPushHandler()
 **
 ** @note
 ** - If the configuration for an observation is using JSON extraction, then the path which is
 ** passed to this handler will include the JSON extraction component. E.g. if the configuration
 ** specified an observation on /orp/status/UART1/value with a JSON extraction of "errors", the
 ** resulting path would be: /orp/status/UART1/value/errors
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Causes the Datahub to load a configuration from a file. Any existing configuration will be
 * removed and replaced with the incoming one
 *
 * @note:
 *  If used over RPC, the filePath parameter must be local to the server.
 *
 * @return
 *  - LE_OK           : Configuration successfully loaded
 *  - LE_NOT_FOUND    : Unable to locate or retrieve configuration file.
 *  - LE_UNSUPPORTED  : Configuration encoding format is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t config_Load
(
    const char* LE_NONNULL filePath,
        ///< [IN] Path of configuration file.
    const char* LE_NONNULL encodedType,
        ///< [IN] Type of encoding used in the file
    config_LoadResultHandlerFunc_t callbackPtr,
        ///< [IN] Callback to notify caller of result
        ///< Context (implied)
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
config_DestinationPushHandlerRef_t config_AddDestinationPushHandler
(
    const char* LE_NONNULL destination,
        ///< [IN] Destination for this event(e.g. "store")
    config_DestinationPushHandlerFunc_t callbackPtr,
        ///< [IN] Destination Push Handler
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
void config_RemoveDestinationPushHandler
(
    config_DestinationPushHandlerRef_t handlerRef
        ///< [IN]
);

#endif // CONFIG_INTERFACE_H_INCLUDE_GUARD
