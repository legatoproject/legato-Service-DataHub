

/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */

/**
 * @page c_dataHubAdmin Data Hub Admin API
 *
 * @ref admin_interface.h "API Reference"
 *
 * The Data Hub Admin API provides access to administrative functions within the Data Hub,
 * including
 *  - Walking the resource tree to discover what's available
 *  - Configuring data flow routes between resources
 *  - Adding, removing, and reconfiguring filtering and buffering of data
 *  - Setting and clearing overrides on resources
 *  - Setting the default values of resources
 *  - Pushing values to resources anywhere in the resource tree
 *
 *
 * @section c_dataHubAdmin_ResTree The Resource Tree
 *
 * The resource tree is a tree structure (like a file system directory tree), that
 * contains data flow "resources" instead of files.
 *
 * The non-leaf entries in the resource tree are called "namespaces". They each have a list of
 * one or more children.
 *
 * The leaf entries in the resource tree are called "resources".
 * Four kinds of resource can be found in the resource tree:
 *  - Input - an input port from another app to the Data Hub.
 *  - Output - an output port from the Data Hub to another app.
 *  - Observation - filters and/or buffers data
 *  - Placeholder - a placeholder for a yet to be created resource
 *
 * Inputs and Outputs are created by external apps using the @ref c_dataHubIo.  The Inputs and
 * Outputs created by a given app "x" reside under a namespace "/app/x" that is reserved for that
 * app.  See @ref c_dataHubIo for more information on the structure of these parts of the
 * resource tree.
 *
 * Observations are created via the Admin API (this API; see below).
 *
 * Placeholders are created automatically by the Data Hub when resources that have not been
 * created yet are specified as the data sources for other resources or have their attributes or
 * overrides set. More information on Placeholders is provided in later sections.
 *
 *
 * @section c_dataHubAdmin_Pushing Pushing Values to Resources
 *
 * The following functions can be used to push values to any resource in the resource tree:
 *  - admin_PushTrigger() - push a new trigger to the resource
 *  - admin_PushBoolean() - push a new Boolean data sample to the resource
 *  - admin_PushNumeric() - push a new numeric data sample to the resource
 *  - admin_PushString() - push a new string data sample to the resource
 *  - admin_PushJson() - push a new JSON data sample to the resource
 *
 * Values pushed in this way propagate through the system in the same way that they would if
 * they were pushed in by an I/O API client via an Input resource.
 *
 * @note All of these @c Push() functions accept @c 0 as a timestamp, which tells the
 *       Data Hub to generate the timestamp.
 *
 * For example,
 *
 * @code
 *
 * admin_PushNumeric(INPUT_NAME, 0, inputValue);
 *
 * @endcode
 *
 *
 * @section c_dataHubAdmin_Watching Watching Resources
 *
 * It's possible to register for call-backs whenever new data arrives at a given
 * resource using the following functions:
 * - admin_AddTriggerPushHandler() (optionally remove using admin_RemoveTriggerPushHandler())
 * - admin_AddBooleanPushHandler() (optionally remove using admin_RemoveBooleanPushHandler())
 * - admin_AddNumericPushHandler() (optionally remove using admin_RemoveNumericPushHandler())
 * - admin_AddStringPushHandler() (optionally remove using admin_RemoveStringPushHandler())
 * - admin_AddJsonPushHandler() (optionally remove using admin_RemoveJsonPushHandler())
 *
 * For example,
 *
 * @code
 *
 * // Register for notification of updates to the outdoor air temperature.
 * // My function TempUpdateHandler() will be called when new samples are provided by
 * // the outdoor air temperature sensor.
 * admin_NumericPushHandlerRef_t ref;
 * ref = admin_AddNumericPushHandler("/app/weather/outdoorAirTemp/value", TempUpdateHandler, NULL);
 *
 * @endcode
 *
 * The push handler will not be called until the resource receives its first new value following
 * registration of the handler.
 *
 *
 * @section c_dataHubAdmin_Config Configuration
 *
 * The following subsections describe the configuration settings that can be made through the
 * Admin API.
 *
 * Whenever a configuration update is expected to involve changes to more than one setting,
 * admin_StartUpdate() should be called.  This signals to the Data Hub that the configuration may
 * be temporarily in an invalid state, and therefore, to prevent the corruption of buffered data
 * sets and/or the output of unexpected, out-of-range data to apps, the propagation of data samples
 * through Observations that are being changed should be suspended until further notice.
 *
 * When a configuration update is finished, the administrator app must call admin_EndUpdate()
 * to indicate to the Data Hub that the configuration session is finished and therefore it can
 * safely resume normal operations and clean up things (such as non-volatile data buffer
 * backup files) whose associated resources are no longer present in the resource tree.
 *
 * @note Apps can register for notification of the starting and ending of updates using
 *       io_AddUpdateStartEndHandler().
 *
 *
 * @subsection c_dataHubAdmin_Placeholders Placeholders
 *
 * Configuration settings can be set on a resource that does not yet exist, in which case
 * a "Placeholder" resource will be created.  Placeholders serve to hold those configuration
 * settings until the actual resource is created.
 *
 * If there are still configuration settings on an Input or Output when the app that created
 * it calls io_DeleteResource() to delete it, that resource will be converted into a Placeholder
 * for the configuration settings.  Only when all of those settings are removed from the
 * Placeholder will the Placeholder disappear from the resource tree.
 *
 *
 * @subsection c_dataHubAdmin_DataSources Data Sources
 *
 * Each Output resource and Observation can optionally have one data source.  This is the resource
 * from which data will be received.
 *
 * Inputs' only data sources are external apps, so their data sources cannot be configured.
 *
 * Data Sources are configured using the following functions:
 *  - admin_SetSource()
 *  - admin_RemoveSource()
 *
 * When a resource has its data source set to another resource that doesn't yet exist, a
 * Placeholder resource will be created for that source resource.
 *
 * When a resource is deleted from the Resource tree, any other resources that have
 * the deleted resource as their data source will automatically have their data source removed.
 * That is, there's no need to explicitly remove a source from a resource if that source ceases
 * to exist.
 *
 *
 * @subsection c_dataHubAdmin_Observations Observations
 *
 * An Observation is used for filtering and buffering data.  It can be used to reduce the flow
 * of data into an output or to buffer data for later batched delivery or statistical analysis.
 *
 * An Observation is created by calling admin_CreateObs() with the resource tree path of the
 * Observation.
 *
 * Observations are deleted using admin_DeleteObs().
 *
 * The data type of an Observation is determined by the data it receives from its data source.
 *
 * All Observations reside in a dedicated @c /obs/ namespace.  They are not permitted to exist
 * outside of that namespace.  As a convenience, operations on Observations can optionally
 * take a relative path, which will be interpreted as residing under @c /obs/ . For example,
 *
 * @code
 * admin_CreateObs("myObs");
 * @endcode
 *
 * is equivalent to
 *
 * @code
 * admin_CreateObs("/obs/myObs");
 * @endcode
 *
 *
 * @subsubsection c_dataHubAdmin_ObsFiltering Filtering
 *
 * By default, an Observation performs no filtering, accepting all samples it receives from its
 * data source without modification.
 *
 * The Following functions are used to configure an Observation's input filtering criteria:
 *  - admin_SetMinPeriod() - throttles the sample rate
 *  - admin_SetLowLimit() - causes numeric or Boolean samples below a given value to be dropped
 *  - admin_SetHighLimit() - causes numeric or Boolean samples above a given value to be dropped
 *  - admin_SetChangeBy() - add hysteresis for numeric, Boolean, or string samples
 *
 * The Following functions can be used to retrieve these settings:
 *  - admin_GetMinPeriod()
 *  - admin_GetLowLimit()
 *  - admin_GetHighLimit()
 *  - admin_GetChangeBy()
 *
 * For example, to implement a filter that drops values higher than 30.0,
 *
 * @code
 * admin_SetHighLimit(obsPath, 30.0);
 * @endcode
 *
 * All of these criteria can be removed by setting them to NAN (not a number).
 *
 * To be accepted by the Observation a received data sample must pass @b all of the filtering
 * criteria.
 *
 * The filter High and Low limits can be used to implement either
 * - a dead band, where the values inside a given range are dropped, or
 * - a live band, where values inside the range are kept and those outside the range are dropped.
 *
 * To implement a live band, set the Max higher than the Min.
 * To implement a dead band, set the Min higher than the Max.
 * For example,
 *
 * To implement a filter that drops values outside of a "live band" range from -10.0 to 30.0,
 *
 * @code
 * //              L        H
 * // - <xxxxxxxxxx|--------|xxxxxxxxxx> +
 * //       dropped   kept   dropped
 *
 * admin_SetHighLimit(obsPath, 30.0);
 * admin_SetLowLimit(obsPath, -10.0);
 * @endcode
 *
 * To implement a filter that drops values inside of a "dead band" range from -10.0 to 30.0,
 *
 * @code
 * //           H           L
 * // - <-------|xxxxxxxxxxx|-------> +
 * //        kept  dropped  kept
 *
 * admin_SetLowLimit(obsPath, 30.0);
 * admin_SetHighLimit(obsPath, -10.0);
 * @endcode
 *
 * To suppress updates for small jittery changes and only accept updates that are significantly
 * different from the last value accepted by the Observation, use admin_SetFilterChangeBy().
 * For example,
 *
 * @code
 * //       -changeBy   last    +changeBy
 * //           |         |         |
 * // - <-------|xxxxxxxxxxxxxxxxxxx|-------> +
 * //    accepted      dropped      accepted
 *
 * admin_SetChangeBy(obsPath, 0.5);
 * @endcode
 *
 *
 * @subsubsection c_dataHubAdmin_ObsTransforms Transforms
 *
 * Data transforms can be applied to an Observation buffer.  Transforms are operations such as
 * mean, standard deviation, etc.  Data in the buffer is operated on by the transform and the
 * output of this transform is written to the observation value
 *
 * A single function is used to set the transform on an Observation:
 *  - admin_SetTransform(path, type, optionalParameters, parameterCount)
 *
 * The type of transform is determined by the argument to the function:
 *  - OBS_TRANSFORM_TYPE_NONE   - No transform
 *  - OBS_TRANSFORM_TYPE_MEAN   - Mean
 *  - OBS_TRANSFORM_TYPE_STDDEV - Standard Deviation
 *  - OBS_TRANSFORM_TYPE_MAX    - Maximum value in buffer
 *  - OBS_TRANSFORM_TYPE_MIN    - Minimum value in buffer
 *
 * The optional parameters are dependent upon the transform being applied - e.g. Tap coefficients
 * for Z-transforms, such as IIR or FIR filtering.  Currently unused
 *
 * The Following function can be used to retrieve the transform type:
 *  - admin_GetTransform(path)
 *
 * For example, to configure an Observation to report the mean of the values in its buffer:
 *
 * @code
 * admin_SetTransform(obsPath, OBS_TRANSFORM_TYPE_MEAN, NULL, 0);
 * @endcode
 *
 *
 * @subsubsection c_dataHubAdmin_JsonExtraction Extracting Structured JSON Data
 *
 * It's possible to tell an Observation to extract a particular member or array element from
 * JSON type data it receives.
 *
 * JSON syntax is used to specify what part of the JSON object should be extracted.
 *
 * For example, to extract only the vertical (z) axis of the accelerometer readings received
 * by the observation resource at <c>/obs/verticalAccel</c>:
 *
 * @code
 * result = admin_SetJsonExtraction("/obs/verticalAccel", "z");
 * @endcode
 *
 * To extract the third element of a JSON array:
 *
 * @code
 * result = admin_SetJsonExtraction("/obs/foo", "[3]");
 * @endcode
 *
 * To extract the member called "x" in an object in the third element of an array:
 *
 * @code
 * result = admin_SetJsonExtraction("/obs/foo", "[3].x");
 * @endcode
 *
 * If a received value is not JSON type or does not contain the member or array element specified,
 * then no part of the value will be accepted by the Observation.
 *
 * The extraction step is performed before any other filtering step, so it is possible to also
 * set a @c ChangeBy, @c HighLimit, etc. to be applied to the extracted value.
 *
 *
 * @subsubsection c_dataHubAdmin_ObsBuffering Buffering
 *
 * By default, an Observation performs no buffering, throwing away all data older than its current
 * value.
 *
 * The following functions can be used to configure buffering of data samples that pass the
 * Observation's filtering criteria:
 *  - admin_SetBufferMaxCount() - set the buffer size
 *  - admin_SetBufferBackupPeriod() - enable periodic backups of the buffer to non-volatile storage
 *
 * The following functions can be used to read the buffer configuration settings:
 *  - admin_GetBufferMaxCount()
 *  - admin_GetBufferBackupPeriod()
 *
 * If the buffer backup period is set to a non-zero number of seconds, then
 *
 * @warning Backing up a buffer to non-volatile storage can cause wear on the non-volatile memory
 *          device (e.g., flash memory), thereby reducing the service lifetime of the product.
 *          The durability of non-volatile memory devices will vary widely.
 *          More frequent backups and larger backups increase the wear rate.  So, larger backup
 *          periods and smaller buffers are preferred. To prevent permanent hardware failure,
 *          use this feature only when absolutely necessary.
 *
 * If a buffer is backed up to non-volatile storage, that backup will be kept until one of the
 * following things happen:
 *  - the Observation is explicitly deleted using admin_DeleteObs()
 *  - the Data Hub application is uninstalled from the device
 *  - the Observation changes data type (because its data source pushed a different type of data)
 *
 *
 * @subsection c_dataHubAdmin_Defaults Default Values
 *
 * Resources can have default values set for them using one of the following functions:
 *  - admin_SetBooleanDefault() - set a Boolean default value
 *  - admin_SetNumericDefault() - set a numeric default value
 *  - admin_SetStringDefault() - set a string default value
 *  - admin_SetJsonDefault() - set a JSON structure default value
 *
 * When the default value is set for a resource that does not already have a current value,
 * the default value is pushed through the system in the same way it would if one of the
 * @ref c_dataHubAdmin_Pushing "Push functions" were called.
 *
 * If the resource already has a current value, the default value setting does not replace
 * the current value and the default value does not get pushed to other resources, but its
 * presence on a resource will still prevent the resource from getting deleted.
 * See @ref c_dataHubAdmin_CleanUp for more information.
 *
 * The function admin_HasDefault() can be used to check if a resource has a default value.
 *
 * The function admin_GetDefaultDataType() can be used to get the data type of a default value.
 *
 * The following functions can be used to fetch default values:
 *  - admin_GetBooleanDefault() - get the resource's default value, if the data type is Boolean
 *  - admin_GetNumericDefault() - get the resource's default value, if the data type is numeric
 *  - admin_GetStringDefault() - get the resource's default value, if the data type is string
 *  - admin_GetJsonDefault() - get the resource's default value in JSON format (any type)
 *
 * The function admin_RemoveDefault() can be used to remove a default value from a resource.
 *
 *
 * @subsection c_dataHubAdmin_Overrides Overrides
 *
 * Setting an Override on a resource causes the value of any data sample pushed to that resource
 * to be replaced with the value of the Override.  For example, if a temperature sensor Input
 * is having values pushed to it every 10 seconds, and it were overridden to 20.0, then every
 * 10 seconds it will push a new sample with the timestamp received from the sensor app, but
 * with the value replaced with 20.0, regardless what value the sensor app pushed.
 *
 * The following functions are used to set and clear Overrides:
 *  - admin_SetBooleanOverride()
 *  - admin_SetNumericOverride()
 *  - admin_SetStringOverride()
 *  - admin_SetJsonOverride()
 *  - admin_RemoveOverride()
 *
 * admin_HasOverride() can be used to check if an override is currently set on a given resource.
 *
 * @warning If the data type of an override on an Input or Output resource doesn't match the
 *          data type of that resource, then the override will have no effect.
 *
 * admin_GetOverrideDataType() can be used to check the data type of an override.
 *
 * The following functions can be used to fetch override values:
 *  - admin_GetBooleanOverride() - get the resource's override value, if the data type is Boolean
 *  - admin_GetNumericOverride() - get the resource's override value, if the data type is numeric
 *  - admin_GetStringOverride() - get the resource's override value, if the data type is string
 *  - admin_GetJsonOverride() - get the resource's override value in JSON format (any type)
 *
 *
 * See also @ref c_dataHubAdmin_CleanUp.
 *
 *
 * @subsection c_dataHubAdmin_Mandatory Mandatory Outputs
 *
 * It's possible for a connected app (e.g., sensor or actuator) to have configuration settings
 * that are considered mandatory to the operation of that app.  For example, a sensor may not
 * be able to function unless it has received calibration data settings, or even just a sampling
 * period setting.
 *
 * The function admin_IsMandatory() can be called to check whether a given Output resource is
 * "mandatory", meaning that it must have a value before the connected app function will work.
 *
 * @code
 * printf("Resource '%s' is%s mandatory.\n", admin_IsMandatory(resPath) ? "" : " not");
 * @endcode
 *
 *
 * @section c_dataHubAdmin_Discovery Discovery
 *
 * To support discovery of what is currently available and/or configured in the resource tree,
 * the administrator can "walk" the resource tree using the following functions:
 *  - admin_GetFirstChild() - get the name of the first child entry under a given parent entry
 *  - admin_GetNextSibling() - get the next child of a given entry's parent
 *  - admin_GetEntryType() - find out if an entry is a Namespace, Input, Output, Observation,
 *                           or Placeholder.
 *
 * Inspection operations that can be performed on resources (Inputs, Outputs, Observations
 * and Placeholders), but not on Namespace entries are:
 *  - admin_IsOverridden() - find out whether the resource currently has an override in effect
 *  - admin_HasDefault() - find out whether the resource currently has a default value
 *  - admin_GetDefaultDataType() - discover the data type of a resource's default value
 *  - admin_GetBooleanDefault() - get the resource's default value, if the data type is Boolean
 *  - admin_GetNumericDefault() - get the resource's default value, if the data type is numeric
 *  - admin_GetStringDefault() - get the resource's default value, if the data type is string
 *  - admin_GetJsonDefault() - get the resource's default value in JSON format (any type)
 *  - admin_GetSource() - get the resource from which data values will normally be pushed.
 *  - Any of the functions in @ref c_dataHubQuery.
 *
 * @note There is no need for functions like admin_GetBooleanOverride() because if an override is
 *       set (if admin_IsOverridden() returns true), then the current value will be the value of
 *       the override.
 *
 * Inspection functions that can be used with Observations only are:
 *  - admin_GetMinPeriod()
 *  - admin_GetLowLimit()
 *  - admin_GetHighLimit()
 *  - admin_GetChangeBy()
 *  - admin_GetTransform()
 *  - admin_GetBufferMaxCount()
 *  - admin_GetBufferBackupPeriod()
 *
 * Inspection functions that can be used with Outputs only are:
 *  - admin_IsMandatory()
 *
 *
 * @section c_dataHubAdmin_ChangeNotifications Receiving Notifications of Resource Tree Changes
 *
 * To be notified when Resources or Observations are created or deleted, clients can register
 * a callback:
 *  - admin_AddResourceTreeChangeHandler()
 *  - admin_RemoveResourceTreeChangeHandler()
 *
 * Handlers will receive the path of the Resource, whether it has been added or deleted, and
 * its EntryType.
 *
 * @section c_dataHubAdmin_CleanUp Cleaning Up Resources
 *
 * Resource tree entries are cleaned up as follows:
 *
 *  - Input or Output resources are deleted when their app disconnects or calls io_DeleteResource()
 *    on them AND the administrator has removed any settings that it has applied to that I/O
 *    resource (i.e., Defaults, Overrides and Sources).
 *
 *  - Namespaces are deleted when they have no children.
 *
 *  - Placeholders are deleted when all their settings (Defaults, Overrides and Sources) have been
 *    removed by the administrator.
 *
 *  - Observations must be explicitly deleted by the administrator (by calling admin_DeleteObs()).
 *
 *  - The non-volatile buffer backup file for a given Observation is deleted when admin_EndUpdate()
 *    is called while there is no Observation attached to it, or when buffering is disabled backups
 *    are disabled on a given Observation.
 *
 *
 * @section c_dataHubAdmin_MultiClient Multiple Clients
 *
 * While it is technically possible to have multiple clients of this API, it is not advised, as
 * there is no support built into this API for coordination between multiple clients.
 *
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 * @file admin_interface.h
 */

