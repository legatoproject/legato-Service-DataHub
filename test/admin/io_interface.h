

/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */

/**
 * @page c_dataHubIo Data Hub I/O API
 *
 * @ref io_interface.h "API Reference"
 *
 * This API allows applications to provide data input to the Data Hub and receive data output
 * from the Data Hub.  This can be used to connect sensors and actuators to processing and
 * connectivity apps.
 *
 *
 * @section c_dataHubIo_Resources I/O Resources
 *
 * Clients create I/O "resources" within the Data Hub's resource tree.  Time-stamped
 * data is "pushed" into the Data Hub via "Input" resources and can be received as output from
 * the Data Hub via "Output" resources.
 *
 * Configuration of the routing, filtering, and buffering of this data inside the Data Hub is
 * the responsibility of an "administrator" app, using the @ref c_dataHubAdmin.  Clients of the
 * I/O API don't care about where the data is routed and how it is processed.  They just create
 * their Input and Output resources and send and receive data through those.  This de-couples
 * the I/O apps from the rest of the system, allowing them to be reused in different ways within
 * different systems.
 *
 * Input resources (for pushing input to the Data Hub) are created using io_CreateInput().
 *
 * Output resources (for receiving output from the Data Hub) are created using io_CreateOutput().
 *
 * Both Input and Output resources can be deleted using io_DeleteResource().
 *
 * @note A resource that has been deleted by the I/O API client app may still appear in the
 *       resource tree if the administrator has applied any settings to that resource. The
 *       resource will only disappear from the resource tree when all administrative settings have
 *       been removed *and* the app that created the resource has either deleted the resource or
 *       disconnected from the Data Hub.
 *
 * Each I/O resource has the following attributes:
 * - Path
 * - Data type
 * - Units
 *
 * @code
 *
 * le_result_t result = io_CreateInput("temperature/value", IO_DATA_TYPE_NUMERIC, "degC");
 *
 * @endcode
 *
 *
 * @section c_dataHubIo_Paths Paths
 *
 * The path of a resource is its unique identifier within the Data Hub's resource tree.
 *
 * Each client of the I/O API is provided with its own namespace in the Data Hub's resource tree,
 * so there's no need to worry about naming conflicts or access violations between client apps.
 *
 * For example, an app named "tempSensor" could create an input with the path "temperature/value".
 * This would appear in the global resource tree at "/app/tempSensor/temperature/value".
 * Meanwhile, another app named "heater" could create an output with the same app-local path
 * ("temperature/value"), and it would appear under "/app/heater/temperature/value".
 *
 * An important set of conventions exist for structuring I/O resource paths:
 * - A sensor's main input resource must be called "value".
 * - An actuator's main output resource must be called "enable".
 * - The name of the "value" or "enable" resource's parent is the name of the sensor or actuator.
 * - All output resources under the same parent as a "value" or "enable" resource are for related
 *   settings.
 *
 * Furthermore, some conventions exist for settings related to sensors:
 * - If a sensor has a boolean output resource called "enable" next to (under the same parent as)
 *   its "value" resource, that "enable" resource can be used by administrator apps to disable the
 *   sensor (by setting that output to "false").
 * - If a boolean output resource called "period" appears next to a "value" input resource
 *   then it can be used to tell the sensor to perform periodic sampling.
 * - If a trigger output resource called "trigger" appears next to a "value" input resource
 *   then it can be used to tell the sensor to push a single sample to its "value" input.
 *
 * For example,
 *
 * @verbatim
/app
  |
  +--/airSensor
  |   |
  |   +--/temperature
  |   |   |
  |   |   +--/value = the temperature sensor input
  |   |   |
  |   |   +--/period = an output used to configure the temperature sensor's sampling period
  |   |   |
  |   |   +--/trigger = an output used to immediately trigger a single temperature sensor sample
  |   |   |
  |   |   +--/enable = an output used to enable or disable the temperature sensor
  |   |
  |   +--/humidity
  |       |
  |       +--/value = the humidity sensor input
  |       |
  |       +--/period = an output used to configure the humidity sensor's sampling period
  |       |
  |       +--/enable = an output used to enable or disable the humidity sensor
  |
  +--/lowBattery
  |   |
  |   +--/value = the lowBattery sensor input
  |   |
  |   +--/level = output used to configure the level at which the low battery alarm will trigger
  |
  +--/hvac
      |
      +--/temperature = the HVAC system's temperature setpoint output
      |
      +--/enable = an output used to enable or disable the HVAC system
      |
      +--/fanOn = an output used to control the fan mode (auto or always on)
      |
      +--/coolOff = an output used to control the cooling mode (auto or disabled)
      |
      +--/heatOff = an output used to control the heating mode (auto or disabled)
 @endverbatim
 *
 * @note The @c enable output can be used to coordinate atomic updates to multiple output
 * resources for the same sensor or actuator.  For example, if an analog-to-digital converter (ADC)
 * accepts settings @c adc/min and @c adc/max to configure the scaling of the ADC reading into
 * physical units like "degC" or "%RH", the sensor may produce garbage readings between the time
 * that @c adc/min and @c adc/max are updated.  The sensor can then also provide @c adc/enable,
 * which the admin tool can use to disable the ADC while it is updating @c adc/min and @c adc/max.
 *
 * Paths are not permitted to contain '.', '[', or ']' characters, as those are reserved for
 * specifying members of structured JSON data samples in the @ref c_dataHubAdmin "Admin" and
 * @ref c_dataHubQuery "Query" APIs.
 *
 *
 * @section c_dataHubIo_DataTypes Data Types
 *
 * Data types supported are:
 * - trigger = used to indicate an event that doesn't have any associated value.
 * - Boolean = a Boolean (true or false) value
 * - numeric = a double-precision floating point value.
 * - string = a UTF-8 string value
 * - JSON = a string in JSON format
 *
 * JSON and string Inputs and Outputs can receive any type of data, but other types of
 * Input or Output can only receive one type of data.  E.g., a Boolean sample cannot
 * be pushed to a trigger or numeric resource, and a string cannot be pushed to a Boolean, trigger,
 * or numeric resource.
 *
 * Furthermore, JSON and string type push hander call-back functions can be registered on other
 * types of Outputs, and a type conversion will happen automatically when the data sample is
 * delivered to its consumer.  For example, if io_AddJsonPushHandler() is used to register a JSON
 * Push Handler call-back on a numeric Output, whenever a numeric data sample arrives at that
 * Output, the JSON push handler will be called with a string parameter containing the JSON
 * representation of that numeric sample's value.
 *
 * @subsection c_dataHubIo_DataTypes_JsonExamples JSON Examples
 *
 * When a JSON Input is created, io_SetJsonExample() can be called to provide an example of what a
 * value should look like.  This can be retrieved by the administrative app via a call to
 * admin_GetJsonExample(), and allows the administrator to see (via an HMI of some kind) what a
 * value might look like before the sensor is enabled.  This assists in the configuration of
 * @ref c_dataHubAdmin_JsonExtraction "JSON extraction" before going live with data collection.
 *
 * @code
 *
 * io_SetJsonExample("accel/value", "{\"x\": 0, \"y\": 0, \"z\": 0}");
 *
 * @endcode
 *
 *
 * @section c_dataHubIo_Units Units
 *
 * Scalar data values have units, such as degrees Celcius, Pascals, Hertz, etc.  Defects can arise
 * if the sender and receiver of a data sample disagree on their units.  For example,
 * https://en.wikipedia.org/wiki/Mars_Climate_Orbiter.
 *
 * When a numeric type I/O resource is created, it can have a string describing its units.
 * If two resources do not agree on their units, data samples will not be routed between them.
 *
 * See the senml RFC draft for a list of units strings in section 12.1 Units Registry at
 * https://tools.ietf.org/html/draft-ietf-core-senml-12#page-26.
 *
 *
 * @section c_dataHubIo_PushingInput Pushing Data Into the Data Hub
 *
 * Data can be pushed to Inputs by calling one of the Push functions:
 * - io_PushTrigger()
 * - io_PushBoolean()
 * - io_PushNumeric()
 * - io_PushString()
 * - io_PushJson()
 *
 * @note All of these @c Push() functions accept @c IO_NOW as a timestamp, which tells the
 *       Data Hub to generate the timestamp.
 *
 * For example,
 *
 * @code
 *
 * io_PushNumeric(INPUT_NAME, IO_NOW, inputValue);
 *
 * @endcode
 *
 *
 * @section c_dataHubIo_ReceivingOutput Receiving Output From the Data Hub
 *
 * Data can be pushed to Outputs by the Data Hub, and clients of the I/O API can register to
 * receive call-backs when this happens:
 * - io_AddTriggerPushHandler() (optionally remove using io_RemoveTriggerPushHandler())
 * - io_AddBooleanPushHandler() (optionally remove using io_RemoveBooleanPushHandler())
 * - io_AddNumericPushHandler() (optionally remove using io_RemoveNumericPushHandler())
 * - io_AddStringPushHandler() (optionally remove using io_RemoveStringPushHandler())
 * - io_AddJsonPushHandler() (optionally remove using io_RemoveJsonPushHandler())
 *
 * For example,
 *
 * @code
 *
 * // This is my enable/disable control.
 * result = io_CreateOutput(ENABLE_NAME, IO_DATA_TYPE_BOOLEAN, "");
 * LE_ASSERT(result == LE_OK);
 *
 * // Register for notification of updates to my enable/disable control.
 * // My function EnableUpdateHandler() will be called when updates arrive for my enable/disable
 * // control.
 * io_AddBooleanPushHandler(ENABLE_NAME, EnableUpdateHandler, NULL);
 *
 * @endcode
 *
 * When a push handler is registered on a resource that already has a value, the push handler
 * will be called to deliver that current value.  If the resource doesn't have a current value yet,
 * then the push handler will not be called until the resource receives its first value.
 *
 * @note Deleting an Output will automatically remove all Push handler call-backs that have been
 *       registered with that Output.
 *
 * It's also possible to fetch the current value of either an Input or an Output using one of the
 * following functions:
 * - io_GetTimestamp() - Get the timestamp of the current value (works with any data type)
 * - io_GetBoolean() - Get the timestamp and the value. Only works with Boolean I/O resources.
 * - io_GetNumeric() - Get the timestamp and the value. Only works with numeric I/O resources.
 * - io_GetString() - Get the timestamp and the value. Only works with string I/O resources.
 * - io_GetJson() - Get the timestamp and value (works with any data type)
 *
 * @note It's possible for a resource to not have any value. This will be indicated by a return
 *       code.
 *
 * @code
 *
 * double timestamp;
 * bool value;
 *
 * le_result_t result = io_GetBoolean(ENABLE_NAME, &timestamp, &value);
 *
 * if (result == LE_OK)
 * {
 *     printf("I'm currently %s.\n", value ? "enabled" : "disabled");
 * }
 * else
 * {
 *     LE_CRIT("Failed to fetch value of " ENABLE_NAME " (%s).", LE_RESULT_TXT(result));
 * }
 *
 * @endcode
 *
 * @note If you need to ensure that your resource always has a current value, you can set its
 *       default value right after you create it.
 *
 *
 * @subsection c_dataHubIo_Mandatory Mandatory Outputs
 *
 * It's possible for a connected app (e.g., sensor or actuator) to have configuration settings
 * that are considered mandatory to the operation of that app.  For example, a sensor may not
 * be able to function unless it has received calibration data settings, or even just a sampling
 * period setting.
 *
 * By default, all Outputs are considered mandatory, but an Output can be marked "optional"
 * by calling io_MarkOptional().
 *
 * @code
 * io_MarkOptional("temperature/offset");
 * @endcode
 *
 * Alternatively, you can set a default value for your mandatory output.
 *
 * Administrative apps can see which resources are mandatory and which are optional, so warnings
 * or hints can be displayed to developers or maintainers if mandatory Outputs are not given values.
 *
 *
 * @section c_dataHubIo_DefaultValues Setting Default Values
 *
 * It's possible to tell the Data Hub that your Input or Output has a default value.
 * if the resource doesn't already have a default value, the following functions will set it:
 * - io_SetBooleanDefault()
 * - io_SetNumericDefault()
 * - io_SetStringDefault()
 * - io_SetJsonDefault()
 *
 * For example,
 *
 * @code
 *
 * // This is my enable/disable control.
 * result = io_CreateOutput(ENABLE_NAME, IO_DATA_TYPE_BOOLEAN, "");
 * LE_ASSERT(result == LE_OK);
 *
 * // The default is to be disabled.
 * io_SetBooleanDefault(ENABLE_NAME, false);
 *
 * @endcode
 *
 * If the resource already has a default value, then calls to these functions will be quietly
 * ignored.
 *
 *
 * @section c_dataHubIo_ConfigUpdateNotification Getting Notified of Configuration Changes
 *
 * If your app needs to be notified when the Data Hub is about to be reconfigured and/or when
 * it has finished being reconfigured, you can register for notification call-backs using
 * io_AddUpdateStartEndHandler().
 *
 * You might need to do this if you have several configuration settings that can be provided to
 * you by the Data Hub via Output resources, and you need to be certain that they are configured
 * consistently with each other in order to produce correct results.  For example, a sensor might
 * have an offset and a scaling factor that are each delivered to it as separate outputs from the
 * Data Hub.  If sensor readings are taken between the time that the offset is updated and the
 * time that the scaling factor is updated, the sensor will produce garbage readings.  Instead,
 * the sensor can get notified that a configuration update is happening, and suspend sampling until
 * the configuration update is finished.
 *
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 * @file io_interface.h
 */

