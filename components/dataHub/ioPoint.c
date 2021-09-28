//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Input and Output Resources.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "dataHub.h"
#include "resource.h"
#include "ioPoint.h"
#include "json.h"


//--------------------------------------------------------------------------------------------------
/**
 * An Input or Output Resource.
 */
//--------------------------------------------------------------------------------------------------
typedef struct ioResource
{
    res_Resource_t resource;    ///< Base class. ** MUST BE FIRST **
    io_DataType_t dataType;     ///< Data type of this resource.
    le_dls_List_t pollHandlerList;  ///< List of Poll Handler callbacks the client app registered.
    bool isMandatory;   ///< true = this is a mandatory output; false otherwise.
}
IoResource_t;

/// Default number of IO resources.  This can be overridden in the .cdef.
#define DEFAULT_IO_RESOURCE_POOL_SIZE 10

//--------------------------------------------------------------------------------------------------
/**
 * Pool from which I/O Resource objects are allocated.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t IoResourcePool = NULL;
LE_MEM_DEFINE_STATIC_POOL(IoResourcePool, DEFAULT_IO_RESOURCE_POOL_SIZE, sizeof(IoResource_t));


//--------------------------------------------------------------------------------------------------
/**
 * Destructor for IoResource_t objects.
 */
//--------------------------------------------------------------------------------------------------
static void IoResourceDestructor
(
    void* objPtr
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = objPtr;

    res_Destruct(&ioPtr->resource);
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the I/O Resource module.
 *
 * @warning This function MUST be called before any others in this module.
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    IoResourcePool = le_mem_InitStaticPool(IoResourcePool, DEFAULT_IO_RESOURCE_POOL_SIZE,
                        sizeof(IoResource_t));
    le_mem_SetDestructor(IoResourcePool, IoResourceDestructor);
}


//--------------------------------------------------------------------------------------------------
/**
 * Create an Input/Output Resource object.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
IoResource_t* Create
(
    io_DataType_t dataType,
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = hub_MemAlloc(IoResourcePool);

    if (ioPtr)
    {
        res_Construct(&ioPtr->resource, entryRef);

        ioPtr->pollHandlerList = LE_DLS_LIST_INIT;

        ioPtr->dataType = dataType;
        ioPtr->isMandatory = false;
    }
    return ioPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create an Input Resource.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* ioPoint_CreateInput
(
    io_DataType_t dataType,
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
)
//--------------------------------------------------------------------------------------------------
{
    res_Resource_t* ret = NULL;
    IoResource_t* ioPtr = Create(dataType, entryRef);
    if (ioPtr)
    {
        ret = &(ioPtr->resource);
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create an Output Resource.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* ioPoint_CreateOutput
(
    io_DataType_t dataType,
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
)
//--------------------------------------------------------------------------------------------------
{
    res_Resource_t* ret = NULL;
    IoResource_t* ioPtr = Create(dataType, entryRef);

    if (ioPtr)
    {
        // By default, all outputs are mandatory.
        ioPtr->isMandatory = true;
        ret = &(ioPtr->resource);
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a placeholder IO Resource.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* ioPoint_CreatePlaceholderIO
(
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
)
//--------------------------------------------------------------------------------------------------
{
    res_Resource_t* ret = NULL;
    IoResource_t* ioPtr = Create(IO_DATA_TYPE_TRIGGER, entryRef);

    if (ioPtr)
    {
        ret = &(ioPtr->resource);
    }
    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of an Input or Output resource.
 *
 * @return The data type.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t ioPoint_GetDataType
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = CONTAINER_OF(resPtr, IoResource_t, resource);

    return ioPtr->dataType;
}

//--------------------------------------------------------------------------------------------------
/**
 * Make a placeholder resource an input
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_MakeResourceInput
(
    res_Resource_t* resPtr,   ///< Pointer to resource
    io_DataType_t dataType    ///< Io resource data type.
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = CONTAINER_OF(resPtr, IoResource_t, resource);
    ioPtr->dataType = dataType;
}

//--------------------------------------------------------------------------------------------------
/**
 * Make a placeholder resource an output
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_MakeResourceOutput
(
    res_Resource_t* resPtr,   ///< Pointer to resource
    io_DataType_t dataType    ///< Io resource data type.
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = CONTAINER_OF(resPtr, IoResource_t, resource);
    ioPtr->dataType = dataType;
    ioPtr->isMandatory = true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Perform type coercion, replacing a data sample with another of a different type, if necessary,
 * to make the data compatible with the data type of a given Input or Output resource.
 *
 * @return
 *      - LE_OK If coercion happened successfully.
 *      - LE_NO_MEMORY If could not coerce to a new type because failed to allocate a new datasample
 */
