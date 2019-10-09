//--------------------------------------------------------------------------------------------------
/**
 * @file handler.c
 *
 * Utilities for keeping track of registered call-backs ("Handlers").
 *
 * @Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "dataHub.h"
#include "handler.h"


//--------------------------------------------------------------------------------------------------
/**
 * Holds the details of a Handler callback that has been registered by a client app.
 */
//--------------------------------------------------------------------------------------------------
typedef struct handler
{
    le_dls_Link_t link; ///< Used to link into one of the I/O resource's lists of handlers.
    void* safeRef;      ///< Safe reference passed to client.
    le_dls_List_t* listPtr; ///< Ptr to the list this handler is on.
    io_DataType_t dataType;    ///< Data type of the handler callback (only for Push handlers).
    void* callbackPtr;  ///< The callback function pointer.
    void* contextPtr;   ///< The context pointer provided by the client.
}
Handler_t;

/// Default number of push handlers.  This can be overridden in the .cdef.
#define DEFAULT_PUSH_HANDLER_POOL_SIZE  10

/// Size of the push handler reference map.
#define PUSH_HANDLER_MAP_SIZE           LE_MEM_BLOCKS(HandlerPool, DEFAULT_PUSH_HANDLER_POOL_SIZE)

//--------------------------------------------------------------------------------------------------
/**
 * Pool from which Handler objects are allocated.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t HandlerPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(HandlerPool, DEFAULT_PUSH_HANDLER_POOL_SIZE, sizeof(Handler_t));


//--------------------------------------------------------------------------------------------------
/**
 * Safe reference map for Handler objects.  Used to generate safe references to pass to clients
 * when they register Poll and Push handler call-backs.
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t HandlerRefMap = NULL;
LE_REF_DEFINE_STATIC_MAP(HandlerRefMap, PUSH_HANDLER_MAP_SIZE);


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Handler module.
 *
 * @warning This function must be called before any others in this module.
 */
//--------------------------------------------------------------------------------------------------
void handler_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
#ifdef DHUB_IO_PUSH_HANDLER_COUNT
#ifdef DHUB_ADMIN_PUSH_HANDLER_COUNT
#ifdef DHUB_QUERY_PUSH_HANDLER_COUNT
    static_assert(LE_MEM_BLOCKS(HandlerPool, DEFAULT_PUSH_HANDLER_POOL_SIZE) == \
                  (DHUB_IO_PUSH_HANDLER_COUNT + DHUB_ADMIN_PUSH_HANDLER_COUNT + \
                   DHUB_QUERY_PUSH_HANDLER_COUNT),
                  "Size of HandlerPool is not equal to sum of all push handler quotas.");
#endif
#endif
#endif
    HandlerPool = le_mem_InitStaticPool(HandlerPool, DEFAULT_PUSH_HANDLER_POOL_SIZE,
                    sizeof(Handler_t));

    HandlerRefMap = le_ref_InitStaticMap(HandlerRefMap, PUSH_HANDLER_MAP_SIZE);
}


//--------------------------------------------------------------------------------------------------
/**
 * Add a Handler to a given list.
 *
 * @return Reference to the handler added. NULL if failed to add handler.
 */