#ifndef IO_INTERFACE_H_INCLUDE_GUARD
#define IO_INTERFACE_H_INCLUDE_GUARD


#include "legato.h"

// Internal includes for this interface
#include "io_common.h"
//--------------------------------------------------------------------------------------------------
/**
 * Type for handler called when a server disconnects.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*io_DisconnectHandler_t)(void *);

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
void io_ConnectService
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
le_result_t io_TryConnectService
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
LE_FULL_API void io_SetServerDisconnectHandler
(
    io_DisconnectHandler_t disconnectHandler,
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
void io_DisconnectService
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Enumerates the data types supported.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing triggers to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing Boolean values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing numeric values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_NumericPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing string values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_StringPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing JSON values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_JsonPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for notification that a Data Hub reconfiguration is beginning or ending.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_UpdateStartEnd'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Create an input resource, which is used to push data into the Data Hub.
 *
 * Does nothing if the resource already exists.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_DUPLICATE if a resource by that name exists but with different direction, type or units.
 *  - LE_NO_MEMORY if the client is not permitted to create that many resources.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_CreateInput
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the client app's resource namespace.
    io_DataType_t dataType,
        ///< [IN] The data type.
    const char* LE_NONNULL units
        ///< [IN] e.g., "degC" (see senml); "" = unspecified.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the example value for a JSON-type Input resource.
 *
 * Does nothing if the resource is not found, is not an input, or doesn't have a JSON type.
 */
