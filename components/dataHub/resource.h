//--------------------------------------------------------------------------------------------------
/**
 * Inter-module interfaces provided by the Resource module, which implements the
 * Resource base class and the Placeholder resource class.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef RESOURCE_H_INCLUDE_GUARD
#define RESOURCE_H_INCLUDE_GUARD

#define RES_FLAG_CHANGING_CONFIG    0x80000000  ///< Administrative config update in progress.
#define RES_FLAG_RELEVANT           0x40000000  ///< Resource is relevant to current operation.
#define RES_FLAG_NEW                0x20000000  ///< Node has been created since the last snapshot.
#define RES_FLAG_DELETED            0x10000000  ///< Node has been deleted since deletions were last
                                                ///< flushed.
#define RES_FLAG_CLEAR_NEW          0x08000000  ///< Node with new flag set has been used in current
                                                ///< snapshot, clear its new flag at the end of it.
#define RES_FLAG_JSON_EX_CHANGED    0x04000000  ///< Node JSON example value has been changed since
                                                ///< the last snapshot (only for JSON resources).

#define RES_FLAG_FROM_CONFIG_FILE   0x02000000  ///< This resource was created by a config file.

// Forward declaration needed by res_Resource_t.entryRef.  See resTree.h
typedef struct resTree_Entry* resTree_EntryRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Base class for all types of Resource found in the resource tree.
 *
 * @note Inherit from this by including it as a member in the sub-class struct.
 *
 * @warning DO NOT reference the members of this structure anywhere but this file or resource.c.
 */
//--------------------------------------------------------------------------------------------------
typedef struct res_Resource
{
    resTree_EntryRef_t entryRef;  ///< Reference to the resource tree entry this is attached to.
    char units[HUB_MAX_UNITS_BYTES]; ///< String describing the units, or "" if unspecified.
    io_DataType_t currentType;  ///< Data type of the current value of this resource.
    dataSample_Ref_t currentValue; ///< The current value of this resource; NULL if none yet.
    io_DataType_t pushedType;  ///< Data type of last value pushed to this resource.
    dataSample_Ref_t pushedValue; ///< Last value pushed to resource; NULL if none yet.
    struct res_Resource* srcPtr; ///< Ptr to resource that data samples will normally come from.
    le_dls_List_t destList; ///< List of routes to which data samples should be pushed.
    le_dls_Link_t destListLink; ///< Used to link into another resource's destList.
    dataSample_Ref_t overrideValue;///< Ref to override data sample; NULL if no override in effect.
    io_DataType_t overrideType;///< Data type of the override, if overrideRef != NULL.
    dataSample_Ref_t defaultValue; ///< Ref to default value; NULL if no default set.
    io_DataType_t defaultType;///< Data type of the default value, if defaultRef != NULL.
    uint32_t flags;  ///< Resource status flags.
    le_dls_List_t pushHandlerList;  ///< List of Push Handler callbacks registered on this resource.
    dataSample_Ref_t jsonExample; ///< Ref to JSON example value; NULL if not set.
}
res_Resource_t;


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Resource module.
 *
 * @warning Must be called before any other functions in this module.
 */
//--------------------------------------------------------------------------------------------------
void res_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Constructor for the Resource base class.
 *
 * @warning This is only for use by sub-classes (obs.c and ioPoint.c).
 */