//--------------------------------------------------------------------------------------------------
le_result_t ioPoint_DoTypeCoercion
(
    res_Resource_t* resPtr,
    io_DataType_t* dataTypePtr,     ///< [INOUT] the data type, may be changed by type coercion
    dataSample_Ref_t* valueRefPtr   ///< [INOUT] the data sample, may be replaced by type coercion
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = CONTAINER_OF(resPtr, IoResource_t, resource);

    io_DataType_t fromType = *dataTypePtr;
    dataSample_Ref_t fromSample = *valueRefPtr;

    io_DataType_t toType = ioPtr->dataType;
    dataSample_Ref_t toSample = NULL;
    bool needsTypeConversion = true;
    double timestamp = dataSample_GetTimestamp(fromSample);

    switch (toType)
    {
        case IO_DATA_TYPE_TRIGGER:

            // If the pushed sample is not a trigger, then create a new trigger sample with the
            // same timestamp as the original sample.  Otherwise no type conversion required.
            if (fromType != IO_DATA_TYPE_TRIGGER)
            {
                le_mem_Release(fromSample);
                toSample = dataSample_CreateTrigger(timestamp);
            }
            else
            {
                needsTypeConversion = false;
            }
            break;

        case IO_DATA_TYPE_BOOLEAN:

            switch (fromType)
            {
                case IO_DATA_TYPE_TRIGGER:

                    // If the pushed sample is a trigger, just use false.
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateBoolean(timestamp, false);
                    break;

                case IO_DATA_TYPE_BOOLEAN:
                    needsTypeConversion = false;
                    break;  // No conversion required.

                case IO_DATA_TYPE_NUMERIC:
                {
                    double value = dataSample_GetNumeric(fromSample);
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateBoolean(timestamp, (value != 0));
                    break;
                }

                case IO_DATA_TYPE_STRING:
                {
                    const char* value = dataSample_GetString(fromSample);
                    bool boolValue = (value[0] != '\0');
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateBoolean(timestamp, boolValue);
                    break;
                }

                case IO_DATA_TYPE_JSON:
                {
                    bool newValue = json_ConvertToBoolean(dataSample_GetJson(fromSample));
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateBoolean(timestamp, newValue);
                    break;
                }
            }

            break;

        case IO_DATA_TYPE_NUMERIC:

            switch (fromType)
            {
                case IO_DATA_TYPE_TRIGGER:

                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateNumeric(timestamp, NAN);
                    break;

                case IO_DATA_TYPE_BOOLEAN:
                {
                    double newValue = (dataSample_GetBoolean(fromSample) ? 1 : 0);
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateNumeric(timestamp, newValue);
                    break;
                }

                case IO_DATA_TYPE_NUMERIC:
                    needsTypeConversion = false;
                    break;  // No conversion required.

                case IO_DATA_TYPE_STRING:
                {
                    double newValue = (dataSample_GetString(fromSample)[0] == '\0' ? 0 : 1);
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateNumeric(timestamp, newValue);
                    break;
                }

                case IO_DATA_TYPE_JSON:
                {
                    double newValue = json_ConvertToNumber(dataSample_GetJson(fromSample));
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateNumeric(timestamp, newValue);
                    break;
                }
            }
            break;

        case IO_DATA_TYPE_STRING:

            switch (fromType)
            {
                case IO_DATA_TYPE_TRIGGER:

                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateString(timestamp, "");
                    break;

                case IO_DATA_TYPE_BOOLEAN:
                {
                    bool oldValue = dataSample_GetBoolean(fromSample);
                    const char* newValue = oldValue ? "true" : "false";
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateString(timestamp, newValue);
                    break;
                }

                case IO_DATA_TYPE_NUMERIC:
                {
                    double oldValue = dataSample_GetNumeric(fromSample);
                    char newValue[HUB_MAX_STRING_BYTES];
                    if (snprintf(newValue, sizeof(newValue), "%lf", oldValue) >=
                        (int) sizeof(newValue))
                    {
                        // Should never happen.
                        LE_CRIT("String overflow.");
                        newValue[0] = '\0';
                    }
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateString(timestamp, newValue);
                    break;
                }

                case IO_DATA_TYPE_STRING:
                    needsTypeConversion = false;
                    break;  // No conversion required.

                case IO_DATA_TYPE_JSON:
                {
                    // We need to unescape the string
                    const char* oldValue = dataSample_GetJson(fromSample);
                    char newValue[HUB_MAX_STRING_BYTES];
                    if (LE_OK == dataSample_JsonToString(newValue, oldValue, sizeof(newValue), NULL))
                    {
                        toSample = dataSample_CreateString(timestamp, newValue);
                    }
                    else
                    {
                        toSample = NULL;
                    }
                    le_mem_Release(fromSample);
                    break;
                }
            }
            break;

        case IO_DATA_TYPE_JSON:

            switch (fromType)
            {
                case IO_DATA_TYPE_TRIGGER:
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateJson(timestamp, "null");
                    break;

                case IO_DATA_TYPE_BOOLEAN:
                {
                    bool oldValue = dataSample_GetBoolean(fromSample);
                    const char* newValue = oldValue ? "true" : "false";
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateJson(timestamp, newValue);
                    break;
                }

                case IO_DATA_TYPE_NUMERIC:
                {
                    double oldValue = dataSample_GetNumeric(fromSample);
                    char newValue[HUB_MAX_STRING_BYTES];
                    if (snprintf(newValue, sizeof(newValue), "%lf", oldValue) >=
                        (int) sizeof(newValue))
                    {
                        // Should never happen.
                        LE_CRIT("String overflow.");
                        newValue[0] = '\0';
                    }
                    le_mem_Release(fromSample);
                    toSample = dataSample_CreateJson(timestamp, newValue);
                    break;
                }

                case IO_DATA_TYPE_STRING:
                {
                    const char* oldValue = dataSample_GetString(fromSample);
                    char newValue[HUB_MAX_STRING_BYTES];
                    size_t len;

                    // Must wrap the string value in quotes.
                    // We need at least 3 bytes for the two quotes and a null terminator.
                    newValue[0] = '"';
                    le_result_t result = dataSample_StringToJson(newValue + 1, oldValue, sizeof(newValue) - 1 , &len);
                    if ((result != LE_OK) || (len >= (sizeof(newValue) - 1)))  // need 1 more for the last '"'
                    {
                        toSample = NULL;
                    }
                    else
                    {
                        newValue[len] = '"'; // replace null-terminator with '"'
                        newValue[len + 1] = '\0'; // null-terminate the string.

                        toSample = dataSample_CreateJson(timestamp, newValue);
                    }
                    le_mem_Release(fromSample);
                    break;
                }

                case IO_DATA_TYPE_JSON:
                    needsTypeConversion = false;
                    break;  // No conversion required.
            }
            break;
    }

    // If a conversion happened, release the old value and replace it with the new one.
    if (toSample != NULL)
    {
        *valueRefPtr = toSample;
        *dataTypePtr = toType;
        return LE_OK;
    }
    else if (needsTypeConversion)
    {
        return LE_NO_MEMORY;
    }
    else
    {
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_MarkOptional
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = CONTAINER_OF(resPtr, IoResource_t, resource);

    ioPtr->isMandatory = false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Check if a given resource is a mandatory output.  If so, it means that this is an output resource
 * that must have a value before the related app function will begin working.
 *
 * @return true if a mandatory output, false if it's an optional output or not an output at all.
 */
//--------------------------------------------------------------------------------------------------
bool ioPoint_IsMandatory
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    IoResource_t* ioPtr = CONTAINER_OF(resPtr, IoResource_t, resource);

    return ioPtr->isMandatory;
}
