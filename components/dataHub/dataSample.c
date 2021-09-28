//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Data Sample class.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "interfaces.h"
#include "dataHub.h"
#include "dataSample.h"
#include "json.h"


typedef double Timestamp_t;


//--------------------------------------------------------------------------------------------------
/**
 * Data sample class. An object of this type can hold various different types of timestamped
 * data sample.
 */
//--------------------------------------------------------------------------------------------------
typedef struct DataSample
{
    Timestamp_t timestamp;      ///< The timestamp on the data sample.

    /// Union of different types of values. Which union member to use depends on the data type
    /// recorded in the resTree_Resource_t.  This is an optimization; Data Samples appear more
    /// frequently than Resources
    union
    {
        bool     boolean;
        double   numeric;
        char    *stringPtr;
    } value;
}
DataSample_t;

/// Size of largest allowed strings in samples.
#define STRING_LARGE_BYTES  HUB_MAX_STRING_BYTES
/// Size of medium sized strings in samples.
#define STRING_MED_BYTES    300
/// Size of small strings in samples.
#define STRING_SMALL_BYTES  50

/// Default non string sample pool size.  This may be overridden in the .cdef.
#define DEFAULT_NON_STRING_SAMPLE_POOL_SIZE 1000

/// Default string based sample pool size. This may be overridden in the .cdef.
#define DEFAULT_STRING_BASED_SAMPLE_POOL_SIZE 1000

/// Default number of large string pool entries.  This may be overridden in the .cdef.
#define DEFAULT_LARGE_STRING_POOL_SIZE 5

/// Number of medium string pool entries.
#define MED_STRING_POOL_SIZE                                                                \
    (((LE_MEM_BLOCKS(StringPool, DEFAULT_LARGE_STRING_POOL_SIZE) / 2) * STRING_LARGE_BYTES) \
        / STRING_MED_BYTES)

/// Number of small string pool entries.
#define SMALL_STRING_POOL_SIZE \
    (((MED_STRING_POOL_SIZE / 2) * STRING_MED_BYTES) / STRING_SMALL_BYTES)

/// Pool of simple Data Sample objects(trigger, boolean, numeric)
static le_mem_PoolRef_t NonStringDataSamplePool = NULL;
LE_MEM_DEFINE_STATIC_POOL(NonStringDataSamplePool, DEFAULT_NON_STRING_SAMPLE_POOL_SIZE,
                          sizeof(DataSample_t));

/// Pool of String based Data Sample objects(string and json).
static le_mem_PoolRef_t StringBasedDataSamplePool = NULL;
LE_MEM_DEFINE_STATIC_POOL(StringBasedDataSamplePool, DEFAULT_STRING_BASED_SAMPLE_POOL_SIZE,
                          sizeof(DataSample_t));

/// Pool for holding strings.
static le_mem_PoolRef_t StringPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(StringPool, DEFAULT_LARGE_STRING_POOL_SIZE, STRING_LARGE_BYTES);

//--------------------------------------------------------------------------------------------------
/**
 * Sample destructor
 * Used to release any strings that are allocated as part of sample creation.
 */
