//--------------------------------------------------------------------------------------------------
/**
 * @file dataSample.h
 *
 * Interface to the Data Sample module.
 *
 * Data Samples are reference counted. Every object that holds a pointer to a Data Sample must
 * increment the reference count on the Data Sample when it stores the pointer and release the
 * Data Sample when the pointer is no longer needed.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef DATA_SAMPLE_H_INCLUDE_GUARD
#define DATA_SAMPLE_H_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Data Samples are reference counted memory pool objects that hold a timestamp and an
 * optional value.
 *
 * Use le_mem_AddRef() and le_mem_Release() to control the reference counts.
 *
 * That's all you need to know about them if you're outside the Data Sample module.
 */
//--------------------------------------------------------------------------------------------------
typedef struct DataSample* dataSample_Ref_t;


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Data Sample module.
 *
 * @warning This function MUST be called before any other functions in this module are called.
 */
//--------------------------------------------------------------------------------------------------
void dataSample_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new Trigger type Data Sample.
 *
 * @return Ptr to the new object (with reference count 1) or NULL if failed to allocate memory.
 *
 * @note These are reference-counted memory pool objects.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t dataSample_CreateTrigger
(
    double timestamp
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new Boolean type Data Sample.
 *
 * @return Ptr to the new object or NULL if failed to allocate memory.
 *
 * @note These are reference-counted memory pool objects.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t dataSample_CreateBoolean
(
    double timestamp,
    bool value
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new Numeric type Data Sample.
 *
 * @return Ptr to the new object or NULL if failed to allocate memory.
 *
 * @note These are reference-counted memory pool objects.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t dataSample_CreateNumeric
(
    double timestamp,
    double value
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new String type Data Sample.
 *
 * @return Ptr to the new object or NULL if failed to allocate memory.
 *
 * @note Copies the string value into the Data Sample.
 *
 * @note These are reference-counted memory pool objects.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t dataSample_CreateString
(
    double timestamp,
    const char* value
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new JSON type Data Sample.
 *
 * @return Ptr to the new object or NULL if failed to allocate memory.
 *
 * @note Copies the JSON value into the Data Sample.
 *
 * @note These are reference-counted memory pool objects.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t dataSample_CreateJson
(
    double timestamp,
    const char* value
);


//--------------------------------------------------------------------------------------------------
/**
 * Read the timestamp on a Data Sample.
 *
 * @return The timestamp, in seconds since the Epoch (1970/01/01 00:00:00 UTC).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double dataSample_GetTimestamp
(
    dataSample_Ref_t sampleRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Read a Boolean value from a Data Sample.
 *
 * @return The value.
 *
 * @warning You had better be sure that this is a Boolean Data Sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool dataSample_GetBoolean
(
    dataSample_Ref_t sampleRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Read a numeric value from a Data Sample.
 *
 * @return The value.
 *
 * @warning You had better be sure that this is a Numeric Data Sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double dataSample_GetNumeric
(
    dataSample_Ref_t sampleRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Read a string value from a Data Sample.
 *
 * @return Ptr to the value. DO NOT use this after releasing your reference to the sample.
 *
 * @warning You had better be sure that this is a String Data Sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED const char* dataSample_GetString
(
    dataSample_Ref_t sampleRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Read a JSON value from a Data Sample.
 *
 * @return Ptr to the value. DO NOT use this after releasing your reference to the sample.
 *
 * @warning You had better be sure that this is a JSON Data Sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED const char* dataSample_GetJson
(
    dataSample_Ref_t sampleRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Read any type of value from a Data Sample, as a printable UTF-8 string.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t dataSample_ConvertToString
(
    dataSample_Ref_t sampleRef,
    io_DataType_t dataType, ///< [IN] The data type of the data sample.
    char* valueBuffPtr,     ///< [OUT] Ptr to buffer where value will be stored.
    size_t valueBuffSize    ///< [IN] Size of value buffer, in bytes.
);


//--------------------------------------------------------------------------------------------------
/**
 * Read any type of value from a Data Sample, in JSON format.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t dataSample_ConvertToJson
(
    dataSample_Ref_t sampleRef,
    io_DataType_t dataType, ///< [IN] The data type of the data sample.
    char* valueBuffPtr,     ///< [OUT] Ptr to buffer where value will be stored.
    size_t valueBuffSize    ///< [IN] Size of value buffer, in bytes.
);


//--------------------------------------------------------------------------------------------------
/**
 * Extract an object member or array element from a JSON data value, based on a given
 * extraction specifier.
 *
 * The extraction specifiers look like "x" or "x.y" or "[3]" or "x[3].y", etc.
 *
 * @return Reference to the extracted data sample, or NULL if failed.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t dataSample_ExtractJson
(
    dataSample_Ref_t sampleRef, ///< [IN] Original JSON data sample to extract from.
    const char* extractionSpec, ///< [IN] the extraction specification.
    io_DataType_t* dataTypePtr  ///< [OUT] Ptr to where to put the data type of the extracted object
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the timestamp of a Data Sample.
 */
//--------------------------------------------------------------------------------------------------
void dataSample_SetTimestamp
(
    dataSample_Ref_t sample,
    double timestamp
);

//--------------------------------------------------------------------------------------------------
/**
 * Copies the string in srcStr to the start of destStr and returns the number of bytes copied (not
 * including the NULL-terminator) in numBytesPtr.  Null can be passed into numBytesPtr if the number
 * of bytes copied is not needed.  The srcStr must be in UTF-8 format.
 *
 * A JSON string begins and ends with quotation marks.  All Unicode characters may be placed within
 * the quotation marks, except for the characters that must be escaped:
 * quotation mark, reverse solidus, and the control characters (U+0001 through U+001F).
 *
 * If the size of srcStr is less than or equal to the destination buffer size then the entire srcStr
 * will be copied including the null-character.  The rest of the destination buffer is not modified.
 *
 * If the size of srcStr is larger than the destination buffer then the maximum number of characters
 * (from srcStr) plus a null-character that will fit in the destination buffer is copied.
 *
 * UTF-8 characters may be more than one byte long and this function will only copy whole characters
 * not partial characters. Therefore, even if srcStr is larger than the destination buffer, the
 * copied characters may not fill the entire destination buffer because the last character copied
 * may not align exactly with the end of the destination buffer.
 *
 * The destination string will always be Null-terminated, unless destSize is zero.
 *
 * If destStr and srcStr overlap the behaviour of this function is undefined.
 *
 * @return
 *      - LE_OK if srcStr was completely copied to the destStr.
 *      - LE_OVERFLOW if srcStr was truncated when it was copied to destStr.
 */
//--------------------------------------------------------------------------------------------------
le_result_t dataSample_StringToJson
(
    char* destStr,          ///< [IN] Destination (JSON format) where the srcStr is to be copied.
    const char* srcStr,     ///< [IN] UTF-8 source string.
    const size_t destSize,  ///< [IN] Size of the destination buffer in bytes.
    size_t* numBytesPtr     ///< [OUT] Number of bytes copied not including the NULL-terminator.
                            ///        Parameter can be set to NULL if the number of bytes copied is
                            ///        not needed.
);

//--------------------------------------------------------------------------------------------------
/**
 * JSON to String conversion
 *
 * This function transforms a JSON string into its unescaped version.
 *
 * This function copies the string in srcStr to the start of destStr and returns the number of bytes
 * copied (not including the NULL-terminator) in numBytesPtr.  Null can be passed into numBytesPtr
 * if the number of bytes copied is not needed.
 *
 * Size of the destination buffer must be equal to at least the length of the source string srcStr.
 *
 * The destination string will always be Null-terminated, unless destSize is zero.
 *
 * If destStr and srcStr overlap the behaviour of this function is undefined.
 *
 * @note UTF-8 characters are supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t dataSample_JsonToString
(
    char* destStr,          ///< [IN] The destination where the srcStr is to be copied.
    const char* srcStr,     ///< [IN] The UTF-8 source string.
    const size_t destSize,  ///< [IN] Size of the destination buffer in bytes.
    size_t* numBytesPtr     ///< [OUT] The number of bytes copied not including the NULL-terminator.
                            ///        This parameter can be set to NULL if the number of bytes
                            ///        copied is not needed.
);

#endif // DATA_SAMPLE_H_INCLUDE_GUARD