//--------------------------------------------------------------------------------------------------
void io_SetJsonExample
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the client app's resource namespace.
    const char* LE_NONNULL example
        ///< [IN] The example JSON value string.
);

//--------------------------------------------------------------------------------------------------
/**
 * Create an output resource, which is used to receive data output from the Data Hub.
 *
 * Does nothing if the resource already exists.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_DUPLICATE if a resource by that name exists but with different direction, type or units.
 *  - LE_NO_MEMORY if the client is not permitted to create that many resources.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_CreateOutput
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the client app's resource namespace.
    io_DataType_t dataType,
        ///< [IN] The data type.
    const char* LE_NONNULL units
        ///< [IN] e.g., "degC" (see senml); "" = unspecified.
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a resource.
 *
 * Does nothing if the resource doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_DeleteResource
(
    const char* LE_NONNULL path
        ///< [IN] Resource path within the client app's namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a trigger type data sample.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_PushTrigger
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double timestamp
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a Boolean type data sample.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_PushBoolean
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
    bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric type data sample.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_PushNumeric
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
    double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a string type data sample.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_PushString
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON data sample.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_PushJson
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
io_TriggerPushHandlerRef_t io_AddTriggerPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    io_TriggerPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
void io_RemoveTriggerPushHandler
(
    io_TriggerPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
io_BooleanPushHandlerRef_t io_AddBooleanPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    io_BooleanPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
void io_RemoveBooleanPushHandler
(
    io_BooleanPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
io_NumericPushHandlerRef_t io_AddNumericPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    io_NumericPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
void io_RemoveNumericPushHandler
(
    io_NumericPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_StringPush'
 */
