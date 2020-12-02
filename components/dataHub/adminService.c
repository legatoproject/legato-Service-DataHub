//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Data Hub Admin API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "dataHub.h"
#include "ioService.h"
#include "resource.h"
#include "handler.h"
#include "json.h"

typedef struct
{
    le_dls_Link_t link; ///< Used to link into the ResourceTreeChangeHandlerList
    admin_ResourceTreeChangeHandlerFunc_t callback;
    void* contextPtr;
}
ResourceTreeChangeHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * List of Resource Tree Change Handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_dls_List_t ResourceTreeChangeHandlerList = LE_DLS_LIST_INIT;

/// Default change handler pool size.  This may be overridden in the .cdef.
#define DEFAULT_RESOURCE_TREE_CHANGE_HANDLER_POOL_SIZE 2

//--------------------------------------------------------------------------------------------------
/**
 * Pool of ResourceTreeChangeHandler objects.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ResourceTreeChangeHandlerPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(ResourceTreeChangeHandlerPool,
                          DEFAULT_RESOURCE_TREE_CHANGE_HANDLER_POOL_SIZE,
                          sizeof(ResourceTreeChangeHandler_t));

//--------------------------------------------------------------------------------------------------
/**
 *  Current number of registered push handlers:
 */
//--------------------------------------------------------------------------------------------------
static unsigned int PushHandlerCount;


//--------------------------------------------------------------------------------------------------
/**
 * Push a trigger type data sample to a resource.
 *
 * @note The LE_OK return from this function means the sample has been successfully received by
 * datahub. It does not guarantee that the sample will be successfully processed by observers
 * of the path or that the sample will not be lost after power cycle.
 *
 * @return
 *      - LE_OK If datasample was pushed successfully.
 *      - LE_NO_MEMORY If failed to push the data sample because of failure in memory allocation.
 *      - LE_IN_PROGRESS Push is rejected because a configuration update is in progress.
 *      - LE_BAD_PARAMETER If there is a mismatch of datasample unit.
 *      - LE_NOT_FOUND If the path does not exist. This will not cause a Placeholder resource to be
 *          created.
 *      - LE_FAULT If any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_PushTrigger
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    double timestamp
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);
    le_result_t ret;

    if (entry != NULL)
    {
        dataSample_Ref_t dataSampleRef = dataSample_CreateTrigger(timestamp);
        if (dataSampleRef)
        {
            ret = resTree_Push(entry, IO_DATA_TYPE_TRIGGER, dataSampleRef);
        }
        else
        {
            LE_ERROR("Failed to push a trigger to path '%s'.", path);
            ret = LE_NO_MEMORY;
        }
    }
    else
    {
        LE_WARN("Discarding value pushed to non-existent resource '%s'.", path);
        ret = LE_NOT_FOUND;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a Boolean type data sample to a resource.
 *
 * @note The LE_OK return from this function means the sample has been successfully received by
 * datahub. It does not guarantee that the sample will be successfully processed by observers
 * of the path or that the sample will not be lost after power cycle.
 *
 * @return
 *      - LE_OK If datasample was pushed successfully.
 *      - LE_NO_MEMORY If failed to push the data sample because of failure in memory allocation.
 *      - LE_IN_PROGRESS Push is rejected because a configuration update is in progress.
 *      - LE_BAD_PARAMETER If there is a mismatch of datasample unit.
 *      - LE_NOT_FOUND If the path does not exist. This will not cause a Placeholder resource to be
 *          created.
 *      - LE_FAULT If any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_PushBoolean
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now.
    bool value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);
    le_result_t ret;

    if (entry != NULL)
    {
        dataSample_Ref_t dataSampleRef = dataSample_CreateBoolean(timestamp, value);
        if (dataSampleRef)
        {
            ret = resTree_Push(entry, IO_DATA_TYPE_BOOLEAN, dataSampleRef);
        }
        else
        {
            LE_ERROR("Failed to push a boolean to path '%s'.", path);
            ret = LE_NO_MEMORY;
        }
    }
    else
    {
        LE_WARN("Discarding value pushed to non-existent resource '%s'.", path);
        ret = LE_NOT_FOUND;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric type data sample to a resource.
 *
 * @note The LE_OK return from this function means the sample has been successfully received by
 * datahub. It does not guarantee that the sample will be successfully processed by observers
 * of the path or that the sample will not be lost after power cycle.
 *
 * @return
 *      - LE_OK If datasample was pushed successfully.
 *      - LE_NO_MEMORY If failed to push the data sample because of failure in memory allocation.
 *      - LE_IN_PROGRESS Push is rejected because a configuration update is in progress.
 *      - LE_BAD_PARAMETER If there is a mismatch of datasample unit.
 *      - LE_NOT_FOUND If the path does not exist. This will not cause a Placeholder resource to be
 *          created.
 *      - LE_FAULT If any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_PushNumeric
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now.
    double value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);
    le_result_t ret;

    if (entry != NULL)
    {
        dataSample_Ref_t dataSampleRef = dataSample_CreateNumeric(timestamp, value);
        if (dataSampleRef)
        {
            ret = resTree_Push(entry, IO_DATA_TYPE_NUMERIC, dataSampleRef);
        }
        else
        {
            LE_ERROR("Failed to push a numeric to path '%s'.", path);
            ret = LE_NO_MEMORY;
        }
    }
    else
    {
        LE_WARN("Discarding value pushed to non-existent resource '%s'.", path);
        ret = LE_NOT_FOUND;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a string type data sample to a resource.
 *
 * @note The LE_OK return from this function means the sample has been successfully received by
 * datahub. It does not guarantee that the sample will be successfully processed by observers
 * of the path or that the sample will not be lost after power cycle.
 *
 * @return
 *      - LE_OK If datasample was pushed successfully.
 *      - LE_NO_MEMORY If failed to push the data sample because of failure in memory allocation.
 *      - LE_IN_PROGRESS Push is rejected because a configuration update is in progress.
 *      - LE_BAD_PARAMETER If there is a mismatch of datasample unit.
 *      - LE_NOT_FOUND If the path does not exist. This will not cause a Placeholder resource to be
 *          created.
 *      - LE_FAULT If any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_PushString
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now.
    const char* value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);
    le_result_t ret;

    if (entry != NULL)
    {
        dataSample_Ref_t dataSampleRef = dataSample_CreateString(timestamp, value);
        if (dataSampleRef)
        {
            ret = resTree_Push(entry, IO_DATA_TYPE_STRING, dataSampleRef);
        }
        else
        {
            LE_ERROR("Failed to push a string to path '%s'.", path);
            ret = LE_NO_MEMORY;
        }
    }
    else
    {
        LE_WARN("Discarding value pushed to non-existent resource '%s'.", path);
        ret = LE_NOT_FOUND;
    }
    return ret;
}



//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON data sample to a resource.
 *
 * @note The LE_OK return from this function means the sample has been successfully received by
 * datahub. It does not guarantee that the sample will be successfully processed by observers
 * of the path or that the sample will not be lost after power cycle.
 *
 * @return
 *      - LE_OK If datasample was pushed successfully.
 *      - LE_NO_MEMORY If failed to push the data sample because of failure in memory allocation.
 *      - LE_IN_PROGRESS Push is rejected because a configuration update is in progress.
 *      - LE_BAD_PARAMETER If there is a mismatch of datasample unit or JSON is invalid.
 *      - LE_NOT_FOUND If the path does not exist. This will not cause a Placeholder resource to be
 *          created.
 *      - LE_FAULT If any other error happened during push.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_PushJson
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< Zero = now.
    const char* value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);
    le_result_t ret;

    if (entry != NULL)
    {
        if (json_IsValid(value))
        {
            dataSample_Ref_t dataSampleRef = dataSample_CreateJson(timestamp, value);
            if (dataSampleRef)
            {
                ret = resTree_Push(entry, IO_DATA_TYPE_JSON, dataSampleRef);
            }
            else
            {
                LE_ERROR("Failed to push a JSON to path '%s'.", path);
                ret = LE_NO_MEMORY;
            }
        }
        else
        {
            LE_ERROR("Discarding invalid JSON string '%s'.", value);
            ret = LE_BAD_PARAMETER;
        }
    }
    else
    {
        LE_WARN("Discarding value pushed to non-existent resource '%s'.", path);
        ret = LE_NOT_FOUND;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add a handler function to be called when a value is pushed to (and accepted by) a resource
 * anywhere in the resource tree.  If there's no resource at that location yet, a placeholder will
 * be created.
 *
 * @return A reference to the handler, which can be removed using handler_Remove(). If adding the
 *         handler fails due to a malformed path or resource exhaustion, NULL is returned.
 */
