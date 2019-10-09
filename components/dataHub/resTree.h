//--------------------------------------------------------------------------------------------------
/**
 * @file resTree.h
 *
 * Interface to the Resource Tree (resTree) module.
 *
 * The resource tree consists of a tree structure of "Entry" objects (resTree_Entry_t).
 * Input, Output, Observation and Placeholder are all sub-classes of Entry.
 * The inheritance is done by including the resTree_Entry_t as a member of the sub-class struct.
 * This is why resTree_Entry_t appears in this header file, rather than being hidden inside the
 * .c file. But, even though the structure of resTree_Entry_t is visible outside of resTree.c,
 * to reduce coupling, the members of resTree_Entry_t should never be accessed outside of resTree.c.
 *
 * Each app X that is a client of the I/O API is given its own Namespace under which all
 * its Resources will be created.  Apps can only create I/O Resources.
 *
 * Observations all live in the same @c /obs/ Namespace.
 *
 * Placeholders can be anywhere in the tree.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef NAMESPACE_H_INCLUDE_GUARD
#define NAMESPACE_H_INCLUDE_GUARD


#include "resource.h"

//--------------------------------------------------------------------------------------------------
/**
 * Reference to a Resource Tree Entry.
 */
//--------------------------------------------------------------------------------------------------
typedef struct resTree_Entry* resTree_EntryRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Resource Tree module.
 *
 * @warning Must be called before any other functions in this module.
 */
//--------------------------------------------------------------------------------------------------
void resTree_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to the root namespace.
 *
 * @return Reference to the object.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED resTree_EntryRef_t resTree_GetRoot
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Check whether a given resource tree Entry is a Resource.
 *
 * @return true if it is a resource. false if not.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_IsResource