//--------------------------------------------------------------------------------------------------
io_StringPushHandlerRef_t io_AddStringPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    io_StringPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_StringPush'
 */
//--------------------------------------------------------------------------------------------------
void io_RemoveStringPushHandler
(
    io_StringPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
io_JsonPushHandlerRef_t io_AddJsonPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    io_JsonPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
void io_RemoveJsonPushHandler
(
    io_JsonPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void io_MarkOptional
(
    const char* LE_NONNULL path
        ///< [IN] Resource path within the client app's namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a Boolean type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_SetBooleanDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a numeric type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_SetNumericDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a string type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_SetStringDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a JSON type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_SetJsonDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the timestamp of the current value of an Input or Output resource with any data type.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_GetTimestamp
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double* timestampPtr
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of a Boolean type Input or Output resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_GetBoolean
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
    bool* valuePtr
        ///< [OUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of a numeric type Input or Output resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_GetNumeric
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
    double* valuePtr
        ///< [OUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of a string type Input or Output resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_OVERFLOW if the value buffer was too small to hold the value.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_GetString
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of an Input or Output resource (of any data type) in JSON format.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_OVERFLOW if the value buffer was too small to hold the value.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t io_GetJson
(
    const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
    double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_UpdateStartEnd'
 */
//--------------------------------------------------------------------------------------------------
io_UpdateStartEndHandlerRef_t io_AddUpdateStartEndHandler
(
    io_UpdateStartEndHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_UpdateStartEnd'
 */
//--------------------------------------------------------------------------------------------------
void io_RemoveUpdateStartEndHandler
(
    io_UpdateStartEndHandlerRef_t handlerRef
        ///< [IN]
);

#endif // IO_INTERFACE_H_INCLUDE_GUARD