//--------------------------------------------------------------------------------------------------
static hub_HandlerRef_t AddPushHandler
(
    const char* path,   ///< Absolute resource path.
    io_DataType_t dataType,
    void* callbackPtr,  ///< Callback function pointer
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
#ifdef DHUB_ADMIN_PUSH_HANDLER_COUNT
    if (PushHandlerCount >= DHUB_ADMIN_PUSH_HANDLER_COUNT)
    {
        LE_WARN("Cannot add any more admin push handlers. Rejecting handler on %s", path);
        return NULL;
    }
#endif
    resTree_EntryRef_t resRef;
    if (resTree_GetResource(resTree_GetRoot(), path, &resRef) != LE_OK)
    {
        LE_ERROR("Failed to get resource at '%s'. Cannot add handler", path);
        return NULL;
    }

    hub_HandlerRef_t handlerRef = resTree_AddPushHandler(resRef, dataType, callbackPtr, contextPtr);
    if (handlerRef == NULL)
    {
        return NULL;
    }

    PushHandlerCount++;
    LE_DEBUG("Added Handler %p, current handler count: %d", handlerRef, PushHandlerCount);

    // If the resource has a current value call the push handler now (if it's a data type match).
    dataSample_Ref_t sampleRef = resTree_GetCurrentValue(resRef);
    if (sampleRef != NULL)
    {
        handler_Call(handlerRef, resTree_GetDataType(resRef), sampleRef);
    }

    return handlerRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_TriggerPush'
 *
 * @return reference to the added push handler, or NULL if the path is malformed.
 *
 * @note If a NULL reference is returned, the IPC system should just store this and
 *       use it as the handler reference when removing the handler later. handler_Remove()
 *       will report an error when this happens, but there will be no other adverse effects.
 *       On the other side of the IPC link, the client will not see the NULL, as it will
 *       be replaced by a reference to a local proxy handler object.
 */
//--------------------------------------------------------------------------------------------------
admin_TriggerPushHandlerRef_t admin_AddTriggerPushHandler
(
    const char* path,
        ///< [IN] Absolute path of resource.
    admin_TriggerPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    hub_HandlerRef_t ref = AddPushHandler(path, IO_DATA_TYPE_TRIGGER, callbackPtr, contextPtr);

    return (admin_TriggerPushHandlerRef_t)ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveTriggerPushHandler
(
    admin_TriggerPushHandlerRef_t handlerRef
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    if (handler_Remove((hub_HandlerRef_t)handlerRef) == LE_OK)
    {
        LE_ASSERT(PushHandlerCount != 0);
        PushHandlerCount--;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_BooleanPush'
 *
 * @return reference to the added push handler, or NULL if the path is malformed.
 *
 * @note If a NULL reference is returned, the IPC system should just store this and
 *       use it as the handler reference when removing the handler later. handler_Remove()
 *       will report an error when this happens, but there will be no other adverse effects.
 *       On the other side of the IPC link, the client will not see the NULL, as it will
 *       be replaced by a reference to a local proxy handler object.
 */
//--------------------------------------------------------------------------------------------------
admin_BooleanPushHandlerRef_t admin_AddBooleanPushHandler
(
    const char* path,
        ///< [IN] Absolute path of resource.
    admin_BooleanPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    hub_HandlerRef_t ref = AddPushHandler(path, IO_DATA_TYPE_BOOLEAN, callbackPtr, contextPtr);

    return (admin_BooleanPushHandlerRef_t)ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveBooleanPushHandler
(
    admin_BooleanPushHandlerRef_t handlerRef
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    if (handler_Remove((hub_HandlerRef_t)handlerRef) == LE_OK)
    {
        LE_ASSERT(PushHandlerCount != 0);
        PushHandlerCount--;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_NumericPush'
 *
 * @return reference to the added push handler, or NULL if the path is malformed.
 *
 * @note If a NULL reference is returned, the IPC system should just store this and
 *       use it as the handler reference when removing the handler later. handler_Remove()
 *       will report an error when this happens, but there will be no other adverse effects.
 *       On the other side of the IPC link, the client will not see the NULL, as it will
 *       be replaced by a reference to a local proxy handler object.
 */
//--------------------------------------------------------------------------------------------------
admin_NumericPushHandlerRef_t admin_AddNumericPushHandler
(
    const char* path,
        ///< [IN] Absolute path of resource.
    admin_NumericPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    hub_HandlerRef_t ref = AddPushHandler(path, IO_DATA_TYPE_NUMERIC, callbackPtr, contextPtr);

    return (admin_NumericPushHandlerRef_t)ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveNumericPushHandler
(
    admin_NumericPushHandlerRef_t handlerRef
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    if (handler_Remove((hub_HandlerRef_t)handlerRef) == LE_OK)
    {
        LE_ASSERT(PushHandlerCount != 0);
        PushHandlerCount--;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_StringPush'
 *
 * @return reference to the added push handler, or NULL if the path is malformed.
 *
 * @note If a NULL reference is returned, the IPC system should just store this and
 *       use it as the handler reference when removing the handler later. handler_Remove()
 *       will report an error when this happens, but there will be no other adverse effects.
 *       On the other side of the IPC link, the client will not see the NULL, as it will
 *       be replaced by a reference to a local proxy handler object.
 */
//--------------------------------------------------------------------------------------------------
admin_StringPushHandlerRef_t admin_AddStringPushHandler
(
    const char* path,
        ///< [IN] Absolute path of resource.
    admin_StringPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    hub_HandlerRef_t ref = AddPushHandler(path, IO_DATA_TYPE_STRING, callbackPtr, contextPtr);

    return (admin_StringPushHandlerRef_t)ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_StringPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveStringPushHandler
(
    admin_StringPushHandlerRef_t handlerRef
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    if (handler_Remove((hub_HandlerRef_t)handlerRef) == LE_OK)
    {
        LE_ASSERT(PushHandlerCount != 0);
        PushHandlerCount--;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'admin_JsonPush'
 *
 * @return reference to the added push handler, or NULL if the path is malformed.
 *
 * @note If a NULL reference is returned, the IPC system should just store this and
 *       use it as the handler reference when removing the handler later. handler_Remove()
 *       will report an error when this happens, but there will be no other adverse effects.
 *       On the other side of the IPC link, the client will not see the NULL, as it will
 *       be replaced by a reference to a local proxy handler object.
 */
//--------------------------------------------------------------------------------------------------
admin_JsonPushHandlerRef_t admin_AddJsonPushHandler
(
    const char* path,
        ///< [IN] Absolute path of resource.
    admin_JsonPushHandlerFunc_t callbackPtr,
        ///< [IN]
    void* contextPtr
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    hub_HandlerRef_t ref = AddPushHandler(path, IO_DATA_TYPE_JSON, callbackPtr, contextPtr);

    return (admin_JsonPushHandlerRef_t)ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveJsonPushHandler
(
    admin_JsonPushHandlerRef_t handlerRef
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    if (handler_Remove((hub_HandlerRef_t)handlerRef) == LE_OK)
    {
        LE_ASSERT(PushHandlerCount != 0);
        PushHandlerCount--;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Creates a data flow route from one resource to another by setting the data source for the
 * destination resource.  If the destination resource already has a source resource, it will be
 * replaced. Does nothing if the route already exists.
 *
 * Creates Placeholders for any resources that do not yet exist in the resource tree.
 *
 * @return
 *  - LE_OK if route already existed or new route was successfully created.
 *  - LE_BAD_PARAMETER if one of the paths is invalid.
 *  - LE_DUPLICATE if the addition of this route would result in a loop.
 *  - LE_NO_MEMORY if there was a failure in memory allocation.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetSource
(
    const char* destPath,
        ///< [IN] Absolute path of destination resource.
    const char* srcPath
        ///< [IN] Absolute path of source resource.
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t ret = LE_OK;
    bool createdDestEntry;
    resTree_EntryRef_t srcEntry = NULL;
    resTree_EntryRef_t destEntry = resTree_FindEntry(resTree_GetRoot(), destPath);
    if (destEntry)
    {
        createdDestEntry = false;
    }
    else
    {
        ret = resTree_GetResource(resTree_GetRoot(), destPath, &destEntry);
        if (ret == LE_OK)
        {
            createdDestEntry = true;
        }
    }
    if (ret == LE_OK)
    {
        ret = resTree_GetResource(resTree_GetRoot(), srcPath, &srcEntry);
    }

    // Set the source.
    if (ret == LE_OK)
    {
        ret = resTree_SetSource(destEntry, srcEntry);
    }
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to create a data flow route from %s to %s", srcPath, destPath);
        if (createdDestEntry)
        {
            // Need to clean up destEntry if it was created above.
            le_mem_Release(destEntry);
        }
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Fetches the data flow source resource from which a given resource expects to receive data
 * samples.
 *
 * @note While an Input can have a source configured, it will ignore anything pushed to it
 *       from other resources via that route. Inputs only accept values pushed by the app that
 *       created them or from the administrator pushed directly to them via admin_Push().
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
    const char* destPath,
        ///< [IN] Absolute path of destination resource.
    char* srcPath,
        ///< [OUT] Absolute path of source resource.
    size_t srcPathSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(destPath);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return LE_NOT_FOUND;
    }
    else
    {
        resTree_EntryRef_t srcEntry = resTree_GetSource(resEntry);

        if (srcEntry == NULL)
        {
            return LE_NOT_FOUND;
        }

        le_result_t result = resTree_GetPath(srcPath, srcPathSize, resTree_GetRoot(), srcEntry);

        if (result >= 0)
        {
            return LE_OK;
        }
        else if (result == LE_OVERFLOW)
        {
            return LE_OVERFLOW;
        }

        LE_FATAL("Unexpected result %d (%s)", result, LE_RESULT_TXT(result));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove the data flow route into a resource.
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveSource
(
    const char* destPath
        ///< [IN] Absolute path of destination resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t destEntry = resTree_FindEntry(resTree_GetRoot(), destPath);

    if ((destEntry != NULL) && resTree_IsResource(destEntry))
    {
        resTree_SetSource(destEntry, NULL);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to an Observation entry in the resource tree.
 * Creates the Observation, if necessary.
 *
 * @return The entry reference or NULL if the path is malformed.
 */
//--------------------------------------------------------------------------------------------------
static resTree_EntryRef_t GetObservation
(
    const char* path    ///< Absolute path under /obs/ or relative path from /obs/.
)
//--------------------------------------------------------------------------------------------------
{
    if (strncmp(path, "/obs/", 5) == 0)
    {
        path += 5;
    }
    else if (path[0] == '/')
    {
        return NULL;
    }
    resTree_EntryRef_t entryRef;
    if (resTree_GetObservation(resTree_GetObsNamespace(), path, &entryRef) == LE_OK)
    {
        return entryRef;
    }
    else
    {
        return NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to an Observation entry in the resource tree, iff it already exists.
 *
 * @return The entry reference or NULL if the path is malformed or the object doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
static resTree_EntryRef_t FindObservation
(
    const char* path    ///< Absolute path under /obs/ or relative path from /obs/.
)
//--------------------------------------------------------------------------------------------------
{
    if (strncmp(path, "/obs/", 5) == 0)
    {
        path += 5;
    }
    else if (path[0] == '/')
    {
        return NULL;
    }

    resTree_EntryRef_t entryRef = resTree_FindEntry(resTree_GetObsNamespace(), path);

    if (resTree_GetEntryType(entryRef) != ADMIN_ENTRY_TYPE_OBSERVATION)
    {
        LE_WARN("Entry '%s' is not an Observation.", path);
        return NULL;
    }

    return entryRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create an Observation in the /obs/ namespace.
 *
 *  @return
 *  - LE_OK If the observation was created or it already existed.
 *  - LE_FAULT If failed to create observation.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_CreateObs
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obs = GetObservation(path);
    if (obs == NULL)
    {
        return LE_FAULT;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete an Observation in the /obs/ namespace.
 */
//--------------------------------------------------------------------------------------------------
void admin_DeleteObs
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    // Verify that the path is within the /obs namespace, and if it's an absolute path, convert
    // it into a relative path.
    if (strncmp(path, "/obs/", 5) == 0)
    {
        path += 5;
    }
    else if (path[0] == '/')
    {
        LE_ERROR("Path is outside the /obs/ namespace (%s).", path);
        return;
    }

    resTree_EntryRef_t obsNamespace = resTree_FindEntry(resTree_GetRoot(), "obs");

    if (obsNamespace != NULL)
    {
        resTree_EntryRef_t entry = resTree_FindEntry(obsNamespace, path);

        if (entry != NULL)
        {
            resTree_DeleteObservation(entry);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum period between data samples accepted by a given Observation.
 *
 * This is used to throttle the rate of data passing into and through an Observation.
 *
 * @return
 *      - LE_OK If minimum period was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetMinPeriod
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    double minPeriod
        ///< [IN] The minimum period, in seconds.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetMinPeriod(obsEntry, minPeriod);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum period between data samples accepted by a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetMinPeriod
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return 0;
    }
    else
    {
        return resTree_GetMinPeriod(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the highest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 *
 * @return
 *      - LE_OK If high limit was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetHighLimit
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    double highLimit
        ///< [IN] The highest value in the range, or NAN (not a number) to remove limit.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetHighLimit(obsEntry, highLimit);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the highest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetHighLimit
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return NAN;
    }
    else
    {
        return resTree_GetHighLimit(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the lowest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 *
 * @return
 *      - LE_OK If low limit was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetLowLimit
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    double lowLimit
        ///< [IN] The lowest value in the range, or NAN (not a number) to remove limit.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetLowLimit(obsEntry, lowLimit);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the lowest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetLowLimit
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return NAN;
    }
    else
    {
        return resTree_GetLowLimit(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * Ignored for trigger types.
 *
 * For all other types, any non-zero value means accept any change, but drop if the same as current.
 *
 * @return
 *      - LE_OK If change by magnitude was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetChangeBy
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    double change
        ///< [IN] The magnitude, or either zero or NAN (not a number) to remove limit.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetChangeBy(obsEntry, change);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetChangeBy
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return 0;
    }
    else
    {
        return resTree_GetChangeBy(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Perform a transform on buffered data. Value of the observation will be the output of the
 * transform
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 *
 * @return
 *      - LE_OK If transfrom was done successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetTransform
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    admin_TransformType_t transformType,
        ///< [IN] Type of transform to apply
    const double* paramsPtr,
        ///< [IN] Optional parameter list
    size_t paramsSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetTransform(obsEntry, transformType, paramsPtr, paramsSize);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the type of transform currently applied to an Observation.
 *
 * @return The TransformType
 */
//--------------------------------------------------------------------------------------------------
admin_TransformType_t admin_GetTransform
(
    const char* path
        ///< Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return ADMIN_OBS_TRANSFORM_TYPE_NONE;
    }
    else
    {
        return resTree_GetTransform(obsEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * If this is set, all non-JSON data will be ignored, and all JSON data that does not contain the
 * the specified object member or array element will also be ignored.
 *
 * @return
 *      - LE_OK If JSON extraction was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetJsonExtraction
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    const char* extractionSpec
        ///< [IN] string specifying the JSON member/element to extract.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetJsonExtraction(obsEntry, extractionSpec);
        return LE_OK;
    }
}


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
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return LE_NOT_FOUND;
    }
    else
    {
        const char* extractionSpec = resTree_GetJsonExtraction(resEntry);

        if (extractionSpec[0] == '\0')
        {
            return LE_NOT_FOUND;
        }

        return le_utf8_Copy(result, extractionSpec, resultSize, NULL);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of data samples to buffer in a given Observation.  Buffers are FIFO
 * circular buffers. When full, the buffer drops the oldest value to make room for a new addition.
 *
 * @return
 *      - LE_OK If max buffer count was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t admin_SetBufferMaxCount
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    uint32_t count
        ///< [IN] The number of samples to buffer (0 = remove setting).
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetBufferMaxCount(obsEntry, count);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the buffer size setting for a given Observation.
 *
 * @return The buffer size (in number of samples) or 0 if not set or the Observation does not exist.
 */
//--------------------------------------------------------------------------------------------------
uint32_t admin_GetBufferMaxCount
(
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return 0;
    }
    else
    {
        return resTree_GetBufferMaxCount(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum time between backups of an Observation's buffer to non-volatile storage.
 * If the buffer's size is non-zero and the backup period is non-zero, then the buffer will be
 * backed-up to non-volatile storage when it changes, but never more often than this period setting
 * specifies.
 *
 * @return
 *      - LE_OK If buffer backup period was set successfully.
 *      - LE_FAULT If an error happened during set.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBufferBackupPeriod
(
    const char* path,
        ///< [IN] Path within the /obs/ namespace.
    uint32_t seconds
        ///< [IN] The minimum number of seconds between backups (0 = disable backups)
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t obsEntry = GetObservation(path);

    if (obsEntry == NULL)
    {
        LE_ERROR("Failed to get observation on path '%s'.", path);
        return LE_FAULT;
    }
    else
    {
        resTree_SetBufferBackupPeriod(obsEntry, seconds);
        return LE_OK;
    }
}


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
    const char* path
        ///< [IN] Path within the /obs/ namespace.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = FindObservation(path);

    if (resEntry == NULL)
    {
        return 0;
    }
    else
    {
        return resTree_GetBufferBackupPeriod(resEntry);
    }
}


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
    const char* path  ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if (resEntry == NULL)
    {
        return false;
    }
    else
    {
        return resTree_IsMandatory(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set a default value for a given resource.
 *
 * @note Default will be discarded by an Input or Output resource if the default's data type
 *       does not match the data type of the Input or Output.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If setting default failed because of failure to allocate memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch.
 *      - LE_FAULT If setting default failed becasue of any other error.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetDefault
(
    const char* path, ///< [IN] Absolute path of the resource.
    io_DataType_t dataType,
    dataSample_Ref_t value
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry;
    le_result_t ret = resTree_GetResource(resTree_GetRoot(), path, &resEntry);

    if (ret != LE_OK)
    {
        LE_ERROR("Failed to get resource at '%s'.", path);
        le_mem_Release(value);
    }
    else
    {
        ret = resTree_SetDefault(resEntry, dataType, value);
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a Boolean value.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If could not set default due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch.
 *      - LE_FAULT If setting default failed becasue of any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBooleanDefault
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    bool value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    dataSample_Ref_t dataSampleRef = dataSample_CreateBoolean(0, value);
    le_result_t ret;
    if (dataSampleRef)
    {
        ret = SetDefault(path, IO_DATA_TYPE_BOOLEAN, dataSampleRef);
    }
    else
    {
        LE_ERROR("Failed to set default boolean value on path:'%s'", path);
        ret = LE_NO_MEMORY;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a numeric value.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If could not set default due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch.
 *      - LE_FAULT If setting default failed becasue of any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetNumericDefault
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    double value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    dataSample_Ref_t dataSampleRef = dataSample_CreateNumeric(0, value);
    le_result_t ret;
    if (dataSampleRef)
    {
        ret = SetDefault(path, IO_DATA_TYPE_NUMERIC, dataSampleRef);
    }
    else
    {
        LE_ERROR("Failed to set default numeric value on path:'%s'", path);
        ret = LE_NO_MEMORY;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a string value.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If could not set default due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch.
 *      - LE_FAULT If setting default failed becasue of any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetStringDefault
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    const char* value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    dataSample_Ref_t dataSampleRef = dataSample_CreateString(0, value);
    le_result_t ret;
    if (dataSampleRef)
    {
        ret = SetDefault(path, IO_DATA_TYPE_STRING, dataSampleRef);
    }
    else
    {
        LE_ERROR("Failed to set default string value on path:'%s'", path);
        ret = LE_NO_MEMORY;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource to a JSON value.
 *
 * @return
 *      - LE_OK If setting default was successful.
 *      - LE_NO_MEMORY If could not set default due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set default value due to type or unit mismatch or JSON is
 *             invalid.
 *      - LE_FAULT If setting default failed becasue of any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetJsonDefault
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    const char* value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t ret;
    if (json_IsValid(value))
    {
        dataSample_Ref_t dataSampleRef = dataSample_CreateJson(0, value);
        if (dataSampleRef)
        {
            ret = SetDefault(path, IO_DATA_TYPE_JSON, dataSampleRef);
        }
        else
        {
            LE_ERROR("Failed to set default JSON value on path:'%s'", path);
            ret = LE_NO_MEMORY;
        }
    }
    else
    {
        LE_ERROR("Discarding invalid JSON value '%s'.", value);
        ret = LE_BAD_PARAMETER;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Discover whether a given resource has a default value.
 *
 * @return true if there is a default value set, false if not.
 */
//--------------------------------------------------------------------------------------------------
bool admin_HasDefault
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return false;
    }
    else
    {
        return resTree_HasDefault(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the default value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t admin_GetDefaultDataType
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return IO_DATA_TYPE_TRIGGER;
    }
    else
    {
        return resTree_GetDefaultDataType(resEntry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the default value of a resource, if it is Boolean.
 *
 * @return the default value, or false if not set or set to another data type.
 */
//--------------------------------------------------------------------------------------------------
bool admin_GetBooleanDefault
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return false;
    }
    else
    {
        dataSample_Ref_t defaultValue = resTree_GetDefaultValue(resEntry);

        if (   (defaultValue == NULL)
            || (resTree_GetDefaultDataType(resEntry) != IO_DATA_TYPE_BOOLEAN)  )
        {
            return false;
        }

        return dataSample_GetBoolean(defaultValue);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the default value, if it is numeric.
 *
 * @return the default value, or NAN (not a number) if not set or set to another data type.
 */
//--------------------------------------------------------------------------------------------------
double admin_GetNumericDefault
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return NAN;
    }
    else
    {
        dataSample_Ref_t defaultValue = resTree_GetDefaultValue(resEntry);

        if (   (defaultValue == NULL)
            || (resTree_GetDefaultDataType(resEntry) != IO_DATA_TYPE_NUMERIC)  )
        {
            return NAN;
        }

        return dataSample_GetNumeric(defaultValue);
    }
}


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
    const char* path,
        ///< [IN] Absolute path of the resource.
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return LE_NOT_FOUND;
    }
    else
    {
        dataSample_Ref_t defaultValue = resTree_GetDefaultValue(resEntry);

        if (   (defaultValue == NULL)
            || (resTree_GetDefaultDataType(resEntry) != IO_DATA_TYPE_STRING)  )
        {
            return LE_NOT_FOUND;
        }

        return le_utf8_Copy(value, dataSample_GetString(defaultValue), valueSize, NULL);
    }
}


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
    const char* path,
        ///< [IN] Absolute path of the resource.
    char* value,
        ///< [OUT]
    size_t valueSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return LE_NOT_FOUND;
    }
    else
    {
        dataSample_Ref_t defaultValue = resTree_GetDefaultValue(resEntry);

        if (defaultValue == NULL)
        {
            return LE_NOT_FOUND;
        }

        return dataSample_ConvertToJson(defaultValue,
                                        resTree_GetDefaultDataType(resEntry),
                                        value,
                                        valueSize);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove any default value on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveDefault
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry != NULL) && (resTree_IsResource(resEntry)))
    {
        resTree_RemoveDefault(resEntry);
    }
}


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
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetOverride
(
    const char* path, ///< [IN] Absolute path of the resource.
    io_DataType_t dataType,
    dataSample_Ref_t value
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry;
    le_result_t ret = resTree_GetResource(resTree_GetRoot(), path, &resEntry);

    if (ret != LE_OK)
    {
        LE_ERROR("Failed to get resource at '%s'.", path);
    }
    else
    {
        ret = resTree_SetOverride(resEntry, dataType, value);
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set an override of Boolean type on a given resource.
 *
 * @return
 *      - LE_OK If setting override was successful.
 *      - LE_NO_MEMORY If could not set override value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set override value due to type or unit mismatch.
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetBooleanOverride
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    bool value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t ret;
    dataSample_Ref_t dataSampleRef = dataSample_CreateBoolean(0, value);
    if (dataSampleRef)
    {
        ret = SetOverride(path, IO_DATA_TYPE_BOOLEAN, dataSampleRef);
        if (ret != LE_OK)
        {
            le_mem_Release(dataSampleRef);
        }
    }
    else
    {
        LE_ERROR("Failed to set boolean override value on path:'%s'", path);
        ret = LE_NO_MEMORY;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set an override of numeric type on a given resource.
 *
 * @return
 *      - LE_OK If setting override was successful.
 *      - LE_NO_MEMORY If could not set override value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set override value due to type or unit mismatch.
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetNumericOverride
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    double value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t ret;
    dataSample_Ref_t dataSampleRef = dataSample_CreateNumeric(0, value);
    if (dataSampleRef)
    {
        ret = SetOverride(path, IO_DATA_TYPE_NUMERIC, dataSampleRef);
        if (ret != LE_OK)
        {
            le_mem_Release(dataSampleRef);
        }
    }
    else
    {
        LE_ERROR("Failed to set boolean override value on path:'%s'", path);
        ret = LE_NO_MEMORY;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set an override of string type on a given resource.
 *
 * @return
 *      - LE_OK If setting override was successful.
 *      - LE_NO_MEMORY If could not set override value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set override value due to type or unit mismatch.
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetStringOverride
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    const char* value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t ret;
    dataSample_Ref_t dataSampleRef = dataSample_CreateString(0, value);
    if (dataSampleRef)
    {
        ret = SetOverride(path, IO_DATA_TYPE_STRING, dataSampleRef);
        if (ret != LE_OK)
        {
            le_mem_Release(dataSampleRef);
        }
    }
    else
    {
        LE_ERROR("Failed to set string override value on path:'%s'", path);
        ret = LE_NO_MEMORY;
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set an override of JSON type on a given resource.
 *
 * @return
 *      - LE_OK If setting override was successful.
 *      - LE_NO_MEMORY If could not set override value due to lack of memory.
 *      - LE_BAD_PARAMETER If could not set override value due to type or unit mismatch or JSON was
 *              invalid.
 *      - LE_FAULT If any other error happened.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_SetJsonOverride
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    const char* value
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t ret;
    if (json_IsValid(value))
    {
        dataSample_Ref_t dataSampleRef = dataSample_CreateJson(0, value);
        if (dataSampleRef)
        {
            ret = SetOverride(path, IO_DATA_TYPE_JSON, dataSampleRef);
            if (ret != LE_OK)
            {
                le_mem_Release(dataSampleRef);
            }
        }
        else
        {
            LE_ERROR("Failed to set JSON override value on path:'%s'", path);
            ret = LE_NO_MEMORY;
        }
    }
    else
    {
        LE_ERROR("Discarding invalid JSON value '%s'.", value);
        ret = LE_BAD_PARAMETER;
    }
    return ret;
}


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
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return false;
    }
    else
    {
        return resTree_HasOverride(resEntry);
    }
}


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
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return IO_DATA_TYPE_TRIGGER;
    }
    else
    {
        return resTree_GetOverrideDataType(resEntry);
    }
}



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
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return false;
    }
    else
    {
        dataSample_Ref_t value = resTree_GetOverrideValue(resEntry);

        if (   (value == NULL)
            || (resTree_GetOverrideDataType(resEntry) != IO_DATA_TYPE_BOOLEAN)  )
        {
            return false;
        }

        return dataSample_GetBoolean(value);
    }
}


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
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return NAN;
    }
    else
    {
        dataSample_Ref_t value = resTree_GetOverrideValue(resEntry);

        if (   (value == NULL)
            || (resTree_GetOverrideDataType(resEntry) != IO_DATA_TYPE_NUMERIC)  )
        {
            return NAN;
        }

        return dataSample_GetNumeric(value);
    }
}


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
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return LE_NOT_FOUND;
    }
    else
    {
        dataSample_Ref_t overrideValue = resTree_GetOverrideValue(resEntry);

        if (   (overrideValue == NULL)
            || (resTree_GetOverrideDataType(resEntry) != IO_DATA_TYPE_STRING)  )
        {
            return LE_NOT_FOUND;
        }

        return le_utf8_Copy(value, dataSample_GetString(overrideValue), valueSize, NULL);
    }
}


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
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry == NULL) || (!resTree_IsResource(resEntry)))
    {
        return LE_NOT_FOUND;
    }
    else
    {
        dataSample_Ref_t overrideValue = resTree_GetOverrideValue(resEntry);

        if (overrideValue == NULL)
        {
            return LE_NOT_FOUND;
        }

        return dataSample_ConvertToJson(overrideValue,
                                        resTree_GetOverrideDataType(resEntry),
                                        value,
                                        valueSize);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove any override on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveOverride
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t resEntry = resTree_FindEntryAtAbsolutePath(path);

    if ((resEntry != NULL) && (resTree_IsResource(resEntry)))
    {
        resTree_RemoveOverride(resEntry);
    }
}


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
    const char* path,
        ///< [IN] Absolute path of the resource.
    char* child,
        ///< [OUT] Absolute path of the first child resource.
    size_t childSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t parentEntry = resTree_FindEntryAtAbsolutePath(path);

    if (parentEntry == NULL)
    {
        return LE_NOT_FOUND;
    }

    resTree_EntryRef_t childEntry = resTree_GetFirstChild(parentEntry);

    if (childEntry == NULL)
    {
        return LE_NOT_FOUND;
    }

    le_result_t result = resTree_GetPath(child, childSize, resTree_GetRoot(), childEntry);

    if (result >= 0)
    {
        return LE_OK;
    }
    else if (result == LE_OVERFLOW)
    {
        return LE_OVERFLOW;
    }

    LE_FATAL("Unexpected result: %d (%s)", result, LE_RESULT_TXT(result));
}


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
    const char* path,
        ///< [IN] Absolute path of the resource.
    char* sibling,
        ///< [OUT] Absolute path of the next sibling resource.
    size_t siblingSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entryRef = resTree_FindEntryAtAbsolutePath(path);

    if (entryRef == NULL)
    {
        return LE_NOT_FOUND;
    }

    resTree_EntryRef_t siblingRef = resTree_GetNextSibling(entryRef);

    if (siblingRef == NULL)
    {
        return LE_NOT_FOUND;
    }

    le_result_t result = resTree_GetPath(sibling, siblingSize, resTree_GetRoot(), siblingRef);

    if (result >= 0)
    {
        return LE_OK;
    }
    else if (result == LE_OVERFLOW)
    {
        return LE_OVERFLOW;
    }

    LE_FATAL("Unexpected result: %d (%s)", result, LE_RESULT_TXT(result));
}



//--------------------------------------------------------------------------------------------------
/**
 * Find out what type of entry lives at a given path in the resource tree.
 *
 * @return The entry type. ADMIN_ENTRY_TYPE_NONE if there's no entry at the given path.
 */
//--------------------------------------------------------------------------------------------------
admin_EntryType_t admin_GetEntryType
(
    const char* path
        ///< [IN] Absolute path of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);

    if (entry == NULL)
    {
        return ADMIN_ENTRY_TYPE_NONE;
    }
    else
    {
        return resTree_GetEntryType(entry);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Find out what units a given resource has.
 *
 * An empty string ("") means the units are unspecified for this resource.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_NOT_FOUND if there's no resource at the given path.
 *  - LE_OVERFLOW if the buffer provided is too small to hold the units string.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetUnits
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    char* unitsBuff,
        ///< [OUT] Buffer to store the units string in.
    size_t unitsBuffSize
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);

    if ((entry == NULL) || (!resTree_IsResource(entry)))
    {
        return LE_NOT_FOUND;
    }

    const char* units = resTree_GetUnits(entry);

    if (le_utf8_Copy(unitsBuff, units, unitsBuffSize, NULL) != LE_OK)
    {
        LE_ERROR("Units string buffer too short.");
        return LE_OVERFLOW;
    }
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Find out what data type a given resource currently has.
 *
 * Note that the data type of Inputs and Outputs are set by the app that creates those resources.
 * All other resources will change data types as values are pushed to them.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_NOT_FOUND if there's no resource at the given path.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_GetDataType
(
    const char* path,
        ///< [IN] Absolute path of the resource.
    io_DataType_t* dataTypePtr
        ///< [OUT] The data type, if LE_OK is returned.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entry = resTree_FindEntryAtAbsolutePath(path);

    if ((entry == NULL) || (!resTree_IsResource(entry)))
    {
        return LE_NOT_FOUND;
    }

    *dataTypePtr = resTree_GetDataType(entry);

    return LE_OK;
}
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
)
//--------------------------------------------------------------------------------------------------
{
    ResourceTreeChangeHandler_t* handlerPtr = NULL;
    // When there is a maximum count defined for the resource tree change handler, datahub can only
    // register that many handlers and further attempts to allocate a handler will be rejected.
    // In this case, le_mem_TryAlloc is used for safe allocation.
    // When there is no maximum count defined, le_mem_Alloc is used so the
    // ResourceTreeChangeHandlerPool can either grow to support more handlers than it was initially
    // configured for or cause an assertion depending on system configuration.
#ifdef DHUB_ADMIN_RESOURCE_TREE_CHANGE_HANDLER_COUNT
    handlerPtr = le_mem_TryAlloc(ResourceTreeChangeHandlerPool);
#else
    handlerPtr = le_mem_Alloc(ResourceTreeChangeHandlerPool);
#endif
    if (handlerPtr != NULL)
    {
        handlerPtr->link = LE_DLS_LINK_INIT;

        handlerPtr->callback = callbackPtr;
        handlerPtr->contextPtr = contextPtr;

        le_dls_Queue(&ResourceTreeChangeHandlerList, &handlerPtr->link);
    }
    else
    {
        LE_WARN("Cannot add any more resource tree change handlers. Rejecting request.");
    }
    return (admin_ResourceTreeChangeHandlerRef_t)handlerPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'admin_ResourceTreeChange'
 */
//--------------------------------------------------------------------------------------------------
void admin_RemoveResourceTreeChangeHandler
(
    admin_ResourceTreeChangeHandlerRef_t handlerRef
        ///< [IN]
)
//--------------------------------------------------------------------------------------------------
{
    ResourceTreeChangeHandler_t* handlerPtr = (ResourceTreeChangeHandler_t*)handlerRef;

    le_dls_Remove(&ResourceTreeChangeHandlerList, &handlerPtr->link);

    le_mem_Release(handlerPtr);
}
//--------------------------------------------------------------------------------------------------
/**
 * Call all the registered Resource Tree Change Handlers.
 */
//--------------------------------------------------------------------------------------------------
void admin_CallResourceTreeChangeHandlers
(
    const char* path,
    admin_EntryType_t entryType,
    admin_ResourceOperationType_t resourceOperationType
)
//--------------------------------------------------------------------------------------------------
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&ResourceTreeChangeHandlerList);

    while (linkPtr != NULL)
    {
        ResourceTreeChangeHandler_t* handlerPtr = CONTAINER_OF(linkPtr, ResourceTreeChangeHandler_t, link);

        handlerPtr->callback(path, entryType, resourceOperationType, handlerPtr->contextPtr);

        linkPtr = le_dls_PeekNext(&ResourceTreeChangeHandlerList, linkPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initializes the module.  Must be called before any other functions in the module are called.
 */
//--------------------------------------------------------------------------------------------------
void adminService_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
#ifdef DHUB_ADMIN_RESOURCE_TREE_CHANGE_HANDLER_COUNT
    // If a maximum is defined for resource tree change handlers, the pool size must be equal to the
    // maximum value.
    static_assert(LE_MEM_BLOCKS(ResourceTreeChangeHandlerPool,
                                DEFAULT_RESOURCE_TREE_CHANGE_HANDLER_POOL_SIZE) == \
                  DHUB_ADMIN_RESOURCE_TREE_CHANGE_HANDLER_COUNT,
                  "Size of ResourceTreeChangeHandlerPool is not equal to "
                  "DHUB_ADMIN_RESOURCE_TREE_CHANGE_HANDLER_COUNT");
#endif
    ResourceTreeChangeHandlerPool = le_mem_InitStaticPool(ResourceTreeChangeHandlerPool,
        DEFAULT_RESOURCE_TREE_CHANGE_HANDLER_POOL_SIZE, sizeof(ResourceTreeChangeHandler_t));
}

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
)
//--------------------------------------------------------------------------------------------------
{
    LE_INFO("Data Hub administrative updates starting.");

    ioService_StartUpdate();

    res_StartUpdate();
}


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
)
//--------------------------------------------------------------------------------------------------
{
    LE_INFO("Data Hub administrative updates complete.");

    ioService_EndUpdate();

    res_EndUpdate();
}

//--------------------------------------------------------------------------------------------------
/**
 * Extract the application name from an absolute path.
 * eg. "/app/foo/sensor/value" => "foo"
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_BAD_PARAMETER if the path is not absolute.
 *  - LE_OVERFLOW if appName buffer is not large enough to contain the application name.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ExtractAppNameFromAbsolutePath
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    char* appName,
        ///< [OUT] The extracted application name.
    size_t appNameSize
        ///< [IN] Size of the appName buffer.
)
{
    // Verify the path is absolute
    if (strncmp(path, "/app/", strlen("/app/")))
    {
        LE_ERROR("Path '%s' is not absolute.", path);
        return LE_BAD_PARAMETER;
    }

    // Skip "/app/"
    size_t i = 5; // Index into path.

    // Look for next slash
    const char* terminatorPtr = strchrnul(path + i, '/');

    // Compute the length of the entry name in this path element.
    size_t nameLen = terminatorPtr - (path + i);

    // Sanity check the length.
    if (nameLen == 0)
    {
        LE_ERROR("Resource path element missing in path '%s'.", path);
        return LE_BAD_PARAMETER;
    }
    if (nameLen >= appNameSize)
    {
        LE_ERROR("Resource path element too long in path '%s'.", path);
        return LE_OVERFLOW;
    }

    // Copy the name out and null terminate it.
    (void)strncpy(appName, path + i, nameLen);
    appName[nameLen] = '\0';

    return LE_OK;
}

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
 *  - LE_FAULT if the path is not absolute.
 */
//--------------------------------------------------------------------------------------------------
le_result_t admin_CreateInput
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    io_DataType_t dataType,
        ///< [IN] The data type.
    const char* LE_NONNULL units
        ///< [IN] e.g., "degC" (see senml); "" = unspecified.
)
{
    LE_DEBUG("'%s' <%s> '%s'.", path, hub_GetDataTypeName(dataType), units);

    resTree_EntryRef_t resRef = resTree_FindEntryAtAbsolutePath(path);

    // Check for another resource having the same name in the same namespace.
    if (resRef != NULL)
    {
        switch (resTree_GetEntryType(resRef))
        {
            case ADMIN_ENTRY_TYPE_INPUT:

                // Check data type and units to see if they match.
                if (   (resTree_GetDataType(resRef) != dataType)
                    || (strcmp(units, resTree_GetUnits(resRef)) != 0))
                {
                    return LE_DUPLICATE;
                }

                // The object already exists.  Nothing more needs to be done.
                return LE_OK;

            case ADMIN_ENTRY_TYPE_OUTPUT:
            case ADMIN_ENTRY_TYPE_OBSERVATION:

                // These conflict.
                return LE_DUPLICATE;

            case ADMIN_ENTRY_TYPE_NAMESPACE:
            case ADMIN_ENTRY_TYPE_PLACEHOLDER:

                // These can be upgraded to Input objects by resTree_CreateInput().
                break;

            case ADMIN_ENTRY_TYPE_NONE:

                LE_FATAL("Unexpected entry type.");
        }
    }

    // Get the "/app" namespace first.
    resTree_EntryRef_t nsRef;
    LE_ASSERT(resTree_GetEntry(resTree_GetRoot(), "app", &nsRef) == LE_OK);

    // Extract the app name from absolute path.
    char appName[HUB_MAX_ENTRY_NAME_BYTES];
    if (LE_OK != ExtractAppNameFromAbsolutePath(path, appName, sizeof(appName)))
    {
        LE_ERROR("Failed to extract app name from abs path '%s'.", path);
        return LE_FAULT;
    }

    LE_DEBUG("App name is '%s'.", appName);

    // Now get the app's namespace under the /app namespace.
    LE_ASSERT(resTree_GetEntry(nsRef, appName, &nsRef) == LE_OK);

    // Create the Input resource. Need to convert absolute path to relative one.
    le_result_t ret = resTree_CreateInput(nsRef, path + strlen("/app/") + strlen(appName) + strlen("/"), dataType, units);
    if (ret < 0)
    {
        LE_ERROR("Failed to create Input '/app/%s/%s'.", resTree_GetEntryName(nsRef), path);
        return LE_FAULT;
    }

    return LE_OK;
}

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
    const char* path,
        ///< [IN] Absolute resource tree path.
    io_DataType_t dataType,
        ///< [IN] The data type.
    const char* LE_NONNULL units
        ///< [IN] e.g., "degC" (see senml); "" = unspecified.
)
{
    LE_DEBUG("'%s' <%s> '%s'.", path, hub_GetDataTypeName(dataType), units);

    resTree_EntryRef_t resRef = resTree_FindEntryAtAbsolutePath(path);

    if (resRef != NULL)
    {
        switch (resTree_GetEntryType(resRef))
        {
            case ADMIN_ENTRY_TYPE_OUTPUT:

                // Check data type and units to see if they match.
                if (   (resTree_GetDataType(resRef) != dataType)
                    || (strcmp(units, resTree_GetUnits(resRef)) != 0))
                {
                    return LE_DUPLICATE;
                }

                // The object already exists.  Nothing more needs to be done.
                return LE_OK;

            case ADMIN_ENTRY_TYPE_INPUT:
            case ADMIN_ENTRY_TYPE_OBSERVATION:

                // These conflict.
                return LE_DUPLICATE;

            case ADMIN_ENTRY_TYPE_NAMESPACE:
            case ADMIN_ENTRY_TYPE_PLACEHOLDER:

                // These can be upgraded to Output objects by resTree_CreateOutput().
                break;

            case ADMIN_ENTRY_TYPE_NONE:

                LE_FATAL("Unexpected entry type.");
        }
    }

    // Get the "/app" namespace first.
    resTree_EntryRef_t nsRef;
    LE_ASSERT(resTree_GetEntry(resTree_GetRoot(), "app", &nsRef) == LE_OK);

    // Extract the app name from absolute path.
    char appName[HUB_MAX_ENTRY_NAME_BYTES];
    if (LE_OK != ExtractAppNameFromAbsolutePath(path, appName, sizeof(appName)))
    {
        LE_ERROR("Failed to extract app name from abs path '%s'.", path);
        return LE_FAULT;
    }

    LE_DEBUG("App name is '%s'.", appName);

    // Now get the app's namespace under the /app namespace.
    LE_ASSERT(resTree_GetEntry(nsRef, appName, &nsRef) == LE_OK);

    // Create the Output resource. Need to convert absolute path to relative one.
    le_result_t ret = resTree_CreateOutput(nsRef, path + strlen("/app/") + strlen(appName) + strlen("/"), dataType, units);
    if (ret < 0)
    {
        LE_ERROR("Failed to create Output '/app/%s/%s'.", resTree_GetEntryName(nsRef), path);
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a resource.
 *
 * Does nothing if the resource doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
void admin_DeleteResource
(
    const char* path
        ///< [IN] Absolute resource tree path.
)
{
    LE_DEBUG("'%s'", path);

    // If the resource exists, delete it.
    resTree_EntryRef_t resRef = resTree_FindEntryAtAbsolutePath(path);
    if (resRef != NULL)
    {
        resTree_DeleteIO(resRef);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the example value for a JSON-type Input resource.
 *
 * Does nothing if the resource is not found, is not an input, or doesn't have a JSON type.
 */
//--------------------------------------------------------------------------------------------------
void admin_SetJsonExample
(
    const char* path,
        ///< [IN] Absolute resource tree path.
    const char* example
        ///< [IN] The example JSON value string.
)
{
    resTree_EntryRef_t resRef = resTree_FindEntryAtAbsolutePath(path);
    if (resRef == NULL)
    {
        LE_ERROR("Resource '%s' does not exist.", path);
    }
    else if (resTree_GetEntryType(resRef) != ADMIN_ENTRY_TYPE_INPUT)
    {
        LE_ERROR("Resource '%s' is not an input.", path);
    }
    else if (resTree_GetDataType(resRef) != IO_DATA_TYPE_JSON)
    {
        LE_ERROR("Resource '%s' does not have JSON data type.", path);
    }
    else
    {
        dataSample_Ref_t sampleRef = dataSample_CreateJson(0, example);

        if (sampleRef != NULL)
        {
            resTree_SetJsonExample(resRef, sampleRef);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void admin_MarkOptional
(
    const char* path
        ///< [IN] Absolute resource tree path.
)
{
    resTree_EntryRef_t resRef = resTree_FindEntryAtAbsolutePath(path);
    if (resRef == NULL)
    {
        LE_ERROR("Attempt to mark non-existent resource optional at '%s'.", path);
    }
    else if (resTree_GetEntryType(resRef) != ADMIN_ENTRY_TYPE_OUTPUT)
    {
        LE_ERROR("Attempt to mark non-Output resource optional at '%s'.", path);
    }
    else
    {
        resTree_MarkOptional(resRef);
    }
}