//--------------------------------------------------------------------------------------------------
static void StringSampleDestructor
(
    void* objPtr
)
{
    dataSample_Ref_t samplePtr = objPtr;
    const char* strValue = dataSample_GetString(samplePtr);
    if (strValue)
    {
        le_mem_Release((char*)strValue);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute the length of the escaped character
 *
 * @return The length of the escaped character in bytes
 *
 * @note UTF-8 characters are supported. Only the first character is required to compute the length.
 */
//--------------------------------------------------------------------------------------------------
static size_t ComputeEscapedCharLength
(
    char inputChar ///< [IN] The character we want to know the length when it is escaped
)
{
    if ((inputChar > 31) && (inputChar != '\"') && (inputChar != '\\')) {
        // The character doesn't need to be escaped, so the length
        // to return is the character length
        return le_utf8_NumBytesInChar(inputChar);
    }

    switch (inputChar)
    {
        case '\\':
        case '\"':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            return 2;
            break;

        default:
            /* Unicode codepoint */
            return 5;
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Escape a character
 *
 * This function transforms a UTF-8 character into its escaped version.
 * If the character doesn't need to be escaped then the character is copied as is.
 *
 * @warning escapedChar must point to a char array that has enough room to received the escaped
 *          character. To know the needed room prior to call this function, you can compute
 *          the escaped character length with ComputeEscapedCharLength().
 *
 * @note UTF-8 characters are supported.
 * @note This function manipules characters and not string, therefore no null-termination character
 *       is added at the end of the escaped character.
 *
 * @return The input character length in bytes (can be > 1 in UTF-8)
 *
 */
//--------------------------------------------------------------------------------------------------
static size_t EscapeCharacter
(
    const char* inputChar,  ///< [IN] The character we want to escape
    char* escapedChar       ///< [OUT] The escaped character
)
{
    if ((inputChar[0] > 31) && (inputChar[0] != '\"') && (inputChar[0] != '\\')) {
        // The input character don't need to be escaped
        size_t charLength = le_utf8_NumBytesInChar(inputChar[0]);
        strncpy (escapedChar, inputChar, charLength);
        return charLength;
    }

    // Write reverse solidus
    escapedChar[0] = '\\';

    switch (inputChar[0])
    {
        case '\\':
            escapedChar[1] = '\\';
            break;
        case '\"':
            escapedChar[1] = '\"';
            break;
        case '\b':
            escapedChar[1] = 'b';
            break;
        case '\f':
            escapedChar[1] = 'f';
            break;
        case '\n':
            escapedChar[1] = 'n';
            break;
        case '\r':
            escapedChar[1] = 'r';
            break;
        case '\t':
            escapedChar[1] = 't';
            break;
        default:
        {
            /* Unicode codepoint */
            int len = snprintf(&escapedChar[1], 6,"u%04x", inputChar[0]);
            LE_ASSERT(len == 5);
            break;
        }
    }

    return 1;
 }

//--------------------------------------------------------------------------------------------------
/**
 * PRELIMINARY NOTE :
 * ------------------
 * This function is similar to le_utf8_Copy. The main difference is that characters that need to
 * be escaped are escaped to produce a valid JSON file.
 *
 * A JSON string begins and ends with quotation marks.  All Unicode characters may be placed within
 * the quotation marks, except for the characters that must be escaped:
 * quotation mark, reverse solidus, and the control characters (U+0001 through U+001F).
 *
 * Source : https://tools.ietf.org/html/rfc7159#section-7
 *
 * This function copies the string in srcStr to the start of destStr and returns the number of bytes
 * copied (not including the NULL-terminator) in numBytesPtr.  Null can be passed into numBytesPtr
 * if the number of bytes copied is not needed.  The srcStr must be in UTF-8 format.
 *
 * If the size of srcStr is less than or equal to the destination buffer size then the entire srcStr
 * will be copied including the null-character.  The rest of the destination buffer is not modified.
 *
 * If the size of srcStr is larger than the destination buffer then the maximum number of characters
 * (from srcStr) plus a null-character that will fit in the destination buffer is copied.
 *
 * UTF-8 characters may be more than one byte long and this function will only copy whole characters
 * not partial characters.  Therefore, even if srcStr is larger than the destination buffer the
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
    char* destStr,          ///< [IN] The destination where the srcStr is to be copied.
    const char* srcStr,     ///< [IN] The UTF-8 source string.
    const size_t destSize,  ///< [IN] Size of the destination buffer in bytes.
    size_t* numBytesPtr     ///< [OUT] The number of bytes copied not including the NULL-terminator.
                            ///        This parameter can be set to NULL if the number of bytes
                            ///        copied is not needed.
)
{
    // Check parameters.
    LE_ASSERT( (destStr != NULL) && (srcStr != NULL) && (destSize > 0) );

    // Go through the string copying one character at a time.
    size_t i = 0;
    size_t j = 0;
    size_t charLength = 0;
    size_t escapedCharLength = 0;
    while (1)
    {
        if (srcStr[i] == '\0')
        {
            // NULL character found.  Complete the copy and return.
            destStr[j] = '\0';

            if (numBytesPtr)
            {
                *numBytesPtr = j;
            }

            return LE_OK;
        }
        else
        {
            // Normal character, copy.

            // First check if we have enough room to store the escaped version of the character.
            escapedCharLength = ComputeEscapedCharLength(srcStr[i]);

            if (escapedCharLength == 0)
            {
                // This is an error in the string format. Zero out the destStr and return.
                destStr[0] = '\0';

                if (numBytesPtr)
                {
                    *numBytesPtr = 0;
                }

                return LE_OK;
            }

            if (escapedCharLength + j >= destSize)
            {
                // It will not fit in the available space, so stop.
                destStr[j] = '\0';

                if (numBytesPtr)
                {
                    *numBytesPtr = j;
                }

                return LE_OVERFLOW;
            }

            // We have enough room, insert the escaped character.
            charLength = EscapeCharacter(&srcStr[i], &destStr[j]);
            i += charLength;
            j += escapedCharLength;
        }
    }
}

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
)
{
    size_t readIndex = 0;
    size_t writeIndex = 0;
    size_t pos;
    size_t srcLen = strlen(srcStr);

    // Check parameters.
    if ((destStr == NULL) || (srcStr == NULL) || (destSize < srcLen))
    {
        return LE_BAD_PARAMETER;
    }

    if ((srcStr[0] != '\"') && (srcStr[srcLen-1] != '\"'))
    {
        // This JSON does not begin nor end with double marks, just copy the string
        return le_utf8_Copy(destStr, srcStr, destSize, numBytesPtr);
    }

    // Now we handle JSON that are contained between double marks.
    // We have to remove those double marks and unescape the string.

    if (srcLen < 2)
    {
        LE_ERROR("Input JSON is invalid");
        return LE_FORMAT_ERROR;
    }

    // Skip the first '"'
    readIndex++;

    while (1)
    {
        // Get the position of the next reverse solidus, if exists
        pos = strcspn(&srcStr[readIndex], "\\");

        // Copy the string up to there
        strncpy(&destStr[writeIndex], &srcStr[readIndex], pos);
        writeIndex += pos;
        readIndex += pos;

        if (readIndex >= srcLen)
        {
            // There is no more characters to copy, stop now
            break;
        }

        // A reverse solidus has been found, remove it and copy the following character
        readIndex++;
        destStr[writeIndex] = srcStr[readIndex];
        readIndex++;
        writeIndex++;
    }

    // Remove the trailing '"' and return.
    writeIndex--;
    destStr[writeIndex] = '\0';

    if (numBytesPtr)
    {
        *numBytesPtr = writeIndex;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Data Sample module.
 */
//--------------------------------------------------------------------------------------------------
void dataSample_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    le_mem_PoolRef_t layeredStringPool;

    NonStringDataSamplePool = le_mem_InitStaticPool(NonStringDataSamplePool,
                                DEFAULT_NON_STRING_SAMPLE_POOL_SIZE, sizeof(DataSample_t));

    StringBasedDataSamplePool = le_mem_InitStaticPool(StringBasedDataSamplePool,
                                  DEFAULT_STRING_BASED_SAMPLE_POOL_SIZE, sizeof(DataSample_t));

    le_mem_SetDestructor(StringBasedDataSamplePool, StringSampleDestructor);

    layeredStringPool = le_mem_InitStaticPool(StringPool, DEFAULT_LARGE_STRING_POOL_SIZE,
                            STRING_LARGE_BYTES);
    layeredStringPool = le_mem_CreateReducedPool(layeredStringPool, "MedStringPool",
                            MED_STRING_POOL_SIZE, STRING_MED_BYTES);
    StringPool = le_mem_CreateReducedPool(layeredStringPool, "SmallStringPool",
                    SMALL_STRING_POOL_SIZE, STRING_SMALL_BYTES);
}


//--------------------------------------------------------------------------------------------------
/**
 * Creates a new Data Sample object and returns a pointer to it.
 *
 * @warning Don't forget to set the value if it's not a trigger type.
 *
 * @return Ptr to the new object or NULL if failed to allocate memory.
 */
//--------------------------------------------------------------------------------------------------
static inline DataSample_t* CreateSample
(
    le_mem_PoolRef_t pool,  ///< Pool to allocate the object from.
    Timestamp_t timestamp
)
//--------------------------------------------------------------------------------------------------
{
    DataSample_t* samplePtr = hub_MemAlloc(pool);

    if (samplePtr == NULL)
    {
        LE_ERROR("Failed to allocate space for a datasample");
        return NULL;
    }

    if (timestamp == IO_NOW)
    {
        le_clk_Time_t currentTime = le_clk_GetAbsoluteTime();
        timestamp = (((double)(currentTime.usec)) / 1000000) + currentTime.sec;
    }

    samplePtr->timestamp = timestamp;

    return samplePtr;
}


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
    Timestamp_t timestamp
)
//--------------------------------------------------------------------------------------------------
{
    DataSample_t* samplePtr = CreateSample(NonStringDataSamplePool, timestamp);

    return samplePtr;
}


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
    Timestamp_t timestamp,
    bool value
)
//--------------------------------------------------------------------------------------------------
{
    DataSample_t* samplePtr = CreateSample(NonStringDataSamplePool, timestamp);
    if (samplePtr)
    {
        samplePtr->value.boolean = value;
    }

    return samplePtr;
}


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
    Timestamp_t timestamp,
    double value
)
//--------------------------------------------------------------------------------------------------
{
    DataSample_t* samplePtr = CreateSample(NonStringDataSamplePool, timestamp);
    if (samplePtr)
    {
        samplePtr->value.numeric = value;
    }

    return samplePtr;
}


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
    Timestamp_t timestamp,
    const char* value
)
//--------------------------------------------------------------------------------------------------
{
    DataSample_t *samplePtr = CreateSample(StringBasedDataSamplePool, timestamp);
    if (samplePtr)
    {
        samplePtr->value.stringPtr = le_mem_StrDup(StringPool, value);
        if (samplePtr->value.stringPtr == NULL)
        {
            LE_ERROR("Could not allocate space for string of size %" PRIuS, le_utf8_NumBytes(value));
            le_mem_Release(samplePtr);
            samplePtr = NULL;
        }
    }
    return samplePtr;
}


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
)
//--------------------------------------------------------------------------------------------------
{
    // Since the data type is not actually stored in the data sample itself, and since the
    // JSON values are stored in the same way that strings are...
    return dataSample_CreateString(timestamp, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Read the timestamp on a Data Sample.
 *
 * @return The timestamp.
 */
//--------------------------------------------------------------------------------------------------
Timestamp_t dataSample_GetTimestamp
(
    dataSample_Ref_t sampleRef
)
//--------------------------------------------------------------------------------------------------
{
    return sampleRef->timestamp;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read a Boolean value from a Data Sample.
 *
 * @return The value.
 *
 * @warning You had better be sure that this is a Boolean Data Sample.
 */
//--------------------------------------------------------------------------------------------------
bool dataSample_GetBoolean
(
    dataSample_Ref_t sampleRef
)
//--------------------------------------------------------------------------------------------------
{
    return sampleRef->value.boolean;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read a numeric value from a Data Sample.
 *
 * @return The value.
 *
 * @warning You had better be sure that this is a Numeric Data Sample.
 */
//--------------------------------------------------------------------------------------------------
double dataSample_GetNumeric
(
    dataSample_Ref_t sampleRef
)
//--------------------------------------------------------------------------------------------------
{
    return sampleRef->value.numeric;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read a string value from a Data Sample.
 *
 * @return Ptr to the value. DO NOT use this after releasing your reference to the sample.
 *
 * @warning You had better be sure that this is a String Data Sample.
 */
//--------------------------------------------------------------------------------------------------
const char* dataSample_GetString
(
    dataSample_Ref_t sampleRef
)
//--------------------------------------------------------------------------------------------------
{
    return sampleRef->value.stringPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read a JSON value from a Data Sample.
 *
 * @return Ptr to the value. DO NOT use this after releasing your reference to the sample.
 *
 * @warning You had better be sure that this is a JSON Data Sample.
 */
//--------------------------------------------------------------------------------------------------
const char* dataSample_GetJson
(
    dataSample_Ref_t sampleRef
)
//--------------------------------------------------------------------------------------------------
{
    // The data type is not actually stored in the data sample itself, and
    // JSON values are stored in the same way that strings are.
    return sampleRef->value.stringPtr;
}


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
)
//--------------------------------------------------------------------------------------------------
{
    switch(dataType)
    {
        case IO_DATA_TYPE_TRIGGER:
        {
            if (valueBuffSize > 0)
            {
                valueBuffPtr[0] = '\0';
                return LE_OK;
            }
            return LE_OVERFLOW;
        }

        case IO_DATA_TYPE_BOOLEAN:
        {
            int i;

            if (sampleRef->value.boolean)
            {
                i = snprintf(valueBuffPtr, valueBuffSize, "true");
            }
            else
            {
                i = snprintf(valueBuffPtr, valueBuffSize, "false");
            }

            if (i >= (int)valueBuffSize)
            {
                return LE_OVERFLOW;
            }
            return LE_OK;
        }

        case IO_DATA_TYPE_NUMERIC:
        {
            if ((int)valueBuffSize <= snprintf(valueBuffPtr,
                                          valueBuffSize,
                                          "%lf",
                                          sampleRef->value.numeric))
            {
                return LE_OVERFLOW;
            }
            return LE_OK;
        }

        case IO_DATA_TYPE_STRING:
        {
            // Already in String format, just copy it into the buffer.
            return le_utf8_Copy(valueBuffPtr, sampleRef->value.stringPtr, valueBuffSize, NULL);
        }

        case IO_DATA_TYPE_JSON:
        {
            // We need to unescape the string
            return dataSample_JsonToString(valueBuffPtr, sampleRef->value.stringPtr, valueBuffSize, NULL);
        }
    }

    LE_ERROR("Invalid data type %d.", dataType);
    return LE_BAD_PARAMETER;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read any type of value from a Data Sample, in JSON format.
 *
 * @return
 *  - LE_OK if successful,
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t dataSample_ConvertToJson
(
    dataSample_Ref_t sampleRef,
    io_DataType_t dataType, ///< [IN] The data type of the data sample.
    char* valueBuffPtr,     ///< [OUT] Ptr to buffer where value will be stored.
    size_t valueBuffSize    ///< [IN] Size of value buffer, in bytes.
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t result;
    size_t len;

    switch (dataType)
    {
        case IO_DATA_TYPE_TRIGGER:
            return le_utf8_Copy(valueBuffPtr, "null", valueBuffSize, NULL);

        case IO_DATA_TYPE_BOOLEAN:
        {
            int i;

            if (sampleRef->value.boolean)
            {
                i = snprintf(valueBuffPtr, valueBuffSize, "true");
            }
            else
            {
                i = snprintf(valueBuffPtr, valueBuffSize, "false");
            }

            if (i >= (int) valueBuffSize)
            {
                return LE_OVERFLOW;
            }
            return LE_OK;
        }

        case IO_DATA_TYPE_NUMERIC:

            if ((int) valueBuffSize <= snprintf(valueBuffPtr, valueBuffSize, "%lf",
                                                sampleRef->value.numeric))
            {
                return LE_OVERFLOW;
            }
            return LE_OK;

        case IO_DATA_TYPE_STRING:

            // Must wrap the string value in quotes.
            // We need at least 3 bytes for the two quotes and a null terminator.
            if (valueBuffSize < 3)
            {
                return LE_OVERFLOW;
            }
            valueBuffPtr[0] = '"';
            valueBuffPtr++;
            valueBuffSize--;
            result = dataSample_StringToJson(valueBuffPtr, sampleRef->value.stringPtr, valueBuffSize, &len);
            if ((result != LE_OK) || (len >= (valueBuffSize - 1)))  // need 1 more for the last '"'
            {
                return LE_OVERFLOW;
            }
            valueBuffPtr[len] = '"'; // replace null-terminator with '"'
            valueBuffPtr[len + 1] = '\0'; // null-terminate the string.
            return LE_OK;

        case IO_DATA_TYPE_JSON:

            // Already in JSON format, just copy it into the buffer.
            return le_utf8_Copy(valueBuffPtr, sampleRef->value.stringPtr, valueBuffSize, NULL);
    }

    LE_ERROR("Invalid data type %d.", dataType);
    return LE_BAD_PARAMETER;
}


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
)
//--------------------------------------------------------------------------------------------------
{
    char resultBuff[HUB_MAX_STRING_BYTES];
    json_DataType_t jsonType;

    le_result_t result = json_Extract(resultBuff,
                                      sizeof(resultBuff),
                                      dataSample_GetJson(sampleRef),
                                      extractionSpec,
                                      &jsonType);
    if (result != LE_OK)
    {
        LE_WARN("Failed to extract '%s' from JSON '%s'.",
                extractionSpec,
                dataSample_GetJson(sampleRef));
        return NULL;
    }
    else
    {
        switch (jsonType)
        {
            case JSON_TYPE_NULL:

                *dataTypePtr = IO_DATA_TYPE_TRIGGER;
                return dataSample_CreateTrigger(dataSample_GetTimestamp(sampleRef));

            case JSON_TYPE_BOOLEAN:

                *dataTypePtr = IO_DATA_TYPE_BOOLEAN;
                return dataSample_CreateBoolean(dataSample_GetTimestamp(sampleRef),
                                                json_ConvertToBoolean(resultBuff));

            case JSON_TYPE_NUMBER:

                *dataTypePtr = IO_DATA_TYPE_NUMERIC;
                return dataSample_CreateNumeric(dataSample_GetTimestamp(sampleRef),
                                                json_ConvertToNumber(resultBuff));

            case JSON_TYPE_STRING:

                *dataTypePtr = IO_DATA_TYPE_STRING;
                return dataSample_CreateString(dataSample_GetTimestamp(sampleRef),
                                               resultBuff);

            case JSON_TYPE_OBJECT:
            case JSON_TYPE_ARRAY:

                *dataTypePtr = IO_DATA_TYPE_JSON;
                return dataSample_CreateJson(dataSample_GetTimestamp(sampleRef),
                                             resultBuff);
        }

        LE_FATAL("Unexpected JSON type %d.", jsonType);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the timestamp of a Data Sample.
 */
//--------------------------------------------------------------------------------------------------
void dataSample_SetTimestamp
(
    dataSample_Ref_t sample,
    double timestamp
)
//--------------------------------------------------------------------------------------------------
{
    sample->timestamp = timestamp;
}