//--------------------------------------------------------------------------------------------------
void res_Construct
(
    res_Resource_t* resPtr,
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the resource tree entry for a given resource.
 *
 * @return reference to the resource tree entry.
 */
//--------------------------------------------------------------------------------------------------
static inline resTree_EntryRef_t res_GetResTreeEntry
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    return resPtr->entryRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create an Input resource object.
 *
 * @return Ptr to the object or NULL if it failed to create an object.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* res_CreateInput
(
    resTree_EntryRef_t entryRef, ///< The entry that this resource will be attached to.
    io_DataType_t dataType,
    const char* units
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an Output resource object.
 *
 * @return Ptr to the object or NULL if it failed to create an object.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* res_CreateOutput
(
    resTree_EntryRef_t entryRef, ///< The entry that this resource will be attached to.
    io_DataType_t dataType,
    const char* units
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an Observation resource object.
 *
 * @return Ptr to the object or NULL if failed to allocate an observation.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* res_CreateObservation
(
    resTree_EntryRef_t entryRef ///< The entry that this resource will be attached to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an IO Placeholder resource object.
 *
 * @return Ptr to the object, or NULL if failed to create one.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* res_CreateIoPlaceholder
(
    resTree_EntryRef_t entryRef ///< The entry that this resource will be attached to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an observation Placeholder resource object.
 *
 * @return Ptr to the object, or NULL if failed to create one.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* res_CreateObsPlaceholder
(
    resTree_EntryRef_t entryRef ///< The entry that this resource will be attached to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Convert a placeholder resource to an Input resource.
 */
//--------------------------------------------------------------------------------------------------
void res_ConvertPlaceholderToInput
(
    res_Resource_t* resPtr,     ///< Pointer to resource
    io_DataType_t dataType,     ///< IO data type
    const char* units           ///< Io resource units
);


//--------------------------------------------------------------------------------------------------
/**
 * Convert a placeholder resource to an Output resource.
 */
//--------------------------------------------------------------------------------------------------
void res_ConvertPlaceholderToOutput
(
    res_Resource_t* resPtr,     ///< Pointer to resource
    io_DataType_t dataType,     ///< IO data type
    const char* units           ///< Io resource units
);


//--------------------------------------------------------------------------------------------------
/**
 * Converts an IO resource that is being deleted to a placeholder.
 */
//--------------------------------------------------------------------------------------------------
void res_ConvertIoToPlaceholder
(
    res_Resource_t* resPtr     ///< Pointer to resource
);


//--------------------------------------------------------------------------------------------------
/**
 * Restore an Observation's data buffer from non-volatile backup, if one exists.
 */
//--------------------------------------------------------------------------------------------------
void res_RestoreBackup
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Destruct a resource object.  This is called by the sub-class's destructor function to destruct
 * the parent class part of the object.
 */
//--------------------------------------------------------------------------------------------------
void res_Destruct
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the Units of a resource.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_OVERFLOW if the units string is too long.
 */
//--------------------------------------------------------------------------------------------------
le_result_t res_SetUnits
(
    res_Resource_t* resPtr,
    const char* units
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the Units of a resource.
 *
 * @return Pointer to the units string (valid as long as the resource exists).
 */
//--------------------------------------------------------------------------------------------------
const char* res_GetUnits
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Find out what data type a given resource currently has.
 *
 * Note that the data type of Inputs and Outputs are set by the app that creates those resources.
 * All other resources will change data types as values are pushed to them.
 *
 * @return the data type.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t res_GetDataType
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource.
 *
 * @return Reference to the Data Sample object or NULL if the resource doesn't have a current value.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t res_GetCurrentValue
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the source resource of a given resource.
 *
 * Does nothing if the route already exists.
 *
 * @return
 *  - LE_OK if route already existed or new route was successfully created.
 *  - LE_DUPLICATE if the addition of this route would result in a loop.
 */
//--------------------------------------------------------------------------------------------------
le_result_t res_SetSource
(
    res_Resource_t* destPtr, ///< [IN] The destination resource.
    res_Resource_t* srcPtr   ///< [IN] The source resource (NULL to clear).
);


//--------------------------------------------------------------------------------------------------
/**
 * Fetches the data flow source resource entry from which a given resource expects to receive data
 * samples.
 *
 * @return Reference to the source entry or NULL if none configured.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t res_GetSource
(
    res_Resource_t* destPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Push a data sample to a resource.
 *
 * @note Takes ownership of the data sample reference.
 *
 * @return
 *      - LE_OK If datasample was pushed successfully.
 *      - LE_NO_MEMORY If failed to push the data sample because of failure in memory allocation.
 *      - LE_IN_PROGRESS Push is rejected because a configuration update is in progress.
 *      - LE_BAD_PARAMETER If there is a mismatch if datasample unit.
 *      - LE_FAULT If any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
le_result_t res_Push
(
    res_Resource_t* resPtr,    ///< The resource to push to.
    io_DataType_t dataType,         ///< The data type.
    const char* units,              ///< The units (NULL or "" = unspecified)
    dataSample_Ref_t dataSample     ///< The data sample (timestamp + value).
);


//--------------------------------------------------------------------------------------------------
/**
 * Add a Push Handler to an Output resource.
 *
 * @return Reference to the handler added. NULL if failed to add handler.
 *
 * @note Can be removed by calling handler_Remove().
 */
//--------------------------------------------------------------------------------------------------
hub_HandlerRef_t res_AddPushHandler
(
    res_Resource_t* resPtr, ///< Ptr to the Output resource.
    io_DataType_t dataType,
    void* callbackPtr,
    void* contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Check whether a given resource has administrative settings.
 *
 * @note Currently ignores Observation-specific settings, because this function isn't needed
 *       for Observations.
 *
 * @return true if there are some administrative settings present, false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool res_HasAdminSettings
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Delete an Observation.
 */
//--------------------------------------------------------------------------------------------------
void res_DeleteObservation
(
    res_Resource_t* obsPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum period between data samples accepted by a given Observation.
 *
 * This is used to throttle the rate of data passing into and through an Observation.
 */
//--------------------------------------------------------------------------------------------------
void res_SetMinPeriod
(
    res_Resource_t* resPtr,
    double minPeriod
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum period between data samples accepted by a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double res_GetMinPeriod
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the highest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void res_SetHighLimit
(
    res_Resource_t* resPtr,
    double highLimit
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the highest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double res_GetHighLimit
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the lowest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void res_SetLowLimit
(
    res_Resource_t* resPtr,
    double lowLimit
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the lowest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double res_GetLowLimit
(
    res_Resource_t* resPtr
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
void res_SetChangeBy
(
    res_Resource_t* resPtr,
    double change
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double res_GetChangeBy
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform a transform on buffered data. Value of the observation will be the output of the
 * transform
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void res_SetTransform
(
    res_Resource_t* resPtr,
    admin_TransformType_t transformType,
    const double* paramsPtr,
    size_t paramsSize
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the type of transform currently applied to an Observation.
 *
 * @return The TransformType
 */
//--------------------------------------------------------------------------------------------------
admin_TransformType_t res_GetTransform
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of data samples to buffer in a given Observation.  Buffers are FIFO
 * circular buffers. When full, the buffer drops the oldest value to make room for a new addition.
 */
//--------------------------------------------------------------------------------------------------
void res_SetBufferMaxCount
(
    res_Resource_t* resPtr,
    uint32_t count
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the buffer size setting for a given Observation.
 *
 * @return The buffer size (in number of samples) or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
uint32_t res_GetBufferMaxCount
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum time between backups of an Observation's buffer to non-volatile storage.
 * If the buffer's size is non-zero and the backup period is non-zero, then the buffer will be
 * backed-up to non-volatile storage when it changes, but never more often than this period setting
 * specifies.
 */
//--------------------------------------------------------------------------------------------------
void res_SetBufferBackupPeriod
(
    res_Resource_t* resPtr,
    uint32_t seconds
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
uint32_t res_GetBufferBackupPeriod
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void res_MarkOptional
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Check if a given resource is a mandatory output.  If so, it means that this is an output resource
 * that must have a value before the related app function will begin working.
 *
 * @return true if a mandatory output, false if it's an optional output or not an output at all.
 */
//--------------------------------------------------------------------------------------------------
bool res_IsMandatory
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource.
 *
 * Will be discarded if setting the default value on an Input or Output that doesn't have the
 * same data type.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If could not set default value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch.
 *      - LE_FAULT Any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t res_SetDefault
(
    res_Resource_t* resPtr,
    io_DataType_t dataType,
    dataSample_Ref_t value  /// Ownership of this reference is passed to the resource.
);


//--------------------------------------------------------------------------------------------------
/**
 * Discover whether a given resource has a default value.
 *
 * @return true if there is a default value set, false if not.
 */
//--------------------------------------------------------------------------------------------------
bool res_HasDefault
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the default value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t res_GetDefaultDataType
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the default value of a resource.
 *
 * @return the default value or NULL if not set.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t res_GetDefaultValue
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove any default value that might be set on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void res_RemoveDefault
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set an override on a given resource.
 *
 * @note Override will be discarded by an Input or Output resource if the override's data type
 *       does not match the data type of the Input or Output.
 *
 * @return
 *      - LE_OK If setting override was successful.
 *      - LE_NO_MEMORY If could not set override value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set override value due to type or unit mismatch.
 *      - LE_FAULT Any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t res_SetOverride
(
    res_Resource_t* resPtr,
    io_DataType_t dataType,
    dataSample_Ref_t value
);


//--------------------------------------------------------------------------------------------------
/**
 * Find out whether the resource currently has an override set.
 *
 * @return true if the resource has an override set, false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool res_HasOverride
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Find out whether the resource currently has an override in effect.
 *
 * @return true if the resource is overridden, false otherwise.
 *
 * @note It's possible for a resource to have an override but not be overridden.  This happens when
 *       the override is the wrong data type for the resource.
 */
//--------------------------------------------------------------------------------------------------
bool res_IsOverridden
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the override value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t res_GetOverrideDataType
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the override value of a resource.
 *
 * @return the override value or NULL if not set.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t res_GetOverrideValue
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove any override that might be in effect for a given resource.
 */
//--------------------------------------------------------------------------------------------------
void res_RemoveOverride
(
    res_Resource_t* resPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the resource's relevance flag.
 */
//--------------------------------------------------------------------------------------------------
void res_SetRelevance
(
    res_Resource_t  *resPtr,    ///< Resource to query.
    bool             relevant   ///< Relevance of resource to current operation.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the resource's relevance flag.
 *
 * @return Relevance of resource to the current operation.
 */
//--------------------------------------------------------------------------------------------------
bool res_IsRelevant
(
    res_Resource_t *resPtr ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the resource's clear newness flag
 */
//--------------------------------------------------------------------------------------------------
void res_SetClearNewnessFlag
(
    res_Resource_t  *resPtr ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the resource's "clear newness" flag.
 *
 * @return Whether the resource "newness" flag must be cleared at the end of current snapshot
 */
//--------------------------------------------------------------------------------------------------
bool res_IsNewnessClearRequired
(
    res_Resource_t  *resPtr ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Mark a resource as no longer "new."  "New" resources are those that were created after the last
 * snapshot scan of the tree.
 */
//--------------------------------------------------------------------------------------------------
void res_ClearNewness
(
    res_Resource_t *resPtr ///< Resource to update.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the resource's "newness" flag.
 *
 * @return Whether the resource was created after the last scan.
 */
//--------------------------------------------------------------------------------------------------
bool res_IsNew
(
    res_Resource_t *resPtr ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Notify that administrative changes are about to be performed.
 *
 * Any resource whose filter or routing (source or destination) settings are changed after a
 * call to res_StartUpdate() will stop accepting new data samples until res_EndUpdate() is called.
 * If new samples are pushed to a resource that is in this state of suspended operation, only
 * the newest one will be remembered and processed when res_EndUpdate() is called.
 */
//--------------------------------------------------------------------------------------------------
void res_StartUpdate
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Notify that all pending administrative changes have been applied, so normal operation may resume,
 * and it's safe to delete buffer backup files that aren't being used.
 */
//--------------------------------------------------------------------------------------------------
void res_EndUpdate
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Read data out of a buffer.  Data is written to a given file descriptor in JSON-encoded format
 * as an array of objects containing a timestamp and a value (or just a timestamp for triggers).
 * E.g.,
 *
 * @code
 * [{"t":1537483647.125,"v":true},{"t":1537483657.128,"v":true}]
 * @endcode
 */
//--------------------------------------------------------------------------------------------------
void res_ReadBufferJson
(
    res_Resource_t* resPtr, ///< Ptr to the resource object for the Observation.
    double startAfter,  ///< Start after this many seconds ago, or after an absolute number of
                        ///< seconds since the Epoch (if startafter > 30 years).
                        ///< Use NAN (not a number) to read the whole buffer.
    int outputFile, ///< File descriptor to write the data to.
    query_ReadCompletionFunc_t handlerPtr, ///< Completion callback.
    void* contextPtr    ///< Value to be passed to completion callback.
);


//--------------------------------------------------------------------------------------------------
/**
 * Find the oldest data sample held in a given Observation's buffer that is newer than a
 * given timestamp.
 *
 * @return Reference to the sample, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t res_FindBufferedSampleAfter
(
    res_Resource_t* resPtr, ///< Ptr to the resource object.
    double startAfter   ///< Start after this many seconds ago, or after an absolute number of
                        ///< seconds since the Epoch (if startafter > 30 years).
                        ///< Use NAN (not a number) to find the oldest.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the resource's "JSON example changed" flag.
 *
 * @return whether the resource's JSON example was updated after the last scan.
 */
//--------------------------------------------------------------------------------------------------
bool res_IsJsonExampleChanged
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Mark a resource's JSON example as not changed.
 */
//--------------------------------------------------------------------------------------------------
void res_ClearJsonExampleChanged
(
    res_Resource_t *resPtr ///< Resource to update.
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the JSON example value for a given resource.
 */
//--------------------------------------------------------------------------------------------------
void res_SetJsonExample
(
    res_Resource_t* resPtr,
    dataSample_Ref_t example
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the JSON example value for a given resource.
 *
 * @return A reference to the example value or NULL if no example set.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t res_GetJsonExample
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * If this is set, all non-JSON data will be ignored, and all JSON data that does not contain the
 * the specified object member or array element will also be ignored.
 */
//--------------------------------------------------------------------------------------------------
void res_SetJsonExtraction
(
    res_Resource_t* resPtr,  ///< Observation resource.
    const char* extractionSpec    ///< [IN] string specifying the JSON member/element to extract.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * @return Ptr to string containing JSON extraction specifier.  "" if not set.
 */
//--------------------------------------------------------------------------------------------------
const char* res_GetJsonExtraction
(
    res_Resource_t* resPtr  ///< Observation resource.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum value found in an Observation's data set within a given time span.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double res_QueryMin
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the maximum value found within a given time span in an Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double res_QueryMax
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the mean (average) of all values found within a given time span in an Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double res_QueryMean
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the standard deviation of all values found within a given time span in an
 * Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double res_QueryStdDev
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
);


//--------------------------------------------------------------------------------------------------
/**
 * Mark an observation as config.
 */
//--------------------------------------------------------------------------------------------------
void res_MarkAsConfig
(
    res_Resource_t* resPtr    ///< Ptr to Observation resource.
);


//--------------------------------------------------------------------------------------------------
/**
 * Checks whether or not an observation is a config.
 *
 * @return:
 *      - true if observation is a config and flase otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool res_IsConfig
(
    res_Resource_t* resPtr    ///< Ptr to Observation resource.
);


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the destination string for the specific Observation.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void res_SetDestination
(
    res_Resource_t* resPtr,      ///< Ptr to Observation resource
    const char* destination      ///< Destination string
);

#endif // RESOURCE_H_INCLUDE_GUARD
