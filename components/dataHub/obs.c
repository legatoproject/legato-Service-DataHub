//--------------------------------------------------------------------------------------------------
/**
 * @file obs.c
 *
 * Implementation of Observations.
 *
 * Data sample buffer backup files are kept under BACKUP_DIR.  Their file system paths relative
 * to BACKUP_DIR are the same as their resource paths relative to the /obs/ namespace in the
 * resource tree.
 *
 * The data sample buffer backup file format looks like this (little-endian byte order):
 *
 * - file format version byte = 0
 * - data type byte containing one of the following ASCII characters:
 *       t = trigger
 *       b = Boolean
 *       n = numeric
 *       s = string
 *       j = JSON
 * - number of records = 4-byte unsigned integer
 * - array of records, sorted oldest-first, each containing:
 *       - timestamp (8-byte IEEE double-precision floating point value)
 *       - value, depending on data type, as follows:
 *             t -> no value
 *             b -> 1 byte, 0 = false, 1 = true
 *             n -> 8-byte IEEE double-precision floating point value
 *             s -> 4-byte unsigned integer length, followed by string content (no null-terminator)
 *             j -> 4-byte unsigned integer length, followed by JSON string content (no term char)
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "dataHub.h"
#include "nan.h"
#include "dataSample.h"
#include "resource.h"
#include "resTree.h"
#include "json.h"
#include "obs.h"
#include "configService.h"

#if LE_CONFIG_LINUX
#   include <ftw.h>
#endif

#ifdef LEGATO_EMBEDDED
 #define BACKUP_DIR "/home/root/dataHubBackup/"
#else
 #define BACKUP_DIR "backup/"
#endif
#define BACKUP_DIR_PATH_LEN (sizeof(BACKUP_DIR) - 1)
#define BACKUP_SUFFIX ".bak"
#define BACKUP_SUFFIX_LEN (sizeof(BACKUP_SUFFIX) - 1)

#define MAX_BACKUP_FILE_PATH_BYTES (  BACKUP_DIR_PATH_LEN \
                                    + IO_MAX_RESOURCE_PATH_LEN \
                                    + BACKUP_SUFFIX_LEN \
                                    + 1 /* for null terminator */ )

/// Number of seconds in 30 years.
#define THIRTY_YEARS 946684800.0

/// Default number of observations.  This can be overridden in the .cdef.
#define DEFAULT_OBSERVATION_POOL_SIZE       5
/// Default number of buffer entries.  This can be overridden in the .cdef.
#define DEFAULT_BUFFER_ENTRY_POOL_SIZE      5
/// Default number of read operations.  This can be overridden in the .cdef.
#define DEFAULT_READ_OPERATION_POOL_SIZE    2

/// Observation Resource.  Allocated from the Observation Pool.
typedef struct
{
    res_Resource_t resource;    ///< The base class (MUST BE FIRST).

    double highLimit; ///< Filter deadband/liveband high limit; NAN = disabled.
    double lowLimit;  ///< Filter deadband/liveband low limit; NAN = disabled.
    double changeBy;  ///< Drop values that differ by less than this from current; NAN/0 = disabled.

    double minPeriod; ///< Min number of seconds before accepting another value; NAN/0 = disabled.
    uint32_t lastPushTime; ///< Time at which last push was accepted (ms, relative clock).

    obs_TransformType_t transformType; ///< Buffer transform type

    size_t maxCount;  ///< Maximum number of entries to buffer.
    size_t count;     ///< Current number of entries in the buffer.

    io_DataType_t bufferedType; ///< Data type of samples currently in the buffer.

    uint32_t backupPeriod; ///< Min time (in seconds) between non-volatile backups of the buffer.
    uint32_t lastBackupTime; ///< Time at which last push was accepted (seconds, relative clock).
    le_timer_Ref_t backupTimer; ///< Reference to the timer used to trigger the next backup.

    le_sls_List_t sampleList; ///< Queue of buffered data samples (oldest first, newest last).

    le_dls_List_t readOpList; ///< List of ongoing Read Operations on the buffered samples.

    char jsonExtraction[ADMIN_MAX_JSON_EXTRACTOR_LEN + 1]; ///< JSON extraction specifier (or "").
    char destination[CONFIG_MAX_DESTINATION_NAME_BYTES];   ///< Destination name string
}
Observation_t;



/// Object used to link a Data Sample into an Observation's buffer.
/// Holds a reference on the Data Sample object.
typedef struct
{
    le_sls_Link_t link;  ///< Used to link into a Observation's sampleList.
    dataSample_Ref_t sampleRef; ///< Reference to the Data Sample object.
}
BufferEntry_t;


/// Each data sample in a read operation looks like the following:
/// {"t":1537483647.125371,"v":true}
/// The largest value is IO_MAX_STRING_VALUE_LEN bytes long.
/// The timestamp is a double-precision floating point number. Doubles can be
/// hundreds of bytes long if the maximum precision is used in non-scientific notation,
/// but in this case they typically won't be more than 6 decimal places.
#define READ_OP_BUFF_BYTES (HUB_MAX_STRING_BYTES + 48)


//--------------------------------------------------------------------------------------------------
/**
 * Record used for keeping track of buffer read operations.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link; ///< Used to link into the Observation's list of ongoing read operations.
    Observation_t* obsPtr;  ///< Ptr to Observation whose buffer is being read.
    le_fdMonitor_Ref_t fdMonitor; ///< Used to get notification when the FD is clear to write.
    int fd; ///< fd to write to.
    BufferEntry_t* nextEntryPtr; ///< Buff entry to load into write buff next (ref counted).
    enum { START, SAMPLE, COMMA, END } state; ///< What are we supposed to write next?
    char writeBuffer[READ_OP_BUFF_BYTES];  ///< Buffer currently being written.
    size_t writeLen; ///< Number of characters (excl. null terminator) in the writeBuffer.
    size_t writeOffset;   ///< Offset into the writeBuffer to write from next.
    query_ReadCompletionFunc_t handlerPtr; ///< Completion callback.
    void* contextPtr;   ///< Value to be passed to completion callback.
}
ReadOperation_t;


/// Pool of Observation objects.
static le_mem_PoolRef_t ObservationPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(ObservationPool, DEFAULT_OBSERVATION_POOL_SIZE, sizeof(Observation_t));

/// Pool of Buffer Entry objects.
static le_mem_PoolRef_t BufferEntryPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(BufferEntryPool, DEFAULT_BUFFER_ENTRY_POOL_SIZE, sizeof(BufferEntry_t));

/// Pool to allocate ReadOperation_t object from.
static le_mem_PoolRef_t ReadOperationPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(ReadOperationPool,
                          DEFAULT_READ_OPERATION_POOL_SIZE,
                          sizeof(ReadOperation_t));


//--------------------------------------------------------------------------------------------------
/**
 * Get the relative time in milliseconds.
 *
 * @return the relative time (ms).
 */
//--------------------------------------------------------------------------------------------------
static uint32_t GetRelativeTimeMs
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    le_clk_Time_t structuredTime = le_clk_GetRelativeTime();

    return (structuredTime.sec * 1000 + structuredTime.usec / 1000);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the file system path to use for the backup file for a given Observation's data sample buffer.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetBackupFilePath
