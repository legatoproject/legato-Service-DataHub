//--------------------------------------------------------------------------------------------------
/**
 * @file handler.h
 *
 * Utilities for keeping track of registered call-backs ("Handlers").
 *
 * @Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef HANDLER_H_INCLUDE_GUARD
#define HANDLER_H_INCLUDE_GUARD


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
);


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
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove a Handler from whatever list it is on.
 *
 * @return A pointer to the list that the handler was removed from, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
le_dls_List_t* handler_Remove
(
    hub_HandlerRef_t handlerRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove all Handlers from a given list.
 */
//--------------------------------------------------------------------------------------------------
void handler_RemoveAll
(
    le_dls_List_t* listPtr
);


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
);


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
);

#endif // HANDLER_H_INCLUDE_GUARD