#ifndef ADMIN_INTERFACE_H_INCLUDE_GUARD
#define ADMIN_INTERFACE_H_INCLUDE_GUARD


#include "legato.h"

// Interface specific includes
#include "io_interface.h"

// Internal includes for this interface
#include "admin_common.h"
//--------------------------------------------------------------------------------------------------
/**
 * Type for handler called when a server disconnects.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*admin_DisconnectHandler_t)(void *);

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
void admin_ConnectService
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
le_result_t admin_TryConnectService
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
LE_FULL_API void admin_SetServerDisconnectHandler
(
    admin_DisconnectHandler_t disconnectHandler,
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
void admin_DisconnectService
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Enumerates the different types of entries that can exist in the resource tree.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Enumerates the different operations on a Resource - add and remove.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Enumerates the different types of transforms which can be applied to an observation buffer
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing triggers to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'admin_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing Boolean values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'admin_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing numeric values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'admin_NumericPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing string values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'admin_StringPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing JSON values to an output
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'admin_JsonPush'
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Register a handler, to be called back whenever a Resource is added or removed
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'admin_ResourceTreeChange'
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
le_result_t admin_CreateInput
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
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
void admin_SetJsonExample
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
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
le_result_t admin_CreateOutput
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
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
void admin_DeleteResource
(
    const char* LE_NONNULL path
        ///< [IN] Absolute resource tree path.
);

//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void admin_MarkOptional
(
    const char* LE_NONNULL path
        ///< [IN] Absolute resource tree path.
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a trigger type data sample to a resource.
 *
 * @note If the resource doesn't exist, the push will be ignored.  This will not cause a
 *       Placeholder resource to be created.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_PushTrigger
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
    double timestamp
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now (i.e., generate a timestamp for me).
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a Boolean type data sample to a resource.
 *
 * @note If the resource doesn't exist, the push will be ignored.  This will not cause a
 *       Placeholder resource to be created.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_PushBoolean
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now (i.e., generate a timestamp for me).
    bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric type data sample to a resource.
 *
 * @note If the resource doesn't exist, the push will be ignored.  This will not cause a
 *       Placeholder resource to be created.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_PushNumeric
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now (i.e., generate a timestamp for me).
    double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a string type data sample to a resource.
 *
 * @note If the resource doesn't exist, the push will be ignored.  This will not cause a
 *       Placeholder resource to be created.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_PushString
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now (i.e., generate a timestamp for me).
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON data sample to a resource.
 *
 * @note If the resource doesn't exist, the push will be ignored.  This will not cause a
 *       Placeholder resource to be created.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_PushJson
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now (i.e., generate a timestamp for me).
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
admin_TriggerPushHandlerRef_t admin_AddTriggerPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
    admin_TriggerPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveTriggerPushHandler
(
    admin_TriggerPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
admin_BooleanPushHandlerRef_t admin_AddBooleanPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
    admin_BooleanPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveBooleanPushHandler
(
    admin_BooleanPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
admin_NumericPushHandlerRef_t admin_AddNumericPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
    admin_NumericPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveNumericPushHandler
(
    admin_NumericPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_StringPush'
 */