(
    char* pathBuffPtr,  ///< [OUT] Ptr to where the path will be written.
    size_t pathBuffSize,    ///< Size of the buffer in bytes.
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    size_t len;
    LE_ASSERT(LE_OK == le_utf8_Copy(pathBuffPtr, BACKUP_DIR, pathBuffSize, &len));

    pathBuffPtr += len;
    pathBuffSize -= len;

    resTree_EntryRef_t obsNamespace = resTree_FindEntry(resTree_GetRoot(), "obs");
    LE_ASSERT(obsNamespace != NULL);

    ssize_t result = resTree_GetPath(pathBuffPtr,
                                     pathBuffSize,
                                     obsNamespace,
                                     res_GetResTreeEntry(&obsPtr->resource));
    if (result <= 0)
    {
        LE_CRIT("Failed to fetch Observation path for '%s' (%s).",
                resTree_GetEntryName(res_GetResTreeEntry(&obsPtr->resource)),
                LE_RESULT_TXT(result));

        return result;
    }

    pathBuffPtr += result;
    pathBuffSize -= result;

    return le_utf8_Copy(pathBuffPtr, BACKUP_SUFFIX, pathBuffSize, NULL);
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete the observation's buffer backup file, if it exists.
 */
//--------------------------------------------------------------------------------------------------
static void DeleteBackup
(
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    char path[HUB_MAX_RESOURCE_PATH_BYTES];
    le_result_t result = GetBackupFilePath(path, sizeof(path), obsPtr);
    if (result == LE_OK)
    {
        unlink(path);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Buffer Entry destructor.
 */
//--------------------------------------------------------------------------------------------------
static void BufferEntryDestructor
(
    void* objectPtr
)
//--------------------------------------------------------------------------------------------------
{
    BufferEntry_t* buffEntryPtr = objectPtr;

    le_mem_Release(buffEntryPtr->sampleRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Terminate a read operation.
 */
//--------------------------------------------------------------------------------------------------
static void EndRead
(
    ReadOperation_t* opPtr,
    le_result_t result
)
//--------------------------------------------------------------------------------------------------
{
    if (opPtr->nextEntryPtr != NULL)
    {
        le_mem_Release(opPtr->nextEntryPtr);
        opPtr->nextEntryPtr = NULL;
    }

    le_fdMonitor_Delete(opPtr->fdMonitor);

    close(opPtr->fd);

    opPtr->handlerPtr(result, opPtr->contextPtr);

    le_dls_Remove(&opPtr->obsPtr->readOpList, &opPtr->link);

    le_mem_Release(opPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Observation destructor.
 */
//--------------------------------------------------------------------------------------------------
static void ObservationDestructor
(
    void* objectPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = objectPtr;

    // Delete all the buffered data samples.
    le_sls_Link_t* linkPtr;
    while (NULL != (linkPtr = le_sls_Pop(&obsPtr->sampleList)))
    {
        BufferEntry_t* buffEntryPtr = CONTAINER_OF(linkPtr, BufferEntry_t, link);

        le_mem_Release(buffEntryPtr);
    }

    obsPtr->count = 0;
    obsPtr->maxCount = 0;

    // If the observation had backups enabled, delete the backup file.
    if (obsPtr->backupPeriod > 0)
    {
        DeleteBackup(obsPtr);
    }

    // If there are read operations in progress, end them.
    while (le_dls_IsEmpty(&obsPtr->readOpList) == false)
    {
        EndRead(CONTAINER_OF(le_dls_Peek(&obsPtr->readOpList), ReadOperation_t, link),
                LE_COMM_ERROR);
    }

    res_Destruct(&obsPtr->resource);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the first (oldest) buffer entry in an Observation's data sample buffer.
 *
 * @return Pointer to the buffer entry or NULL if the buffer is empty.
 */
//--------------------------------------------------------------------------------------------------
static BufferEntry_t* GetOldestBufferEntry
(
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    le_sls_Link_t* linkPtr = le_sls_Peek(&obsPtr->sampleList);

    if (linkPtr != NULL)
    {
        return CONTAINER_OF(linkPtr, BufferEntry_t, link);
    }
    else
    {
        return NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the next (newer) buffer entry in an Observation's data sample buffer.
 *
 * @return A pointer to the buffer entry, or NULL if there are no newer samples in the buffer.
 */
//--------------------------------------------------------------------------------------------------
static BufferEntry_t* GetNextBufferEntry
(
    Observation_t* obsPtr,
    BufferEntry_t* buffEntryPtr
)
//--------------------------------------------------------------------------------------------------
{
    le_sls_Link_t* linkPtr = le_sls_PeekNext(&obsPtr->sampleList, &buffEntryPtr->link);

    if (linkPtr != NULL)
    {
        return CONTAINER_OF(linkPtr, BufferEntry_t, link);
    }
    else
    {
        return NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Load the write buffer with a JSON representation of the next sample to be read.
 *
 * @return true if successful, false if there are no more samples.
 */
//--------------------------------------------------------------------------------------------------
static bool LoadReadOpBuffer
(
    ReadOperation_t* opPtr
)
//--------------------------------------------------------------------------------------------------
{
    opPtr->writeLen = 0;
    opPtr->writeOffset = 0;
    opPtr->writeBuffer[0] = '\0';

    do
    {
        if (opPtr->nextEntryPtr == NULL)
        {
            return false;
        }

        // If the buffer entry's ref count should be 2.  If it is only 1, then we know it has
        // fallen off the end of the observation's buffer, which means that all entries in the
        // observation's buffer are now newer than this one.
        if (le_mem_GetRefCount(opPtr->nextEntryPtr) == 1)
        {
            le_mem_Release(opPtr->nextEntryPtr);
            opPtr->nextEntryPtr = GetOldestBufferEntry(opPtr->obsPtr);
            if (opPtr->nextEntryPtr != NULL)
            {
                le_mem_AddRef(opPtr->nextEntryPtr);
            }
            else
            {
                return false;
            }
        }

        int len = snprintf(opPtr->writeBuffer,
                           sizeof(opPtr->writeBuffer),
                           "{\"t\":%lf,\"v\":",
                           dataSample_GetTimestamp(opPtr->nextEntryPtr->sampleRef));
        if (len >= (int) sizeof(opPtr->writeBuffer))
        {
            LE_CRIT("Buffer overflow. Skipping entry.");
            // Leave the writeLen 0 so we'll loop around and try the next sample.
        }
        else
        {
            // Copy the JSON version of the contents of the current buffer entry's data into
            // the write buffer, if there's space (leaving room for an additional '}' at the end).
            le_result_t result = dataSample_ConvertToJson(opPtr->nextEntryPtr->sampleRef,
                                                          res_GetDataType(&(opPtr->obsPtr->resource)),
                                                          opPtr->writeBuffer + len,
                                                          sizeof(opPtr->writeBuffer) - len - 1);
            if (result != LE_OK)
            {
                LE_ERROR("JSON value doesn't fit in write buffer. Skipping.");
                // Leave the writeLen 0 so we'll loop around and try the next sample.
            }
            else
            {
                len += strlen(opPtr->writeBuffer + len);

                opPtr->writeBuffer[len] = '}';
                opPtr->writeBuffer[len + 1] = '\0';

                opPtr->writeLen = len + 1;
            }
        }

        // Advance the nextEntryPtr to the next entry in the Observation's data sample list.
        BufferEntry_t* nextEntryPtr = GetNextBufferEntry(opPtr->obsPtr, opPtr->nextEntryPtr);
        le_mem_Release(opPtr->nextEntryPtr);

        if (nextEntryPtr != NULL)
        {
            opPtr->nextEntryPtr = nextEntryPtr;
            le_mem_AddRef(nextEntryPtr);
        }
        else
        {
            opPtr->nextEntryPtr = NULL;
        }

    } while (opPtr->writeLen == 0); // Loop if the write buffer is still empty.

    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Write to an unbuffered file descriptor.
 *
 * @return The number of bytes written.  -1 on error (errno is set).
 */
//--------------------------------------------------------------------------------------------------
static ssize_t WriteToFd
(
    int fd,
    const void* buffPtr,
    size_t byteCount
)
//--------------------------------------------------------------------------------------------------
{
    ssize_t result;

    LE_ASSERT(buffPtr != NULL);

    do
    {
        result = write(fd, buffPtr, byteCount);

    } while ((result == -1) && (errno == EINTR));

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Continue a read operation.
 */
//--------------------------------------------------------------------------------------------------
static void ContinueReadOp
(
    ReadOperation_t* opPtr
)
//--------------------------------------------------------------------------------------------------
{
    ssize_t result;
    for (;;)
    {
        const char* writeBuffPtr = NULL;
        size_t writeLen = 0;

        // Figure out what to write based on the state.
        switch (opPtr->state)
        {
            case START:

                writeBuffPtr = "[";
                writeLen = 1;

                break;

            case SAMPLE:

                writeBuffPtr = opPtr->writeBuffer + opPtr->writeOffset;
                writeLen = opPtr->writeLen - opPtr->writeOffset;

                break;

            case COMMA:

                writeBuffPtr = ",";
                writeLen = 1;

                break;

            case END:

                writeBuffPtr = "]";
                writeLen = 1;

                break;
        }

        // Write and check for errors.
        result = WriteToFd(opPtr->fd, writeBuffPtr, writeLen);
        if (result == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                // Return and wait for this function to be called again by the FD Monitor.
                return;
            }

            LE_ERROR("Error writing (%m).");
            EndRead(opPtr, LE_COMM_ERROR);

            return;
        }

        // Advance to the next state.
        switch (opPtr->state)
        {
            case START:

                // If there's a sample in the write buffer,
                if (opPtr->writeLen > 0)
                {
                    opPtr->state = SAMPLE;
                }
                else
                {
                    opPtr->state = END;
                }
                break;

            case SAMPLE:

                // Update the write offset.
                opPtr->writeOffset += result;

                // If the write buffer has been written entirely,
                if (opPtr->writeOffset == opPtr->writeLen)
                {
                    // Try to load the next sample into the write buffer.
                    if (LoadReadOpBuffer(opPtr))
                    {
                        opPtr->state = COMMA;
                    }
                    else
                    {
                        opPtr->state = END;
                    }
                }
                // Note: If the write buffer has not been written entirely, stay in the SAMPLE
                // state and loop back around to write more.
                break;

            case COMMA:

                opPtr->state = SAMPLE;
                break;

            case END:

                EndRead(opPtr, LE_OK);

                return;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Event handler call-back for events on a read operation's write file descriptor.
 */
//--------------------------------------------------------------------------------------------------
static void ReadOpFdEventHandler
(
    int fd,
    short events
)
//--------------------------------------------------------------------------------------------------
{
    LE_UNUSED(fd);

    ReadOperation_t* opPtr = le_fdMonitor_GetContextPtr();

    // Check for error or hang-up.
    if ((events & POLLERR) || (events & POLLHUP) || (events & POLLRDHUP))
    {
        LE_ERROR("Error or hang-up on output stream.");
        EndRead(opPtr, LE_COMM_ERROR);
    }
    // Note: The only other reason for this function to be called is POLLOUT (writeable).
    else
    {
        ContinueReadOp(opPtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Start a read operation on a given Observation's buffer.
 */
//--------------------------------------------------------------------------------------------------
static void StartRead
(
    Observation_t* obsPtr,
    BufferEntry_t* startPtr, ///< Ptr to buffer entry to start at, or NULL if read data set empty.
    int outputFile, ///< File descriptor to write the data to.
    query_ReadCompletionFunc_t handlerPtr, ///< Completion callback.
    void* contextPtr    ///< Value to be passed to completion callback.
)
//--------------------------------------------------------------------------------------------------
{
    // Set the fd non-blocking
    if (0 != fcntl(outputFile, F_SETFL, O_NONBLOCK))
    {
        LE_ERROR("Failed to activate non-blocking mode (%m).");
        handlerPtr(LE_COMM_ERROR, contextPtr);
        return;
    }

    ReadOperation_t* opPtr = hub_MemAlloc(ReadOperationPool);
    if (opPtr == NULL)
    {
        LE_ERROR("Failed to allocate a read operation");
        handlerPtr(LE_NO_MEMORY, contextPtr);
        return;
    }
    opPtr->link = LE_DLS_LINK_INIT;
    le_dls_Queue(&obsPtr->readOpList, &opPtr->link);

    opPtr->obsPtr = obsPtr;

    opPtr->fdMonitor = le_fdMonitor_Create("Read", outputFile, ReadOpFdEventHandler, POLLOUT);
    le_fdMonitor_SetContextPtr(opPtr->fdMonitor, opPtr);
    opPtr->fd = outputFile;
    opPtr->nextEntryPtr = startPtr;
    // We hold a ref count on the buffer entry to prevent it from being released.
    if (startPtr != NULL)
    {
        le_mem_AddRef(startPtr);
    }
    opPtr->handlerPtr = handlerPtr;
    opPtr->contextPtr = contextPtr;

    opPtr->state = START;
    (void)LoadReadOpBuffer(opPtr);

    ContinueReadOp(opPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Adds a given data sample to the buffer of a given Observation.
 *
 * @return
 *      - LE_OK If datasample was added to observation buffer successfully.
 *      - LE_NO_MEMORY If failed to allocate memory for the new datasample in Buffer Pool.
 *      - LE_BAD_PARAMETER If dropped sample because its timestamp was older than the sample already
 *           in buffer.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AddToBuffer
(
    Observation_t* obsPtr,
    dataSample_Ref_t sampleRef
)
//--------------------------------------------------------------------------------------------------
{
    BufferEntry_t* buffEntryPtr;

    // If the new sample is timestamped older than the newest sample already in the buffer,
    // then we have a serious problem, because buffer traversal operations could get stuck in loops.
    le_sls_Link_t* linkPtr = le_sls_PeekTail(&obsPtr->sampleList);
    if (linkPtr != NULL)
    {
        buffEntryPtr = CONTAINER_OF(linkPtr, BufferEntry_t, link);

        double oldEntryTimestamp = dataSample_GetTimestamp(buffEntryPtr->sampleRef);
        double newEntryTimestamp = dataSample_GetTimestamp(sampleRef);

        if (oldEntryTimestamp > newEntryTimestamp)
        {
            LE_ERROR("New sample has older timestamp than (older) sample already in the buffer!");
            LE_ERROR("Dropping new sample timestamped %lf (< %lf in buffer)!",
                     newEntryTimestamp,
                     oldEntryTimestamp);
            return LE_BAD_PARAMETER;
        }
    }

    buffEntryPtr = hub_MemAlloc(BufferEntryPool);
    if (buffEntryPtr)
    {
        le_mem_AddRef(sampleRef);
        buffEntryPtr->sampleRef = sampleRef;
        buffEntryPtr->link = LE_SLS_LINK_INIT;
        le_sls_Queue(&obsPtr->sampleList, &buffEntryPtr->link);

        (obsPtr->count)++;
        return LE_OK;
    }
    else
    {
        LE_ERROR("Failed to allocate a buffer entry");
        return LE_NO_MEMORY;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * If the number of entries in a given Observation's buffer is larger than the number given,
 * discard enough of the oldest entries to correct that condition.
 */
//--------------------------------------------------------------------------------------------------
static void TruncateBuffer
(
    Observation_t* obsPtr,
    size_t count
)
//--------------------------------------------------------------------------------------------------
{
    while (obsPtr->count > count)
    {
        le_sls_Link_t* linkPtr = le_sls_Pop(&obsPtr->sampleList);
        LE_ASSERT(linkPtr != NULL);

        le_mem_Release(CONTAINER_OF(linkPtr, BufferEntry_t, link));

        (obsPtr->count)--;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Update the value of a data sample by replacing it, if necessary
 *
 * Note: Even though this function uses dataSample_CreateBoolean and dataSample-CreateNumberic which
 * may return NULL if dataSample pool is exhusted, because the previous sample is always released
 * first before calling the createSample function, it is expected that creating sample is always
 * successful.
 *
 * @return
 *      Pointer to the updated sample.
 */
//--------------------------------------------------------------------------------------------------
static dataSample_Ref_t UpdateSample
(
    dataSample_Ref_t sampleRef,
    io_DataType_t dataType,
    void *valuePtr
)
//--------------------------------------------------------------------------------------------------
{
    dataSample_Ref_t sample = sampleRef;
    double timestamp = dataSample_GetTimestamp(sampleRef);

    switch (dataType)
    {
        case IO_DATA_TYPE_BOOLEAN:
        {
            bool value = *((double *)valuePtr) > 0.0 ? true : false;
            if (dataSample_GetBoolean(sampleRef) != value)
            {
                le_mem_Release(sampleRef);
                sample = dataSample_CreateBoolean(timestamp, value);
            }
            break;
        }

        case IO_DATA_TYPE_NUMERIC:
        {
            double value = *((double *)valuePtr);
            if (dataSample_GetNumeric(sampleRef) != value)
            {
                le_mem_Release(sampleRef);
                sample = dataSample_CreateNumeric(timestamp, value);
            }
        }

        default:
            break;
    }

    if (!sample)
    {
        LE_FATAL("UpdateSample Failed to allocate a new sample");
    }
    return sample;
}


#if LE_CONFIG_FILESYSTEM
//--------------------------------------------------------------------------------------------------
/**
 * Writes a buffer load of data to a buffered file stream.
 *
 * On error, logs an error message and closes the file.
 *
 * @return true if successful, false if failed.
 */
//--------------------------------------------------------------------------------------------------
static bool WriteToStream
(
    FILE* file,
    const void* buffPtr,
    size_t buffSize
)
//--------------------------------------------------------------------------------------------------
{
    size_t recordsWritten = fwrite(buffPtr, buffSize, 1, file);
    if (recordsWritten != 1)
    {
        LE_CRIT("Failed to write (%m).");
        le_atomFile_CancelStream(file);
        return false;
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Reads a buffer load of data from a backup file.
 *
 * On error, logs an error message and closes the file.
 *
 * @return LE_OK if successful, LE_UNDERFLOW if the end of file was reached, LE_FAULT if failed.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ReadFromFile
(
    void* buffPtr,
    size_t buffSize,
    FILE* file
)
//--------------------------------------------------------------------------------------------------
{
    size_t bytesRead = fread(buffPtr, 1, buffSize, file);

    le_result_t result = LE_OK;

    if (bytesRead < buffSize)
    {
        if (feof(file))
        {
            result = LE_UNDERFLOW;
        }
        else
        {
            LE_CRIT("Failed to read (%m).");
            result = LE_FAULT;
        }

        le_atomFile_CancelStream(file);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the data type code byte to be written into a backup file.
 *
 * @return the data type code byte.
 */
//--------------------------------------------------------------------------------------------------
static uint8_t GetDataTypeCode
(
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    switch (res_GetDataType(&obsPtr->resource))
    {
        case IO_DATA_TYPE_TRIGGER:  return 't';
        case IO_DATA_TYPE_BOOLEAN:  return 'b';
        case IO_DATA_TYPE_NUMERIC:  return 'n';
        case IO_DATA_TYPE_STRING:   return 's';
        case IO_DATA_TYPE_JSON:     return 'j';
    }

    LE_FATAL("Invalid data type %d.", res_GetDataType(&obsPtr->resource));
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the data type represented by the code byte read from a backup file.
 *
 * @return true if successful, false on error.
 */
//--------------------------------------------------------------------------------------------------
static bool GetDataTypeFromCode
(
    io_DataType_t* dataTypePtr, ///< [OUT] Ptr to where the result will be put on success.
    uint8_t code
)
//--------------------------------------------------------------------------------------------------
{
    switch (code)
    {
        case 't': *dataTypePtr = IO_DATA_TYPE_TRIGGER; return true;
        case 'b': *dataTypePtr = IO_DATA_TYPE_BOOLEAN; return true;
        case 'n': *dataTypePtr = IO_DATA_TYPE_NUMERIC; return true;
        case 's': *dataTypePtr = IO_DATA_TYPE_STRING;  return true;
        case 'j': *dataTypePtr = IO_DATA_TYPE_JSON;    return true;
    }

    LE_CRIT("Invalid data type code %d.", (int)code);

    return false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Writes all the data samples for a given Observation to a given backup file.
 *
 * On error, logs an error message and closes the file.
 *
 * @return true if successful, false if failed.
 */
//--------------------------------------------------------------------------------------------------
static bool WriteSamplesToFile
(
    FILE* file,
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    io_DataType_t dataType = obsPtr->bufferedType;

    BufferEntry_t* buffEntryPtr = GetOldestBufferEntry(obsPtr);

    while (buffEntryPtr != NULL)
    {
        // Write the timestamp.
        double timestamp = dataSample_GetTimestamp(buffEntryPtr->sampleRef);
        if (!WriteToStream(file, &timestamp, sizeof(timestamp)))
        {
            return false;
        }

        switch (dataType)
        {
            case IO_DATA_TYPE_TRIGGER:

                // No Value.
                break;

            case IO_DATA_TYPE_BOOLEAN:
            {
                bool value = dataSample_GetBoolean(buffEntryPtr->sampleRef);
                if (!WriteToStream(file, &value, sizeof(value)))
                {
                    return false;
                }
                break;
            }
            case IO_DATA_TYPE_NUMERIC:
            {
                double value = dataSample_GetNumeric(buffEntryPtr->sampleRef);
                if (!WriteToStream(file, &value, sizeof(value)))
                {
                    return false;
                }
                break;
            }
            case IO_DATA_TYPE_STRING:
            {
                const char* valuePtr = dataSample_GetString(buffEntryPtr->sampleRef);
                uint32_t stringLen = strlen(valuePtr);
                if (!WriteToStream(file, &stringLen, 4))
                {
                    return false;
                }
                if (!WriteToStream(file, valuePtr, stringLen))
                {
                    return false;
                }
                break;
            }
            case IO_DATA_TYPE_JSON:
            {
                const char* valuePtr = dataSample_GetJson(buffEntryPtr->sampleRef);
                uint32_t stringLen = strlen(valuePtr);
                if (!WriteToStream(file, &stringLen, 4))
                {
                    return false;
                }
                if (!WriteToStream(file, valuePtr, stringLen))
                {
                    return false;
                }
                break;
            }
        }

        buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);
    }

    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Reads all the data samples from a given backup file and adds them to a given Observation's
 * data sample buffer.  Closes the file when done.
 */
//--------------------------------------------------------------------------------------------------
static void ReadSamplesFromFile
(
    Observation_t* obsPtr,
    FILE* file,
    size_t count    ///< The expected number of samples to read.
)
//--------------------------------------------------------------------------------------------------
{
    if (count == 0)
    {
        return;
    }

    io_DataType_t dataType = obsPtr->bufferedType;

    dataSample_Ref_t dataSample = NULL;

    for (;;)
    {
        // Read the timestamp.
        double timestamp;
        le_result_t result = ReadFromFile(&timestamp, sizeof(timestamp), file);
        if (result != LE_OK)
        {
            if (result == LE_UNDERFLOW)
            {
                // End of file reached, check that we've received all we should.
                if (count == 0)
                {
                    // The file contained exactly the number of samples we expected.
                    // The last data sample read from the file (which is the newest)
                    // should be pushed to the Observation so it becomes the current value.
                    res_Push(&obsPtr->resource, dataType, "", dataSample);
                    return;
                }
                else
                {
                    LE_CRIT("Backup file was truncated. Expected %zu more samples.", count);
                }
            }

            goto error;
        }

        // We succeeded in reading another timestamp, so we had better be expecting more samples.
        if (count == 0)
        {
            LE_CRIT("Backup file contains extra samples.");
            le_atomFile_CancelStream(file);
            goto error;
        }

        count--;

        switch (dataType)
        {
            case IO_DATA_TYPE_TRIGGER:

                // No Value.
                dataSample = dataSample_CreateTrigger(timestamp);
                break;

            case IO_DATA_TYPE_BOOLEAN:
            {
                bool value;
                if (ReadFromFile(&value, sizeof(value), file) != LE_OK)
                {
                    LE_CRIT("Failed to read boolean value.");
                    goto error;
                }
                dataSample = dataSample_CreateBoolean(timestamp, value);
                break;
            }
            case IO_DATA_TYPE_NUMERIC:
            {
                double value;
                if (ReadFromFile(&value, sizeof(value), file) != LE_OK)
                {
                    LE_CRIT("Failed to read numeric value.");
                    goto error;
                }
                dataSample = dataSample_CreateNumeric(timestamp, value);
                break;
            }
            case IO_DATA_TYPE_STRING:
            {
                char value[HUB_MAX_STRING_BYTES];

                uint32_t stringLen;
                if (ReadFromFile(&stringLen, 4, file) != LE_OK)
                {
                    LE_CRIT("Failed to read string length.");
                    goto error;
                }
                if (stringLen > (sizeof(value) - 1))
                {
                    LE_CRIT("String length (%zu) is larger than permitted (%zu).",
                            (size_t)stringLen,
                            sizeof(value) - 1);
                    le_atomFile_CancelStream(file);
                    goto error;
                }
                if (ReadFromFile(value, stringLen, file) != LE_OK)
                {
                    LE_CRIT("Failed to read string value of length %zu.", (size_t)stringLen);
                    goto error;
                }
                value[stringLen] = '\0';
                dataSample = dataSample_CreateString(timestamp, value);
                break;
            }
            case IO_DATA_TYPE_JSON:
            {
                char value[HUB_MAX_STRING_BYTES];

                uint32_t stringLen;
                if (ReadFromFile(&stringLen, 4, file) != LE_OK)
                {
                    LE_CRIT("Failed to read JSON object length.");
                    goto error;
                }
                if (stringLen > (sizeof(value) - 1))
                {
                    LE_CRIT("JSON string length (%zu) is larger than permitted (%zu).",
                            (size_t)stringLen,
                            sizeof(value) - 1);
                    le_atomFile_CancelStream(file);
                    goto error;
                }
                if (ReadFromFile(value, stringLen, file) != LE_OK)
                {
                    LE_CRIT("Failed to read JSON value of length %zu.", (size_t)stringLen);
                    goto error;
                }
                value[stringLen] = '\0';
                dataSample = dataSample_CreateJson(timestamp, value);
                break;
            }
        }

        // Add the sample to the buffer, unless this is the last (newest) sample, in which case
        // we will push it to the Observation later when we confirm that the file doesn't have
        // more than expected in it (which would mean that all these samples are probably corrupt
        // and need to be discarded).
        if (count != 0 && dataSample)
        {
            if (AddToBuffer(obsPtr, dataSample) != LE_OK)
            {
                goto error;
            }
        }
        else if (!dataSample)
        {
            // Failed to allocate a data sample and therefore cannot proceed with read.
            LE_ERROR("Failed to allocate dataSample for value read from file");
            goto error;
        }
    }

error:

    // On error, dump the buffer contents in case we read some corrupted samples from the file.
    TruncateBuffer(obsPtr, 0);
}
#endif /* end LE_CONFIG_FILESYSTEM */


//--------------------------------------------------------------------------------------------------
/**
 * Perform a backup to non-volatile storage of an observation's data sample buffer.
 */
//--------------------------------------------------------------------------------------------------
static void Backup
(
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    // If the backup timer exists, delete it.
    if (obsPtr->backupTimer != NULL)
    {
        le_timer_Delete(obsPtr->backupTimer);
        obsPtr->backupTimer = NULL;
    }

    // Update the time of last backup.
    le_clk_Time_t now = le_clk_GetRelativeTime();
    obsPtr->lastBackupTime = now.sec;

#if LE_CONFIG_FILESYSTEM
    // Get the backup file path.
    char path[MAX_BACKUP_FILE_PATH_BYTES];
    if (GetBackupFilePath(path, sizeof(path), obsPtr) != LE_OK)
    {
        return;
    }

    LE_DEBUG("Backing up to '%s'...", path);

    // Create the backup directory, if it doesn't exist already.
    struct stat st = {0};
    if (stat(BACKUP_DIR, &st) == -1)
    {
        LE_DEBUG("Creating directory '" BACKUP_DIR "'.");

        if (mkdir(BACKUP_DIR, 0700) == -1)
        {
            LE_CRIT("Unable to create directory '" BACKUP_DIR "' (%m).");
            return;
        }
    }

    // Open the file for writing, truncating it to zero length to start.
    le_result_t result;
    FILE* file = le_atomFile_CreateStream(path,
                                          LE_FLOCK_WRITE,
                                          LE_FLOCK_REPLACE_IF_EXIST,
                                          0600,
                                          &result);
    if (result != LE_OK)
    {
        LE_CRIT("Unable to open file '%s' for writing (%s).", path, LE_RESULT_TXT(result));
        return;
    }

    // Write in the version byte.
    uint8_t byte = 0;
    if (!WriteToStream(file, &byte, 1))
    {
        return;
    }

    // Write the data type code.
    byte = GetDataTypeCode(obsPtr);
    if (byte == 0)
    {
        le_atomFile_CancelStream(file);
        return;
    }
    if (!WriteToStream(file, &byte, 1))
    {
        return;
    }

    // Write in the number of samples.
    uint32_t count = obsPtr->count;
    if (!WriteToStream(file, &count, 4))
    {
        return;
    }

    // Write all the data samples to the file.
    if (!WriteSamplesToFile(file, obsPtr))
    {
        return;
    }

    // Commit the file.
    result = le_atomFile_CloseStream(file);
    if (result != LE_OK)
    {
        LE_CRIT("Failed to save '%s' (%s).", path, LE_RESULT_TXT(result));
    }
#else /* !LE_CONFIG_FILESYSTEM */
    // TODO: implement non-volatile storage without a filesystem.
#endif /* end !LE_CONFIG_FILESYSTEM */

    LE_DEBUG("Backup complete.");
}


//--------------------------------------------------------------------------------------------------
/**
 * Disable backups of a given Observation's data sample buffer.
 */
//--------------------------------------------------------------------------------------------------
static void DisableBackups
(
    Observation_t* obsPtr
)
//--------------------------------------------------------------------------------------------------
{
    if (obsPtr->backupTimer != NULL)
    {
        le_timer_Stop(obsPtr->backupTimer);
        le_timer_Delete(obsPtr->backupTimer);
        obsPtr->backupTimer = NULL;
    }

    obsPtr->lastBackupTime = 0;

    DeleteBackup(obsPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Timer expiry handler function for Observation data sample buffer backup timers.
 *
 * Saves the contents of an Observation's data sample buffer to non-volatile storage.
 *
 * @note The timer should only be running if a data sample arrived in the buffer before the
 *       backup period had elapsed since the previous backup.
 */
//--------------------------------------------------------------------------------------------------
static void BackupTimerExpired
(
    le_timer_Ref_t timer
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = le_timer_GetContextPtr(timer);

    Backup(obsPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Observation module.
 *
 * @warning This must be called before any other function in this module is called.
 */
//--------------------------------------------------------------------------------------------------
void obs_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    ObservationPool = le_mem_InitStaticPool(ObservationPool, DEFAULT_OBSERVATION_POOL_SIZE,
                        sizeof(Observation_t));
    le_mem_SetDestructor(ObservationPool, ObservationDestructor);

    BufferEntryPool = le_mem_InitStaticPool(BufferEntryPool, DEFAULT_BUFFER_ENTRY_POOL_SIZE,
                        sizeof(BufferEntry_t));
    le_mem_SetDestructor(BufferEntryPool, BufferEntryDestructor);

    ReadOperationPool = le_mem_InitStaticPool(ReadOperationPool,
                                              DEFAULT_READ_OPERATION_POOL_SIZE,
                                              sizeof(ReadOperation_t));
}


//--------------------------------------------------------------------------------------------------
/**
 * Create an Observation object.  This allocates the object and initializes the class members,
 * but not the parent class members.
 *
 * @return Pointer to the new object or NULL if failed to allocate an observation.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* obs_Create
(
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = hub_MemAlloc(ObservationPool);

    if (obsPtr == NULL)
    {
        LE_ERROR("Failed to allocate observation");
        return NULL;
    }
    res_Construct(&obsPtr->resource, entryRef);

    obsPtr->lowLimit = NAN;
    obsPtr->highLimit = NAN;
    obsPtr->changeBy = NAN;
    obsPtr->minPeriod = NAN;

    obsPtr->maxCount = 0;
    obsPtr->count = 0;

    obsPtr->transformType = OBS_TRANSFORM_TYPE_NONE;

    obsPtr->bufferedType = IO_DATA_TYPE_TRIGGER;

    obsPtr->backupPeriod = 0;
    obsPtr->lastBackupTime = 0;
    obsPtr->backupTimer = NULL;

    obsPtr->sampleList = LE_SLS_LIST_INIT;

    obsPtr->readOpList = LE_DLS_LIST_INIT;

    obsPtr->jsonExtraction[0] = '\0';

    obsPtr->destination[0] = '\0';

    return &obsPtr->resource;
}


//--------------------------------------------------------------------------------------------------
/**
 * Restore an Observation's data buffer from non-volatile backup, if one exists.
 */
//--------------------------------------------------------------------------------------------------
void obs_RestoreBackup
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
#if LE_CONFIG_FILESYSTEM
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // If there's no backup directory yet, then we know there are no backups, so don't
    // try opening one (which would result in an error message in the logs because the lock file
    // can't be created).
    struct stat st = {0};
    if (stat(BACKUP_DIR, &st) == -1)
    {
        LE_DEBUG("Backup directory '" BACKUP_DIR "' not found. (%m)");
        return;
    }

    char path[MAX_BACKUP_FILE_PATH_BYTES];
    if (GetBackupFilePath(path, sizeof(path), obsPtr) != LE_OK)
    {
        return;
    }

    LE_INFO("Loading observation buffer from file '%s'.", path);

    // Open the file for reading.
    le_result_t result;
    FILE* file = le_atomFile_OpenStream(path, LE_FLOCK_READ, &result);
    if (result != LE_OK)
    {
        LE_DEBUG("Unable to open '%s' for reading (%s).", path, LE_RESULT_TXT(result));
        return;
    }

    // Read the version byte.
    uint8_t byte;
    if (ReadFromFile(&byte, 1, file) != LE_OK)
    {
        LE_ERROR("Failed to read version byte.");
        return;
    }
    if (byte != 0)
    {
        LE_CRIT("Backup file format version %d unrecognized.", (int)byte);
        le_atomFile_CancelStream(file);
        return;
    }

    // Read the data type code.
    if (ReadFromFile(&byte, 1, file) != LE_OK)
    {
        LE_ERROR("Failed to read data type code.");
        return;
    }
    io_DataType_t dataType;
    if (!GetDataTypeFromCode(&dataType, byte))
    {
        le_atomFile_CancelStream(file);
        return;
    }
    obsPtr->bufferedType = dataType;

    // Read the number of samples.
    uint32_t count;
    if (ReadFromFile(&count, 4, file) != LE_OK)
    {
        LE_ERROR("Failed to read number of samples.");
        return;
    }

    // The maximum count must be at least the number we read.
    if (obsPtr->maxCount == 0)
    {
        obsPtr->maxCount = count;
    }
    // NOTE: Don't enable backups, though, because we don't know the frequency to choose
    //       and flash wear can permanently damage a device.

    // Read all the data samples from the file.
    ReadSamplesFromFile(obsPtr, file, count);
#else /* !LE_CONFIG_FILESYSTEM */
    // TODO: read from non-volatile storage without a filesystem.
    LE_UNUSED(resPtr);
#endif /* end !LE_CONFIG_FILESYSTEM */
}


//--------------------------------------------------------------------------------------------------
/**
 * Perform JSON extraction.  If the data type is not JSON, does nothing.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t obs_DoJsonExtraction
(
    res_Resource_t* resPtr,
    io_DataType_t* dataTypePtr,     ///< [INOUT] the data type, may be changed by JSON extraction
    dataSample_Ref_t* valueRefPtr   ///< [INOUT] the data sample, may be replaced by JSON extraction
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // If JSON extraction is enabled,
    if (obsPtr->jsonExtraction[0] != '\0')
    {
        if (*dataTypePtr != IO_DATA_TYPE_JSON)
        {
            LE_WARN("Ignoring non-JSON value pushed to observation configured to extract JSON.");
            return LE_FAULT;
        }

        // Extract the appropriate JSON data element from the value.
        io_DataType_t extractedType;
        dataSample_Ref_t extractedValue = dataSample_ExtractJson(*valueRefPtr,
                                                                 obsPtr->jsonExtraction,
                                                                 &extractedType);
        if (extractedValue == NULL)
        {
            // Extraction failed.
            return LE_FAULT;
        }

        // Extraction succeeded, so replace value data sample with extracted one.
        le_mem_Release(*valueRefPtr);
        *valueRefPtr = extractedValue;
        *dataTypePtr = extractedType;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Determine whether the value should be accepted by a given Observation.
 *
 * @warning JSON extraction should be performed first if the data type is JSON.
 *
 * @return true if the value should be accepted.
 */
//--------------------------------------------------------------------------------------------------
bool obs_ShouldAccept
(
    res_Resource_t* resPtr,
    io_DataType_t dataType,     ///< [IN] the data type
    dataSample_Ref_t valueRef   ///< [IN] the data sample
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // Check the high limit and low limit before other limits.
    if (dataType == IO_DATA_TYPE_NUMERIC)
    {
        double numericValue = dataSample_GetNumeric(valueRef);

        // If both limits are enabled and the low limit is higher than the high limit, then
        // this is the "deadband" case. ( - <------HxxxxxxxxxL------> + )
        if (   (!isnan(obsPtr->highLimit))
            && (!isnan(obsPtr->lowLimit))
            && (obsPtr->lowLimit > obsPtr->highLimit)  )
        {
            if ((numericValue < obsPtr->lowLimit) && (numericValue > obsPtr->highLimit))
            {
                return false;
            }
        }
        // In all other cases, reject if lower than non-NAN low limit or higher than non-NAN high.
        else
        {
            if ((!isnan(obsPtr->lowLimit)) && (numericValue < obsPtr->lowLimit))
            {
                return false;
            }

            if ((!isnan(obsPtr->highLimit)) && (numericValue > obsPtr->highLimit))
            {
                return false;
            }
        }
    }

    // The current time in ms since some unspecified start time.
    uint32_t now = 0;

    // If we have received a push before (giving us something to compare against),
    // Check the minPeriod and changeBy,
    dataSample_Ref_t previousValue = res_GetCurrentValue(resPtr);
    if (previousValue != NULL)
    {
        // If there is a changedBy filter in effect,
        if ((obsPtr->changeBy != 0) && (!isnan(obsPtr->changeBy)))
        {
            // If overridden, reject everything because the value won't change.
            if (res_IsOverridden(resPtr))
            {
                return false;
            }

            // If the data type has changed, we can't do a comparison, so only check the changeBy
            // filter if the new data sample's type is the same as the previous current value's
            // type.
            if (dataType == res_GetDataType(resPtr))
            {
                // If this is a numeric value and the last value was numeric too,
                if (dataType == IO_DATA_TYPE_NUMERIC)
                {
                    // Reject changes in the current value smaller than the changeBy setting.
                    double previousNumber = dataSample_GetNumeric(previousValue);
                    if (  fabs(dataSample_GetNumeric(valueRef) - previousNumber)
                        < obsPtr->changeBy)
                    {
                        return false;
                    }
                }
                // For Boolean, a non-zero changeBy means filter out if unchanged.
                else if (dataType == IO_DATA_TYPE_BOOLEAN)
                {
                    if (dataSample_GetBoolean(valueRef) == dataSample_GetBoolean(previousValue))
                    {
                        return false;
                    }
                }
                // For string or JSON, a non-zero changeBy means filter out if unchanged.
                else if (   (dataType == IO_DATA_TYPE_STRING)
                         || (dataType == IO_DATA_TYPE_JSON))
                {
                    if (0 == strcmp(dataSample_GetString(valueRef),
                                    dataSample_GetString(previousValue)))
                    {
                        return false;
                    }
                }
            }
        }

        // All of the above can be done without a system call, so that's why we do the
        // minPeriod check last.
        if ((obsPtr->minPeriod != 0) && (!isnan(obsPtr->minPeriod)))
        {
            now = GetRelativeTimeMs();  // system call

            if ((now - obsPtr->lastPushTime) < (obsPtr->minPeriod * 1000))
            {
                return false;
            }
        }
    }

    // Update the time of last update.
    if (now == 0)
    {
        now = GetRelativeTimeMs(); // system call
    }
    obsPtr->lastPushTime = now;

    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Perform processing of an accepted pushed data sample that is specific to an Observation
 * resource.
 */
//--------------------------------------------------------------------------------------------------
void obs_ProcessAccepted
(
    res_Resource_t* resPtr,
    io_DataType_t dataType,     ///< Data type of the data sample.
    dataSample_Ref_t sampleRef  ///< Data sample.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    if (obsPtr->maxCount > 0)
    {
        // If the data type has changed, we have to dump the current set of buffered samples.
        if (obsPtr->bufferedType != dataType)
        {
            TruncateBuffer(obsPtr, 0);

            obsPtr->bufferedType = dataType;
        }

        if (AddToBuffer(obsPtr, sampleRef) != LE_OK)
        {
            LE_ERROR("Failed to an accepted sample to buffer");
            return;
        }

        TruncateBuffer(obsPtr, obsPtr->maxCount);

        // If the buffer backup period is non-zero, then back-ups are enabled.
        if (obsPtr->backupPeriod > 0)
        {
            // If more than the backup period has passed since the time of last backup, do a backup.
            uint32_t nextBackupTime = obsPtr->lastBackupTime + obsPtr->backupPeriod;
            le_clk_Time_t now = le_clk_GetRelativeTime();
            if (nextBackupTime <= now.sec)
            {
                Backup(obsPtr);
            }
            // If the backup period hasn't passed yet, and there isn't already a timer running,
            // then start a timer to expire when it's time to do a backup.
            else if (obsPtr->backupTimer == NULL)
            {
                uint32_t timerInterval = (nextBackupTime - now.sec) * 1000;

                obsPtr->backupTimer = le_timer_Create("backup");
                LE_ASSERT(le_timer_SetMsInterval(obsPtr->backupTimer, timerInterval) == LE_OK);
                LE_ASSERT(le_timer_SetHandler(obsPtr->backupTimer, BackupTimerExpired) == LE_OK);
                LE_ASSERT(le_timer_SetContextPtr(obsPtr->backupTimer, obsPtr) == LE_OK);
                LE_ASSERT(le_timer_Start(obsPtr->backupTimer) == LE_OK);
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Perform any post-filtering on a given Observation.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t obs_ApplyTransform
(
    res_Resource_t* resPtr,
    io_DataType_t dataType,     ///< Data type of the data sample.
    dataSample_Ref_t sampleRef  ///< Data sample.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);
    dataSample_Ref_t sample = sampleRef;
    double transformVal;

    switch (obsPtr->transformType)
    {
        case OBS_TRANSFORM_TYPE_NONE:
            goto done;

        case OBS_TRANSFORM_TYPE_MEAN:
            transformVal = obs_QueryMean(resPtr, NAN);
            break;

        case OBS_TRANSFORM_TYPE_STDDEV:
            transformVal = obs_QueryStdDev(resPtr, NAN);
            break;

        case OBS_TRANSFORM_TYPE_MAX:
            transformVal = obs_QueryMax(resPtr, NAN);
            break;

        case OBS_TRANSFORM_TYPE_MIN:
            transformVal = obs_QueryMin(resPtr, NAN);
            break;

        default:
            LE_FATAL("Invalid transform type %d", obsPtr->transformType);
            break;
    }

    // Update the sample to have the new, transformed value.
    sample = UpdateSample(sampleRef, dataType, (void *)&transformVal);

done:
    return sample;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum period between data samples accepted by a given Observation.
 *
 * This is used to throttle the rate of data passing into and through an Observation.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetMinPeriod
(
    res_Resource_t* resPtr,
    double minPeriod
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    obsPtr->minPeriod = minPeriod;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum period between data samples accepted by a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double obs_GetMinPeriod
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->minPeriod;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the highest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetHighLimit
(
    res_Resource_t* resPtr,
    double highLimit
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    obsPtr->highLimit = highLimit;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the highest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double obs_GetHighLimit
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->highLimit;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the lowest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetLowLimit
(
    res_Resource_t* resPtr,
    double lowLimit
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    obsPtr->lowLimit = lowLimit;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the lowest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double obs_GetLowLimit
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->lowLimit;
}


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
void obs_SetChangeBy
(
    res_Resource_t* resPtr,
    double change
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    obsPtr->changeBy = change;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double obs_GetChangeBy
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->changeBy;
}


//--------------------------------------------------------------------------------------------------
/**
 * Perform a transform on buffered data. Value of the observation will be the output of the
 * transform
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetTransform
(
    res_Resource_t* resPtr,
    obs_TransformType_t transformType,
    const double* paramsPtr,
    size_t paramsSize
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    obsPtr->transformType = transformType;

    // If the transform is being set to anything other than NONE, ensure there is at least one
    // data sample buffered in order to allow transforms to behave properly
    if (   (OBS_TRANSFORM_TYPE_NONE != obsPtr->transformType)
        && (0 == obsPtr->maxCount))
    {
        obsPtr->maxCount = 1;
    }

    // Clear the buffer and current value of the observation.  Do this even if the same transform
    // is being re-applied.  This allows any cumulative behavior to be cleared
    TruncateBuffer(obsPtr, 0);
    if (resPtr->pushedValue != NULL)
    {
        le_mem_Release(resPtr->pushedValue);
        resPtr->pushedValue = NULL;
    }

    (void)paramsPtr;
    (void)paramsSize;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the type of transform currently applied to an Observation.
 *
 * @return The TransformType
 */
//--------------------------------------------------------------------------------------------------
obs_TransformType_t obs_GetTransform
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->transformType;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of data samples to buffer in a given Observation.  Buffers are FIFO
 * circular buffers. When full, the buffer drops the oldest value to make room for a new addition.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetBufferMaxCount
(
    res_Resource_t* resPtr,
    uint32_t count
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // If the buffer size is being changed,
    if (obsPtr->maxCount != count)
    {
        // If the size is now zero and backups were enabled, disable backups.
        if ((count == 0) && (obsPtr->backupPeriod > 0))
        {
            DisableBackups(obsPtr);
        }

        // Update the size.
        obsPtr->maxCount = count;

        // Discard extra samples if the size has shrunk.
        TruncateBuffer(obsPtr, count);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the buffer size setting for a given Observation.
 *
 * @return The buffer size (in number of samples) or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
uint32_t obs_GetBufferMaxCount
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->maxCount;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum time between backups of an Observation's buffer to non-volatile storage.
 * If the buffer's size is non-zero and the backup period is non-zero, then the buffer will be
 * backed-up to non-volatile storage when it changes, but never more often than this period setting
 * specifies.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetBufferBackupPeriod
(
    res_Resource_t* resPtr,
    uint32_t seconds
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    uint32_t oldPeriod = obsPtr->backupPeriod;

    // If the period is being changed,
    if (oldPeriod != seconds)
    {
        obsPtr->backupPeriod = seconds;

        // If the buffer size is zero, then backups aren't done, so we can skip the rest.
        if (obsPtr->maxCount > 0)
        {
            // If the period is now zero, disable backups.
            if (seconds == 0)
            {
                DisableBackups(obsPtr);
            }
            // If there's nothing in the buffer, we can skip the rest and just wait for something
            // to be added to the buffer.
            else if (!le_sls_IsEmpty(&obsPtr->sampleList))
            {
                // If backups were already enabled and the period has just changed,
                if (oldPeriod != 0)
                {
                    // If the timer is running, then we know there's something waiting to be
                    // backed up.  Otherwise, we wait for something to be added to the buffer.
                    if (obsPtr->backupTimer != NULL)
                    {
                        // Stop the old timer, because we know it has the wrong interval.
                        le_timer_Stop(obsPtr->backupTimer);

                        // If the backup period has passed since the last backup,
                        // release the timer and do a backup now.
                        uint32_t nextBackupTime = obsPtr->lastBackupTime + seconds;
                        le_clk_Time_t now = le_clk_GetRelativeTime();
                        if (nextBackupTime < now.sec)
                        {
                            le_timer_Delete(obsPtr->backupTimer);
                            obsPtr->backupTimer = NULL;

                            Backup(obsPtr);
                        }
                        else // If the backup period has not yet passed, correct the timer's
                             // interval and restart it.
                        {
                            uint32_t timerInterval = (nextBackupTime - now.sec) * 1000;
                            LE_ASSERT(le_timer_SetMsInterval(obsPtr->backupTimer, timerInterval)
                                      == LE_OK);
                            LE_ASSERT(le_timer_Start(obsPtr->backupTimer) == LE_OK);
                        }
                    }
                }
            }
        }
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
uint32_t obs_GetBufferBackupPeriod
(
    res_Resource_t* resPtr
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->backupPeriod;
}


#if LE_CONFIG_LINUX
//--------------------------------------------------------------------------------------------------
/**
 * Function that gets called for each file system object (file, directory, symlink, etc.) found
 * under the backup directory.
 *
 * @return 0 to continue the file system tree walk, non-zero to stop.
 */
//--------------------------------------------------------------------------------------------------
static int BackupDirTreeWalkCallback
(
    const char *fpath,      ///< File system path of the object.
    const struct stat *sb,  ///< Ptr to the stat() info for the object.
    int typeflag,           ///< What type of file system object we're looking at.
    struct FTW *ftwbuf      ///< Ptr to buffer containing the basename and the nesting level.
)
//--------------------------------------------------------------------------------------------------
{
    switch (typeflag)
    {
        case FTW_F:  // regular file
        {
            // Compute the resource tree entry path of the associated Observation.
            const char* relPath = fpath + BACKUP_DIR_PATH_LEN;
            const char* suffixPtr = strstr(relPath, BACKUP_SUFFIX);
            if (suffixPtr == NULL)
            {
                LE_WARN("Unexpected file in backup directory. Skipping '%s'.", fpath);
                return 0;
            }
            // Copy all but the suffix into the observation path.
            size_t obsPathBytes = (suffixPtr - relPath) + sizeof("/obs/");
            char obsPath[HUB_MAX_RESOURCE_PATH_BYTES];
            if (obsPathBytes >= sizeof(obsPath))
            {
                LE_ERROR("Length of path too long. Skipping '%s'.", fpath);
                return 0;
            }
            (void)snprintf(obsPath, obsPathBytes, "/obs/%s", relPath);

            // If that Observation doesn't exist, or its backup period is 0, delete the file.
            resTree_EntryRef_t entryRef = resTree_FindEntry(resTree_GetRoot(), obsPath);
            if (   (entryRef == NULL)
                || (resTree_GetEntryType(entryRef) != ADMIN_ENTRY_TYPE_OBSERVATION)
                || (resTree_GetBufferBackupPeriod(entryRef) == 0)  )
            {
                if (unlink(fpath) != 0)
                {
                    LE_CRIT("Failed to delete '%s' (%m).", fpath);
                }
            }

            return 0;
        }
        case FTW_D:  // directory and FTW_DEPTH was NOT specified

            // This should never happen, because we specified FTW_DEPTH.
            LE_CRIT("Received FTW_D flag for '%s'!", fpath);
            return 0;

        case FTW_DNR:   // directory that can't be read

            LE_ERROR("Can't read directory '%s'", fpath);
            return 0;

        case FTW_DP:    // directory and FTW_DEPTH was specified
        {
            // If the directory is empty, delete it.
            int result = rmdir(fpath);
            if ((result == -1) && (errno != ENOTEMPTY) && (errno != EEXIST))
            {
                LE_CRIT("Failed to remove directory '%s' (%m).", fpath);
            }
            return 0;
        }
        case FTW_NS:     // stat() call failed on fpath

            LE_CRIT("Failed to stat '%s'", fpath);
            return 0;

        case FTW_SL:     // symbolic link and FTW_PHYS specified

            // This should never happen, because we didn't specify FTW_PHYS.
            LE_CRIT("Received FTW_SL flag for '%s'!", fpath);
            return 0;

        case FTW_SLN:    // symbolic link pointing to a nonexistent file

            LE_CRIT("Broken symlink found at '%s'", fpath);
            return 0;
    }

    LE_CRIT("Unexpected type flag %d.", typeflag);

    return -1;
}
#endif /* end LE_CONFIG_LINUX */


//--------------------------------------------------------------------------------------------------
/**
 * Delete buffer backup files that aren't being used.
 */
//--------------------------------------------------------------------------------------------------
void obs_DeleteUnusedBackupFiles
(
    void
)
//--------------------------------------------------------------------------------------------------
{
#if LE_CONFIG_LINUX
    LE_DEBUG("Cleaning up unused buffer backup files.");

    // Walk the directory tree under the backup directory.
    // For each file, compute the resource tree entry path of the associated Observation.
    // If that Observation doesn't exist, delete the file.
    // If the directory is empty (after examining all files in it), delete the directory.
    // Note: The number of file descriptors allowed to be used is selected to prevent
    // running into the app's open file descriptor limit, while avoiding a lot of opening
    // and closing of directories.  Normally we don't expect a lot of depth in the Observation
    // resource naming heirarchy, so there shouldn't be a lot of depth in the backup directory.
    int result = nftw(BACKUP_DIR,
                      BackupDirTreeWalkCallback,
                      4 /* max fds */,
                      FTW_DEPTH /* depth-first traversal */);
    if (result != 0)
    {
        if (errno == ENOENT)
        {
            LE_DEBUG("No backup directory. Skipping backup file clean-up.");
        }
        else
        {
            LE_CRIT("Failed to traverse backup directory '%s' (%m)", BACKUP_DIR);
        }
    }
#endif /* end LE_CONFIG_LINUX */
}


//--------------------------------------------------------------------------------------------------
/**
 * Find the data sample at or after a given timestamp in a given Observation's buffer.
 *
 * @return a pointer to the buffer entry, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
static BufferEntry_t* FindBufferEntry
(
    Observation_t* obsPtr,
    double startTime   ///< NAN for oldest; if < 30 years, count back from now; else absolute time.
)
//--------------------------------------------------------------------------------------------------
{
    // Start at the oldest end and search forward.
    BufferEntry_t* buffEntryPtr = GetOldestBufferEntry(obsPtr);

    // If the buffer isn't empty and the startTime was specified,
    if ((buffEntryPtr != NULL) && (!isnan(startTime)))
    {
        // If the start time is less than or equal to 30 years, then convert to an
        // absolute timestamp by subtracting it from the current time.
        if (startTime <= THIRTY_YEARS)
        {
            le_clk_Time_t now = le_clk_GetAbsoluteTime();
            startTime = ((((double)(now.usec)) / 1000000) + now.sec) - startTime;
        }

        // Walk up the buffer looking for an entry that is the same age or newer than the
        // specified start time.
        do
        {
            if (dataSample_GetTimestamp(buffEntryPtr->sampleRef) >= startTime)
            {
                break;
            }

            buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);

        } while (buffEntryPtr != NULL);
    }

    return buffEntryPtr;
}


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
void obs_ReadBufferJson
(
    res_Resource_t* resPtr, ///< Ptr to the resource object for the Observation.
    double startAfter,  ///< Start after this many seconds ago, or after an absolute number of
                        ///< seconds since the Epoch (if startafter > 30 years).
                        ///< Use NAN (not a number) to read the whole buffer.
    int outputFile, ///< File descriptor to write the data to.
    query_ReadCompletionFunc_t handlerPtr, ///< Completion callback.
    void* contextPtr    ///< Value to be passed to completion callback.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    BufferEntry_t* startPtr = FindBufferEntry(obsPtr, startAfter);

    // If the data sample found is an exact match for the startAfter time, then skip to the
    // sample after that.
    if ((startPtr != NULL) && (dataSample_GetTimestamp(startPtr->sampleRef) == startAfter))
    {
        startPtr = GetNextBufferEntry(obsPtr, startPtr);
    }

    StartRead(obsPtr, startPtr, outputFile, handlerPtr, contextPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Find the oldest data sample in a given Observation's buffer that is newer than a given timestamp.
 *
 * @return Reference to the sample, or NULL if not found in buffer.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t obs_FindBufferedSampleAfter
(
    res_Resource_t* resPtr, ///< Ptr to the resource object for the Observation.
    double startAfter   ///< Start after this many seconds ago, or after an absolute number of
                        ///< seconds since the Epoch (if startafter > 30 years).
                        ///< Use NAN (not a number) to find the oldest.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    BufferEntry_t* startPtr = FindBufferEntry(obsPtr, startAfter);

    // If the data sample found is an exact match for the startAfter time, then skip to the
    // sample after that.
    if ((startPtr != NULL) && (dataSample_GetTimestamp(startPtr->sampleRef) == startAfter))
    {
        startPtr = GetNextBufferEntry(obsPtr, startPtr);
    }

    if (startPtr != NULL)
    {
        return startPtr->sampleRef;
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * If this is set, all non-JSON data will be ignored, and all JSON data that does not contain the
 * the specified object member or array element will also be ignored.
 */
//--------------------------------------------------------------------------------------------------
void obs_SetJsonExtraction
(
    res_Resource_t* resPtr,  ///< Observation resource.
    const char* extractionSpec    ///< [IN] string specifying the JSON member/element to extract.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    LE_ASSERT(LE_OK == le_utf8_Copy(obsPtr->jsonExtraction,
                                    extractionSpec,
                                    sizeof(obsPtr->jsonExtraction),
                                    NULL));
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the JSON member/element specifier for extraction of data from within a structured JSON
 * value received by a given Observation.
 *
 * @return Ptr to string containing JSON extraction specifier.  "" if not set.
 */
//--------------------------------------------------------------------------------------------------
const char* obs_GetJsonExtraction
(
    res_Resource_t* resPtr  ///< Observation resource.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    return obsPtr->jsonExtraction;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get buffer entry numerical value.  This works for numeric or Boolean types only.
 *
 * @return The value.
 */
//--------------------------------------------------------------------------------------------------
static double GetBufferedNumber
(
    BufferEntry_t* buffEntryPtr,
    io_DataType_t dataType
)
//--------------------------------------------------------------------------------------------------
{
    if (dataType == IO_DATA_TYPE_NUMERIC)
    {
        return dataSample_GetNumeric(buffEntryPtr->sampleRef);
    }
    else if (dataType == IO_DATA_TYPE_BOOLEAN)
    {
        if (dataSample_GetBoolean(buffEntryPtr->sampleRef))
        {
            return 1.0;
        }
        else
        {
            return 0.0;
        }
    }
    else
    {
        LE_CRIT("Non-numerical data type %d.", dataType);
        return NAN;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum value found in an Observation's data set within a given time span.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double obs_QueryMin
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // This only works for numeric or Boolean type data.
    if (   (obsPtr->bufferedType != IO_DATA_TYPE_NUMERIC)
        && (obsPtr->bufferedType != IO_DATA_TYPE_BOOLEAN)  )
    {
        return NAN;
    }

    BufferEntry_t* buffEntryPtr = FindBufferEntry(obsPtr, startTime);

    double result = NAN;

    while (buffEntryPtr != NULL)
    {
        double value = GetBufferedNumber(buffEntryPtr, obsPtr->bufferedType);

        if (!isnan(value))
        {
            if (isnan(result) || (result > value))
            {
                result = value;
            }
        }

        buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the maximum value found within a given time span in an Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double obs_QueryMax
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // This only works for numeric or Boolean type data.
    if (   (obsPtr->bufferedType != IO_DATA_TYPE_NUMERIC)
        && (obsPtr->bufferedType != IO_DATA_TYPE_BOOLEAN)  )
    {
        return NAN;
    }

    BufferEntry_t* buffEntryPtr = FindBufferEntry(obsPtr, startTime);

    double result = NAN;

    while (buffEntryPtr != NULL)
    {
        double value = GetBufferedNumber(buffEntryPtr, obsPtr->bufferedType);

        if (!isnan(value))
        {
            if (isnan(result) || (result < value))
            {
                result = value;
            }
        }

        buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the mean (average) of all values found within a given time span in an Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
double obs_QueryMean
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // This only works for numeric or Boolean type data.
    if (   (obsPtr->bufferedType != IO_DATA_TYPE_NUMERIC)
        && (obsPtr->bufferedType != IO_DATA_TYPE_BOOLEAN)  )
    {
        return NAN;
    }

    BufferEntry_t* buffEntryPtr = FindBufferEntry(obsPtr, startTime);

    double sum = 0;
    size_t count = 0;

    while (buffEntryPtr != NULL)
    {
        double value = GetBufferedNumber(buffEntryPtr, obsPtr->bufferedType);

        if (!isnan(value))
        {
            sum += value;
            count++;
        }

        buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);
    }

    if (count == 0)
    {
        return NAN;
    }

    return (sum / count);
}


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
double obs_QueryStdDev
(
    res_Resource_t* resPtr,    ///< Ptr to Observation resource.
    double startTime    ///< If < 30 years then seconds before now; else seconds since the Epoch.
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    // This only works for numeric or Boolean type data.
    if (   (obsPtr->bufferedType != IO_DATA_TYPE_NUMERIC)
        && (obsPtr->bufferedType != IO_DATA_TYPE_BOOLEAN)  )
    {
        return NAN;
    }

    BufferEntry_t* startEntryPtr = FindBufferEntry(obsPtr, startTime);

    if (startEntryPtr == NULL)
    {
        return NAN;
    }

    double sum = 0;
    size_t count = 0;

    BufferEntry_t* buffEntryPtr = startEntryPtr;
    while (buffEntryPtr != NULL)
    {
        double value = GetBufferedNumber(buffEntryPtr, obsPtr->bufferedType);

        if (!isnan(value))
        {
            sum += value;
            count++;
        }

        buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);
    }

    if (count == 0)
    {
        return NAN;
    }

    const double mean = (sum / count);

    double sumOfSquaredDifferences = 0;

    buffEntryPtr = startEntryPtr;
    while (buffEntryPtr != NULL)
    {
        double value = GetBufferedNumber(buffEntryPtr, obsPtr->bufferedType);

        if (!isnan(value))
        {
            double diff = value - mean;
            sumOfSquaredDifferences += (diff * diff);
        }

        buffEntryPtr = GetNextBufferEntry(obsPtr, buffEntryPtr);
    }

    return sqrt(sumOfSquaredDifferences / count);
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to get an Observation's Source path.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_BAD_PARAMETER if the path is invalid.
 *  - LE_NOT_FOUND if the resource doesn't exist or doesn't have a source.
 *  - LE_OVERFLOW if the path of the source resource won't fit in the string buffer provided.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t obs_GetSource
(
    res_Resource_t* resPtr,  ///< [IN] Ptr to Observation resource
    char* srcPath,           ///< [OUT] Ptr to source path string
    size_t srcPathSize       ///< [IN] Size of source path string buffer
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t srcEntry = resTree_GetSource(resPtr->entryRef);

    if (srcEntry == NULL)
    {
        return LE_NOT_FOUND;
    }
    char resourcePath[HUB_MAX_RESOURCE_PATH_BYTES] = {};
    le_result_t result = resTree_GetPath(resourcePath, HUB_MAX_RESOURCE_PATH_BYTES,
        resTree_GetRoot(), srcEntry);

    if (result >= 0)
    {
        const char* jsonExtraction = obs_GetJsonExtraction(resPtr);

        // Concatenate the Json Extraction to the end of the source path
        if (strcmp(jsonExtraction, "") != 0)
        {
            snprintf(srcPath, srcPathSize, "%s/%s", resourcePath, jsonExtraction);
        }
        else
        {
            snprintf(srcPath, srcPathSize, "%s", resourcePath);
        }
        return LE_OK;
    }
    else if (result == LE_OVERFLOW)
    {
        return LE_OVERFLOW;
    }

    LE_FATAL("Unexpected result %d (%s)", result, LE_RESULT_TXT(result));
}


//--------------------------------------------------------------------------------------------------
/**
 * Trigger configService to call destination push handler registered for this observation.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void obs_TriggerDestinationCallback
(
    res_Resource_t* resPtr,      ///< Ptr to Observation resource
    io_DataType_t dataType,      ///< Data type of the data sample
    dataSample_Ref_t dataSample  ///< Data sample
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t result;
    char srcPath[CONFIG_MAX_DESTINATION_SRC_BYTES];
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    if (obsPtr == NULL)
    {
        LE_ERROR("Unable to retrieve Observation, resPtr [%p]", resPtr);
        return;
    }

    // Verify the destination has been configured on the observation
    if (strlen(obsPtr->destination) == 0)
    {
        return;
    }

    // Get Observation source path
    result = obs_GetSource(resPtr, srcPath, sizeof(srcPath));
    if (result != LE_OK)
    {
        LE_ERROR("Error retrieving Observation Source path, result [%d]", result);
        return;
    }

    LE_DEBUG("[%s] Calling configServices to trigger Destination Push Handler, dataType [%d]",
             __FUNCTION__,
             dataType);

    // Trigger the Destination PushHandler
    result = configService_TriggerDestinationPushHandler(
                 obsPtr->destination,
                 resTree_GetEntryName(resPtr->entryRef),
                 srcPath,
                 dataType,
                 dataSample);

    if ((result != LE_OK) &&
        (result != LE_NOT_FOUND))
    {
        LE_ERROR("Error triggering Destination PushHandler, result [%d]", result);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the destination string for the specific Observation.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void obs_SetDestination
(
    res_Resource_t* resPtr,      ///< Ptr to Observation resource
    const char* destination      ///< Destination string
)
//--------------------------------------------------------------------------------------------------
{
    Observation_t* obsPtr = CONTAINER_OF(resPtr, Observation_t, resource);

    if (obsPtr == NULL)
    {
        LE_ERROR("Unable to retrieve Observation, resPtr [%p]", resPtr);
        return;
    }

    // Set the desination string
    strncpy(obsPtr->destination, destination, CONFIG_MAX_DESTINATION_NAME_BYTES);
}
