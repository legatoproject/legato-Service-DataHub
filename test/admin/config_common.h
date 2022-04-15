
/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */
#ifndef CONFIG_COMMON_H_INCLUDE_GUARD
#define CONFIG_COMMON_H_INCLUDE_GUARD


#include "legato.h"

// Interface specific includes
#include "io_common.h"
#include "admin_common.h"

#define IFGEN_CONFIG_PROTOCOL_ID "6851d4a3a312cb3f35b1a6e0fa8e4872"
#define IFGEN_CONFIG_MSG_SIZE 50262



//--------------------------------------------------------------------------------------------------
/**
 * String used to select a supported configuration format
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_ENCODED_TYPE_LEN 15

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of the destination string (excluding null terminator).
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_DESTINATION_NAME_LEN 15

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of the destination string (including null terminator).
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_DESTINATION_NAME_BYTES 16

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of source path reported by destination push handler (excluding null terminator).
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_DESTINATION_SRC_LEN 142

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of source path reported by destination push handler (including null terminator).
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_DESTINATION_SRC_BYTES 143

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of parser error message string (excluding null terminator).
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_ERROR_MSG_LEN 255

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of parser error message string (including null terminator).
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_MAX_ERROR_MSG_BYTES 256

//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct config_DestinationPushHandler* config_DestinationPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Handler to pass the result of a configuration load request back to the caller
 * The result argument may have the following values:
 * - LE_OK            : Configuration was valid and was successfully applied.
 * - LE_FORMAT_ERROR  : Configuration is not valid due to a format error.
 * - LE_BAD_PARAMETER : A parameter in the configuration file is not valid.
 * - LE_FAULT         : An error has happened during the apply phase. Datahub has deleted all
 *  resources marked as configuration.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*config_LoadResultHandlerFunc_t)
(
        le_result_t result,
        ///< Result code
        const char* LE_NONNULL errorMsg,
        ///< Parse Error Message string
        ///< (NOTE: Only valid when result is not LE_OK)
        uint32_t fileLoc,
        ///< File location (in bytes) where error occurred
        ///< (NOTE: Only valid when result is not LE_OK)
        ///< Context (implied)
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 ** Callback function for observations in a configuration. The Datahub will call the registered
 ** handler when data is received by an observation AND the observation's destination field in the
 ** configuration matches the destination string which was passed in AddDestinationPushHandler()
 **
 ** @note
 ** - If the configuration for an observation is using JSON extraction, then the path which is
 ** passed to this handler will include the JSON extraction component. E.g. if the configuration
 ** specified an observation on /orp/status/UART1/value with a JSON extraction of "errors", the
 ** resulting path would be: /orp/status/UART1/value/errors
 */
//--------------------------------------------------------------------------------------------------
typedef void (*config_DestinationPushHandlerFunc_t)
(
        double timestamp,
        ///< Seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC)
        const char* LE_NONNULL obsName,
        ///< Name of observation from configuration
        const char* LE_NONNULL srcPath,
        ///< Source path + JSON extraction, if applicable
        io_DataType_t dataType,
        ///< Indicates type of data being returned (Bool, Numeric, or String)
        bool boolValue,
        ///< Boolean value
        double numericValue,
        ///< Numeric value
        const char* LE_NONNULL stringValue,
        ///< String or JSON string value
        void* contextPtr
        ///<
);


//--------------------------------------------------------------------------------------------------
/**
 * Get if this client bound locally.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_config_HasLocalBinding
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Init data that is common across all threads
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_config_InitCommonData
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform common initialization and open a session
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_config_OpenSession
(
    le_msg_SessionRef_t _ifgen_sessionRef,
    bool isBlocking
);

//--------------------------------------------------------------------------------------------------
/**
 * Causes the Datahub to load a configuration from a file. Any existing configuration will be
 * removed and replaced with the incoming one
 *
 * @note:
 *  If used over RPC, the filePath parameter must be local to the server.
 *
 * @return
 *  - LE_OK           : Configuration successfully loaded
 *  - LE_NOT_FOUND    : Unable to locate or retrieve configuration file.
 *  - LE_UNSUPPORTED  : Configuration encoding format is not supported.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_config_Load
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL filePath,
        ///< [IN] Path of configuration file.
        const char* LE_NONNULL encodedType,
        ///< [IN] Type of encoding used in the file
        config_LoadResultHandlerFunc_t callbackPtr,
        ///< [IN] Callback to notify caller of result
        ///< Context (implied)
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED config_DestinationPushHandlerRef_t ifgen_config_AddDestinationPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL destination,
        ///< [IN] Destination for this event(e.g. "store")
        config_DestinationPushHandlerFunc_t callbackPtr,
        ///< [IN] Destination Push Handler
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_config_RemoveDestinationPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        config_DestinationPushHandlerRef_t handlerRef
        ///< [IN]
);

#endif // CONFIG_COMMON_H_INCLUDE_GUARD