//--------------------------------------------------------------------------------------------------
hub_HandlerRef_t handler_Add
(
    le_dls_List_t* listPtr,
    io_DataType_t dataType,
    void* callbackPtr,
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    Handler_t* handlerPtr = hub_MemAlloc(HandlerPool);

    if (handlerPtr == NULL)
    {
        LE_WARN("Failed to allocate a Handler");
        return NULL;
    }

    handlerPtr->link = LE_DLS_LINK_INIT;
    handlerPtr->safeRef = le_ref_CreateRef(HandlerRefMap, handlerPtr);
    handlerPtr->listPtr = listPtr;
    handlerPtr->dataType = dataType;
    handlerPtr->callbackPtr = callbackPtr;
    handlerPtr->contextPtr = contextPtr;

    le_dls_Queue(listPtr, &handlerPtr->link);

    LE_DEBUG("Added Handler %p for %d", (hub_HandlerRef_t)handlerPtr->safeRef, dataType);

    return (hub_HandlerRef_t)(handlerPtr->safeRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a handler.
 *
 * @warning Be sure to remove the handler from its list before deleting it.
 */
//--------------------------------------------------------------------------------------------------
static void DeleteHandler
(
    Handler_t* handlerPtr
)
//--------------------------------------------------------------------------------------------------
{
    LE_DEBUG("Deleting handler %p", handlerPtr->safeRef);
    le_ref_DeleteRef(HandlerRefMap, handlerPtr->safeRef);

    le_mem_Release(handlerPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove a Handler from a given list.
 *
 * @return A pointer to the list that the handler was removed from, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
le_dls_List_t* handler_Remove
(
    hub_HandlerRef_t handlerRef
)
//--------------------------------------------------------------------------------------------------
{
    le_dls_List_t* listPtr = NULL;
    Handler_t* handlerPtr = le_ref_Lookup(HandlerRefMap, handlerRef);
    if (handlerPtr != NULL)
    {
        listPtr = handlerPtr->listPtr;
        le_dls_Remove(listPtr, &handlerPtr->link);

        DeleteHandler(handlerPtr);
    }
    else
    {
        LE_ERROR("Invalid handler reference %p. Cannot remove", handlerRef);
    }

    return listPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove all Handlers from a given list.
 */
//--------------------------------------------------------------------------------------------------
void handler_RemoveAll
(
    le_dls_List_t* listPtr
)
//--------------------------------------------------------------------------------------------------
{
    le_dls_Link_t* linkPtr;

    while (NULL != (linkPtr = le_dls_Pop(listPtr)))
    {
        DeleteHandler(CONTAINER_OF(linkPtr, Handler_t, link));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Call a given push handler, passing it a given data sample.
 */
//--------------------------------------------------------------------------------------------------
static void CallPushHandler
(
    Handler_t* handlerPtr,
    io_DataType_t dataType,     ///< Data type of the data sample.
    dataSample_Ref_t sampleRef  ///< Data sample.
)
//--------------------------------------------------------------------------------------------------
{
    if (handlerPtr->dataType == dataType)
    {
        double timestamp = dataSample_GetTimestamp(sampleRef);

        switch (dataType)
        {
            case IO_DATA_TYPE_TRIGGER:
            {
                io_TriggerPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
                callbackPtr(timestamp, handlerPtr->contextPtr);
                break;
            }

            case IO_DATA_TYPE_BOOLEAN:
            {
                io_BooleanPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
                callbackPtr(timestamp,
                            dataSample_GetBoolean(sampleRef),
                            handlerPtr->contextPtr);
                break;
            }

            case IO_DATA_TYPE_NUMERIC:
            {
                io_NumericPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
                callbackPtr(timestamp,
                            dataSample_GetNumeric(sampleRef),
                            handlerPtr->contextPtr);
                break;
            }

            case IO_DATA_TYPE_STRING:
            {
                io_StringPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
                callbackPtr(timestamp,
                            dataSample_GetString(sampleRef),
                            handlerPtr->contextPtr);
                break;
            }

            case IO_DATA_TYPE_JSON:
            {
                io_JsonPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
                callbackPtr(timestamp,
                            dataSample_GetJson(sampleRef),
                            handlerPtr->contextPtr);
                break;
            }
        }
    }
    else if (handlerPtr->dataType == IO_DATA_TYPE_STRING)
    {
        char value[HUB_MAX_STRING_BYTES];
        if (LE_OK != dataSample_ConvertToString(sampleRef,
                                                dataType,
                                                value,
                                                sizeof(value)) )
        {
            LE_ERROR("Conversion to string would result in string buffer overflow.");
        }
        else
        {
            double timestamp = dataSample_GetTimestamp(sampleRef);

            io_StringPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
            callbackPtr(timestamp,
                        value,
                        handlerPtr->contextPtr);
        }
    }
    else if (handlerPtr->dataType == IO_DATA_TYPE_JSON)
    {
        char value[HUB_MAX_STRING_BYTES];
        if (LE_OK != dataSample_ConvertToJson(sampleRef,
                                              dataType,
                                              value,
                                              sizeof(value)) )
        {
            LE_ERROR("Conversion to JSON would result in string buffer overflow.");
        }
        else
        {
            double timestamp = dataSample_GetTimestamp(sampleRef);

            io_JsonPushHandlerFunc_t callbackPtr = handlerPtr->callbackPtr;
            callbackPtr(timestamp,
                        value,
                        handlerPtr->contextPtr);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Call a given push handler, passing it a given data sample.
 */
//--------------------------------------------------------------------------------------------------
void handler_Call
(
    hub_HandlerRef_t handlerRef,
    io_DataType_t dataType,     ///< Data type of the data sample.
    dataSample_Ref_t sampleRef  ///< Data sample.
)
//--------------------------------------------------------------------------------------------------
{
    Handler_t* handlerPtr = le_ref_Lookup(HandlerRefMap, handlerRef);
    if (handlerPtr == NULL)
    {
        LE_CRIT("Invalid handler reference %p", handlerRef);
    }
    else
    {
        CallPushHandler(handlerPtr, dataType, sampleRef);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Call all the push handler functions in a given list that match a given data type.
 */
//--------------------------------------------------------------------------------------------------
void handler_CallAll
(
    le_dls_List_t* listPtr,         ///< List of push handlers
    io_DataType_t dataType,         ///< Data Type of the data sample
    dataSample_Ref_t sampleRef      ///< Data Sample to pass to the push handlers that are called.
)
//--------------------------------------------------------------------------------------------------
{
    // Iterate over the Push Handler List and call each one that is a data type match.
    le_dls_Link_t* linkPtr = le_dls_Peek(listPtr);

    while (linkPtr != NULL)
    {
        Handler_t* handlerPtr = CONTAINER_OF(linkPtr, Handler_t, link);

        CallPushHandler(handlerPtr, dataType, sampleRef);

        linkPtr = le_dls_PeekNext(listPtr, linkPtr);
    }
}