(
    resTree_EntryRef_t entryRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Find a child entry with a given name, optionally including already deleted nodes if they have not
 * been flushed.
 *
 * @return Reference to the object or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_FindChildEx
(
    resTree_EntryRef_t   nsRef,         ///< Namespace entry to search.
    const char          *name,          ///< Name of the child entry.
    bool                 withZombies    ///< If the child has been deleted but is still around,
                                        ///< return it.
);

//--------------------------------------------------------------------------------------------------
/**
 * Find a child entry with a given name.
 *
 * @return Reference to the object or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_FindChild
(
    resTree_EntryRef_t nsRef, ///< Namespace entry to search.
    const char* name    ///< Name of the child entry.
);


//--------------------------------------------------------------------------------------------------
/**
 * Find an entry at a given resource path.
 *
 * @return Reference to the object, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED resTree_EntryRef_t resTree_FindEntry
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path    ///< Path.
);


//--------------------------------------------------------------------------------------------------
/**
 * Find an entry in the resource tree that resides at a given absolute path.
 *
 * @return a pointer to the entry or NULL if not found (including if the path is malformed).
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_FindEntryAtAbsolutePath
(
    const char* path
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the name of an entry.
 *
 * @return Ptr to the name. Only valid while the entry exists.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED const char *resTree_GetEntryName
(
    resTree_EntryRef_t entryRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the type of an entry.
 *
 * @return The entry type.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED admin_EntryType_t resTree_GetEntryType
(
    resTree_EntryRef_t entryRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the Units of a resource.
 *
 * @return Pointer to the units string.  Valid as long as the resource exists.
 */
//--------------------------------------------------------------------------------------------------
const char* resTree_GetUnits
(
    resTree_EntryRef_t resRef
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
LE_SHARED io_DataType_t resTree_GetDataType
(
    resTree_EntryRef_t resRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Gets a reference to an entry at a given path in the resource tree.
 * Creates a Namespace if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * @return
 *      - LE_OK If getting entry was successful and entryRefPtr points to entry reference.
 *      - LE_NO_MEMORY If there was a failure in memory allocation.
 *      - LE_BAD_PARAMETER If path is malformed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_GetEntry
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,                 ///< Path.
    resTree_EntryRef_t* entryRefPtr   ///<[OUT] Pointer to write reference to object to.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to a resource at a given path.
 * Creates a Placeholder resource if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace at the given path, it will be replaced by a
 * Placeholder.
 *
 * @return
 *      - LE_OK If getting resource was successful. entryRefPtr points to entry reference.
 *      - LE_BAD_PARAMETER If path is malformed.
 *      - LE_NO_MEMORY There was a failure to allocate memory.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_GetResource
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,                 ///< Path.
    resTree_EntryRef_t* entryRefPtr   ///<[OUT] Pointer to write reference to object to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new Input resource at the given path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace or Placeholder at the given path, it will be converted to an
 * Input. Should not be called if there's already an IO resource or observation at that path.
 *
 * @return
 *      - LE_OK If successful.
 *      - LE_NO_MEMORY If there was no memory to allocate the input.
 *      - LE_BAD_PARAMETER If path is malformed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_CreateInput
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,       ///< Path.
    io_DataType_t dataType, ///< The data type.
    const char* units       ///< Units string, e.g., "degC" (see senml); "" = unspecified.
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new Output resource at the given path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace or Placeholder at the given path, it will be converted to an
 * Output. Should not be called if there's already an IO resource or observation at that path.
 *
 * @return
 *      - LE_OK If successful.
 *      - LE_NO_MEMORY If there was no memory to allocate the output.
 *      - LE_BAD_PARAMETER If path is malformed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_CreateOutput
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,       ///< Path.
    io_DataType_t dataType, ///< The data type.
    const char* units       ///< Units string, e.g., "degC" (see senml); "" = unspecified.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to an Observation resource at a given path.
 * Creates a new Observation resource if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace or Placeholder at the given path, it will be deleted and
 * replaced by an Observation.
 *
 * @return
 *      - LE_OK If getting observation was successful. entryRefPtr points to entry reference.
 *      - LE_BAD_PARAMETER If the path is malformed or an Input or Output already exists at that
 *        location.
 *      - LE_NO_MEMORY If there was a failure to allocate memory.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_GetObservation
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,                 ///< Path.
    resTree_EntryRef_t* entryRefPtr   ///<[OUT] Pointer to write reference to object to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get observations base namespace entry, the /obs/ path. Creates it if necessary.
 *
 * @return Reference to observations base namespace entry.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED resTree_EntryRef_t resTree_GetObsNamespace
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the path of a given resource tree entry relative to a given namespace.
 *
 * @return
 *  - Number of bytes written to the string buffer (excluding null terminator) if successful.
 *  - LE_OVERFLOW if the string doesn't have space for the path.
 *  - LE_NOT_FOUND if the resource is not in the given namespace.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED ssize_t resTree_GetPath
(
    char* stringBuffPtr,  ///< Ptr to where the path should be written.
    size_t stringBuffSize,  ///< Size of the string buffer, in bytes.
    resTree_EntryRef_t baseNamespace,
    resTree_EntryRef_t entryRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the parent of a given entry.
 *
 * @return Reference to the parent entry, or NULL if the entry has no parent (root).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED resTree_EntryRef_t resTree_GetParent
(
    resTree_EntryRef_t entryRef ///< Node to get the parent of.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the first child of a given entry, optionally including already deleted nodes if they have not
 * been flushed.
 *
 * @return Reference to the first child entry, or NULL if the entry has no children.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetFirstChildEx
(
    resTree_EntryRef_t  entryRef,   ///< Node to get the child of.
    bool                withZombies ///< If the child has been deleted but is still around, return
                                    ///< it.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the first child of a given entry.
 *
 * @return Reference to the first child entry, or NULL if the entry has no children.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED resTree_EntryRef_t resTree_GetFirstChild
(
    resTree_EntryRef_t entryRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the next sibling (child of the same parent) of a given entry, optionally including already
 * deleted nodes if they have not been flushed.
 *
 * @return Reference to the next entry in the parent's child list, or
 *         NULL if already at the last child.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetNextSiblingEx
(
    resTree_EntryRef_t  entryRef,   ///< Node to get the sibling of.
    bool                withZombies ///< If the sibling has been deleted but is still around, return
                                    ///< it.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the next sibling (child of the same parent) of a given entry.
 *
 * @return Reference to the next entry in the parent's child list, or
 *         NULL if already at the last child.
 *
 * @warning Do not call this for the Root Entry.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED resTree_EntryRef_t resTree_GetNextSibling
(
    resTree_EntryRef_t entryRef
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
 *      - LE_BAD_PARAMETER If there is a mismatch of datasample unit.
 *      - LE_FAULT is any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_Push
(
    resTree_EntryRef_t entryRef,    ///< The entry to push to.
    io_DataType_t dataType,         ///< The data type.
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
hub_HandlerRef_t resTree_AddPushHandler
(
    resTree_EntryRef_t resRef, ///< Reference to the Output resource.
    io_DataType_t dataType,
    void* callbackPtr,
    void* contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove a Push Handler from a resource.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_RemovePushHandler
(
    hub_HandlerRef_t handlerRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource.
 *
 * @return Reference to the Data Sample object or NULL if the resource doesn't have a current value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED dataSample_Ref_t resTree_GetCurrentValue
(
    resTree_EntryRef_t resRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a data flow route from one resource to another by setting the data source for the
 * destination resource.  If the destination resource already has a source resource, it will be
 * replaced. Does nothing if the route already exists.
 *
 * @note While an Input can have a source configured, it will ignore anything pushed to it
 *       from other resources via that route. Inputs only accept values pushed by the app that
 *       created them or from the administrator pushed directly to them via admin_Push().
 *
 * @return
 *  - LE_OK if route already existed or new route was successfully created.
 *  - LE_DUPLICATE if the addition of this route would result in a loop.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_SetSource
(
    resTree_EntryRef_t destEntry,
    resTree_EntryRef_t srcEntry     ///< Source entry ref, or NULL to clear the source.
);


//--------------------------------------------------------------------------------------------------
/**
 * Fetches the data flow source resource entry from which a given resource expects to receive data
 * samples.
 *
 * @return Reference to the source entry or NULL if none configured.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetSource
(
    resTree_EntryRef_t destEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Delete an Input or Output resource.
 *
 * Converts the resource into a Placeholder if it still has configuration settings.
 */
//--------------------------------------------------------------------------------------------------
void resTree_DeleteIO
(
    resTree_EntryRef_t entryRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Delete an Observation.
 *
 * Deletes any configuration settings that still exist before deleting the Observation.
 */
//--------------------------------------------------------------------------------------------------
void resTree_DeleteObservation
(
    resTree_EntryRef_t entryRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Delete a Namespace resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_DeleteNamespace
(
    resTree_EntryRef_t entryRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum period between data samples accepted by a given Observation.
 *
 * This is used to throttle the rate of data passing into and through an Observation.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetMinPeriod
(
    resTree_EntryRef_t obsEntry,
    double minPeriod
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum period between data samples accepted by a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetMinPeriod
(
    resTree_EntryRef_t obsEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the highest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetHighLimit
(
    resTree_EntryRef_t obsEntry,
    double highLimit
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the highest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetHighLimit
(
    resTree_EntryRef_t obsEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the lowest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetLowLimit
(
    resTree_EntryRef_t obsEntry,
    double lowLimit
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the lowest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetLowLimit
(
    resTree_EntryRef_t obsEntry
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
void resTree_SetChangeBy
(
    resTree_EntryRef_t obsEntry,
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
double resTree_GetChangeBy
(
    resTree_EntryRef_t obsEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform a transform on buffered data. Value of the observation will be the output of the
 * transform
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetTransform
(
    resTree_EntryRef_t obsEntry,
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
admin_TransformType_t resTree_GetTransform
(
    resTree_EntryRef_t obsEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of data samples to buffer in a given Observation.  Buffers are FIFO
 * circular buffers. When full, the buffer drops the oldest value to make room for a new addition.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetBufferMaxCount
(
    resTree_EntryRef_t obsEntry,
    uint32_t count
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the buffer size setting for a given Observation.
 *
 * @return The buffer size (in number of samples) or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
uint32_t resTree_GetBufferMaxCount
(
    resTree_EntryRef_t obsEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum time between backups of an Observation's buffer to non-volatile storage.
 * If the buffer's size is non-zero and the backup period is non-zero, then the buffer will be
 * backed-up to non-volatile storage when it changes, but never more often than this period setting
 * specifies.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetBufferBackupPeriod
(
    resTree_EntryRef_t obsEntry,
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
uint32_t resTree_GetBufferBackupPeriod
(
    resTree_EntryRef_t obsEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void resTree_MarkOptional
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Check if a given resource is a mandatory output.  If so, it means that this is an output resource
 * that must have a value before the related app function will begin working.
 *
 * @return true if a mandatory output, false if it's an optional output or not an output at all.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool resTree_IsMandatory
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If could not set default value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch.
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_SetDefault
(
    resTree_EntryRef_t resEntry,
    io_DataType_t dataType,
    dataSample_Ref_t value
);


//--------------------------------------------------------------------------------------------------
/**
 * Discover whether a given resource has a default value.
 *
 * @return true if there is a default value set, false if not.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool resTree_HasDefault
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the default value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_DataType_t resTree_GetDefaultDataType
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the default value of a resource.
 *
 * @return the default value, or NULL if not set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED dataSample_Ref_t resTree_GetDefaultValue
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove any default value that might be set on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_RemoveDefault
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Set an override on a given resource.
 *
 * @return
 *      - LE_OK If setting override was successful.
 *      - LE_NO_MEMORY If could not set override value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set override value due to type or unit mismatch.
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_SetOverride
(
    resTree_EntryRef_t resEntry,
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
bool resTree_HasOverride
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the override value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t resTree_GetOverrideDataType
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the override value of a resource.
 *
 * @return the override value, or NULL if not set.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t resTree_GetOverrideValue
(
    resTree_EntryRef_t resEntry
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove any override that might be in effect for a given resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_RemoveOverride
(
    resTree_EntryRef_t resEntry
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the last modified time stamp of a resource.
 *
 * @return Time stamp value, in seconds since the Epoch.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetLastModified
(
    resTree_EntryRef_t resEntry ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the node's relevance flag.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetRelevance
(
    resTree_EntryRef_t  resEntry,   ///< Resource to query.
    bool                relevant    ///< Relevance of node to current operation.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the node's relevance flag.
 *
 * @return Relevance of node to the current operation.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_IsRelevant
(
    resTree_EntryRef_t resEntry ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the node's clear newness flag
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetClearNewnessFlag
(
    resTree_EntryRef_t resEntry ///< Resource to update.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the node's "clear newness" flag.
 *
 * @return Whether the node "newness" flag must be cleared at the end of current snapshot
 */
//--------------------------------------------------------------------------------------------------
bool resTree_IsNewnessClearRequired
(
    resTree_EntryRef_t resEntry ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Mark a node as no longer "new."  New nodes are those that were created after the last snapshot
 * scan of the tree.
 */
//--------------------------------------------------------------------------------------------------
void resTree_ClearNewness
(
    resTree_EntryRef_t resEntry ///< Resource to update.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the node's "newness" flag.
 *
 * @return Whether the node was created after the last scan.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool resTree_IsNew
(
    resTree_EntryRef_t resEntry ///< Resource to query.
);

//--------------------------------------------------------------------------------------------------
/**
 * Mark a node as deleted.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetDeleted
(
    resTree_EntryRef_t resEntry ///< Resource to update.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the node's "deleted" flag.
 *
 * @return Whether the node was deleted after the last flush.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool resTree_IsDeleted
(
    resTree_EntryRef_t resEntry ///< Resource to query.
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
void resTree_StartUpdate
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Notify that all pending administrative changes have been applied, so normal operation may resume,
 * and it's safe to delete buffer backup files that aren't being used.
 */
//--------------------------------------------------------------------------------------------------
void resTree_EndUpdate
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * For each resource in the resource tree, call a given function.
 */
//--------------------------------------------------------------------------------------------------
void resTree_ForEachResource
(
    void (*func)(res_Resource_t* resPtr, admin_EntryType_t entryType)
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
void resTree_ReadBufferJson
(
    resTree_EntryRef_t obsEntry, ///< Observation entry.
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
dataSample_Ref_t resTree_FindBufferedSampleAfter
(
    resTree_EntryRef_t obsEntry, ///< Observation resource entry.
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
LE_SHARED bool resTree_IsJsonExampleChanged
(
    resTree_EntryRef_t resEntry ///< Resource to poll
);


//--------------------------------------------------------------------------------------------------
/**
 * Mark a resource's JSON example as not changed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void resTree_ClearJsonExampleChanged
(
    resTree_EntryRef_t resEntry ///< Resource to update.
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the JSON example value for a given resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetJsonExample
(
    resTree_EntryRef_t resEntry,
    dataSample_Ref_t example
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the JSON example value for a given resource.
 *
 * @return A reference to the example value or NULL if no example set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED dataSample_Ref_t resTree_GetJsonExample
(
    resTree_EntryRef_t resEntry
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
void resTree_SetJsonExtraction
(
    resTree_EntryRef_t resEntry,  ///< Observation entry.
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
const char* resTree_GetJsonExtraction
(
    resTree_EntryRef_t resEntry  ///< Observation entry.
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
double resTree_QueryMin
(
    resTree_EntryRef_t obsEntry,    ///< Observation entry.
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
double resTree_QueryMax
(
    resTree_EntryRef_t obsEntry,    ///< Observation entry.
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
double resTree_QueryMean
(
    resTree_EntryRef_t obsEntry,    ///< Observation entry.
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
double resTree_QueryStdDev
(
    resTree_EntryRef_t obsEntry,    ///< Observation entry.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
);


//--------------------------------------------------------------------------------------------------
/**
 *  Mark an observation as config.
 */
//--------------------------------------------------------------------------------------------------
void resTree_MarkObservationAsConfig
(
    resTree_EntryRef_t obsEntry    ///< Observation entry.
);


//--------------------------------------------------------------------------------------------------
/**
 *  Is an observation entry a config?
 *
 *  @return
 *      - true if observation is config and false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_IsObservationConfig
(
    resTree_EntryRef_t obsEntry    ///< Observation entry.
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the destination string for the specific Observation.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetDestination
(
    resTree_EntryRef_t obsEntry,  ///< Observation entry
    const char* destination       ///< Destination string
);

#endif // NAMESPACE_H_INCLUDE_GUARD