//--------------------------------------------------------------------------------------------------
admin_StringPushHandlerRef_t admin_AddStringPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
    admin_StringPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_StringPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveStringPushHandler
(
    admin_StringPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
admin_JsonPushHandlerRef_t admin_AddJsonPushHandler
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
    admin_JsonPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveJsonPushHandler
(
    admin_JsonPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Creates a data flow route from one resource to another by setting the data source for the
 * destination resource.  If the destination resource already has a source resource, it will be
 * replaced. Does nothing if the route already exists.
 *
 * Creates Placeholders for any source or destination resource that doesn't yet exist in the
 * resource tree.
 *
 * @note While an Input can have a source configured, it will ignore anything pushed to it
 *       from other resources via that route. Inputs only accept values pushed by the app that
 *       created them or from the administrator pushed directly to them via one of the
 *       @ref c_dataHubAdmin_Pushing "Push functions".
 *
 * @return
 *  - LE_OK if route already existed or new route was successfully created.
 *  - LE_BAD_PARAMETER if one of the paths is invalid.
 *  - LE_DUPLICATE if the addition of this route would result in a loop.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetSource
(
    const char* LE_NONNULL destPath,
        ///< [IN] Absolute path of destination resource.
    const char* LE_NONNULL srcPath
        ///< [IN] Absolute path of source resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetches the data flow source resource from which a given resource expects to receive data
 * samples.
 *
 * @note While an Input can have a source configured, it will ignore anything pushed to it
 *       from other resources via that route. Inputs only accept values pushed by the app that
 *       created them or from the administrator pushed directly to them via one of the
 *       @ref c_dataHubAdmin_Pushing "Push functions".
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_BAD_PARAMETER if the path is invalid.
 *  - LE_NOT_FOUND if the resource doesn't exist or doesn't have a source.
 *  - LE_OVERFLOW if the path of the source resource won't fit in the string buffer provided.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetSource
(
    const char* LE_NONNULL destPath,
        ///< [IN] Absolute path of destination resource.
    char* srcPath,
        ///< [OUT] Absolute path of source resource.
    size_t srcPathSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove the data flow route into a resource.
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveSource
(
    const char* LE_NONNULL destPath
        ///< [IN] Absolute path of destination resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Create an Observation in the /obs/ namespace.
 *
 *  @return
 *  - LE_OK if the observation was created or it already existed.
 *  - LE_BAD_PARAMETER if the path is invalid.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_CreateObs
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete an Observation in the /obs/ namespace.
 */
//--------------------------------------------------------------------------------------------------
void admin_DeleteObs
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum period between data samples accepted by a given Observation.
 *
 * This is used to throttle the rate of data passing into and through an Observation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetMinPeriod
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    double minPeriod
        ///< [IN] The minimum period, in seconds.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum period between data samples accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetMinPeriod
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the highest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetHighLimit
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    double highLimit
        ///< [IN] The highest value in the range, or NAN (not a number) to remove limit.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the highest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetHighLimit
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the lowest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetLowLimit
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    double lowLimit
        ///< [IN] The lowest value in the range, or NAN (not a number) to remove limit.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the lowest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetLowLimit
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * Ignored for trigger types.
 *
 * For all other types, any non-zero value means accept any change, but drop if the same as current.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetChangeBy
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    double change
        ///< [IN] The magnitude, or either zero or NAN (not a number) to remove limit.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetChangeBy
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Perform a transform on an observation's buffered data. Value of the observation will be
 * the output of the transform
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetTransform
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    admin_TransformType_t transformType,
        ///< [IN] Type of transform to apply
    const double* paramsPtr,
        ///< [IN] Optional parameter list
    size_t paramsSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the type of transform currently applied to an Observation.
 *
 * @return The TransformType
 */
//--------------------------------------------------------------------------------------------------
admin_TransformType_t admin_GetTransform
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * If this is set, all non-JSON data will be ignored, and all JSON data that does not contain the
 * the specified object member or array element will also be ignored.
 *
 * To clear, set to an empty string.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetJsonExtraction
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    const char* LE_NONNULL extractionSpec
        ///< [IN] str specifying member/element to extract.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_NOT_FOUND if the resource doesn't exist or doesn't have a JSON extraction specifier set.
 *  - LE_OVERFLOW if the JSON extraction specifier won't fit in the string buffer provided.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetJsonExtraction
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    char* result,
        ///< [OUT] Buffer where result goes if LE_OK returned.
    size_t resultSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of data samples to buffer in a given Observation.  Buffers are FIFO
 * circular buffers. When full, the buffer drops the oldest value to make room for a new addition.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBufferMaxCount
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    uint32_t count
        ///< [IN] The number of samples to buffer (0 = remove setting).
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the buffer size setting for a given Observation.
 *
 * @return The buffer size (in number of samples) or 0 if not set or the Observation does not exist.
 */
//--------------------------------------------------------------------------------------------------
uint32_t admin_GetBufferMaxCount
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum time between backups of an Observation's buffer to non-volatile storage.
 * If the buffer's size is non-zero and the backup period is non-zero, then the buffer will be
 * backed-up to non-volatile storage when it changes, but never more often than this period setting
 * specifies.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBufferBackupPeriod
(
    const char* LE_NONNULL path,
        ///< [IN] Path within the /obs/ namespace.
    uint32_t seconds
        ///< [IN] The minimum number of seconds between backups (0 = disable backups)
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum time between backups of an Observation's buffer to non-volatile storage.
 * See admin_SetBufferBackupPeriod() for more information.
 *
 * @return The buffer backup period (in seconds) or 0 if backups are disabled or the Observation
 *         does not exist.
 */
//--------------------------------------------------------------------------------------------------
uint32_t admin_GetBufferBackupPeriod
(
    const char* LE_NONNULL path
        ///< [IN] Path within the /obs/ namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a Boolean value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBooleanDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a numeric value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetNumericDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a string value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetStringDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a JSON value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetJsonDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Discover whether a given resource has a default value.
 *
 * @return true if there is a default value set, false if not.
 */
//--------------------------------------------------------------------------------------------------
bool admin_HasDefault
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the default value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t admin_GetDefaultDataType
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the default value of a resource, if it is Boolean.
 *
 * @return the default value, or false if not set or set to another data type.
 */
//--------------------------------------------------------------------------------------------------
bool admin_GetBooleanDefault
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the default value, if it is numeric.
 *
 * @return the default value, or NAN (not a number) if not set or set to another data type.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetNumericDefault
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the default value, if it is a string.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 *  - LE_NOT_FOUND if the resource doesn't have a string default value set.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetStringDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the default value, in JSON format.
 *
 * @note This works for any type of default value.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 *  - LE_NOT_FOUND if the resource doesn't have a default value set.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetJsonDefault
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove any default value on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveDefault
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set an override of Boolean type on a given resource.
 *
 * @note Override will be ignored by an Input or Output resource if the override's data type
 *       does not match the data type of the Input or Output.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBooleanOverride
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set an override of numeric type on a given resource.
 *
 * @note Override will be ignored by an Input or Output resource if the override's data type
 *       does not match the data type of the Input or Output.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetNumericOverride
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set an override of string type on a given resource.
 *
 * @note Override will be ignored by an Input or Output resource if the override's data type
 *       does not match the data type of the Input or Output.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetStringOverride
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set an override of JSON type on a given resource.
 *
 * @note Override will be ignored by an Input or Output resource if the override's data type
 *       does not match the data type of the Input or Output.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetJsonOverride
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Find out whether the resource currently has an override set.
 *
 * @return true if the resource has an override, false otherwise.
 *
 * @note It's possible for an Input or Output to have an override set, but not be overridden.
 *       This is because setting an override to a data type that does not match the Input or
 *       Output resource's data type will result in the override being ignored.  Observations
 *       (and Placeholders) have flexible data types, so if they have an override set, they will
 *       definitely be overridden.
 */
//--------------------------------------------------------------------------------------------------
bool admin_HasOverride
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the override value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t admin_GetOverrideDataType
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the override value of a resource, if it is Boolean.
 *
 * @return the override value, or false if not set or set to another data type.
 */
//--------------------------------------------------------------------------------------------------
bool admin_GetBooleanOverride
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the override value, if it is numeric.
 *
 * @return the override value, or NAN (not a number) if not set or set to another data type.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetNumericOverride
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the override value, if it is a string.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 *  - LE_NOT_FOUND if the resource doesn't have a string override value set.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetStringOverride
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the override value, in JSON format.
 *
 * @note This works for any type of override value.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 *  - LE_NOT_FOUND if the resource doesn't have an override value set.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetJsonOverride
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove any override on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveOverride
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the name of the first child entry under a given parent entry in the resource tree.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_OVERFLOW if the buffer provided is too small to hold the child's path.
 *  - LE_NOT_FOUND if the resource doesn't have any children.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetFirstChild
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    char* child,
        ///< [OUT] Absolute path of the first child resource.
    size_t childSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the name of the next child entry under the same parent as a given entry in the resource tree.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_OVERFLOW if the buffer provided is too small to hold the next sibling's path.
 *  - LE_NOT_FOUND if the resource is the last child in its parent's list of children.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetNextSibling
(
    const char* LE_NONNULL path,
        ///< [IN] Absolute path of the resource.
    char* sibling,
        ///< [OUT] Absolute path of the next sibling resource.
    size_t siblingSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Find out what type of entry lives at a given path in the resource tree.
 *
 * @return The entry type. ADMIN_ENTRY_TYPE_NONE if there's no entry at the given path.
 */
//--------------------------------------------------------------------------------------------------
admin_EntryType_t admin_GetEntryType
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if a given resource is a mandatory output.  If so, it means that this is an output resource
 * that must have a value before the related app function will begin working.
 *
 * @return true if a mandatory output, false if it's an optional output or not an output at all.
 */
//--------------------------------------------------------------------------------------------------
bool admin_IsMandatory
(
    const char* LE_NONNULL path
        ///< [IN] Absolute path of the resource.
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_ResourceTreeChange'
 */
//--------------------------------------------------------------------------------------------------
admin_ResourceTreeChangeHandlerRef_t admin_AddResourceTreeChangeHandler
(
    admin_ResourceTreeChangeHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_ResourceTreeChange'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveResourceTreeChangeHandler
(
    admin_ResourceTreeChangeHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Signal to the Data Hub that administrative changes are about to be performed.
 *
 * This will result in call-backs to any handlers registered using io_AddUpdateStartEndHandler().
 */
//--------------------------------------------------------------------------------------------------
void admin_StartUpdate
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Signal to the Data Hub that all pending administrative changes have been applied and that
 * normal operation may resume.
 *
 * This may trigger clean-up actions, such as deleting non-volatile backups of any Observations
 * that do not exist at the time this function is called.
 *
 * This will also result in call-backs to any handlers registered using
 * io_AddUpdateStartEndHandler().
 */
//--------------------------------------------------------------------------------------------------
void admin_EndUpdate
(
    void
);

#endif // ADMIN_INTERFACE_H_INCLUDE_GUARD
