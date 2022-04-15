#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "io_interface.h"
#include "admin_interface.h"
#include "query_interface.h"
#include "config_interface.h"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of application names.
 */
//--------------------------------------------------------------------------------------------------
#define LE_LIMIT_APP_NAME_LEN 47

// #define CONFIG_MAX_ERROR_MSG_LEN 255
// #define CONFIG_MAX_ERROR_MSG_BYTES (CONFIG_MAX_ERROR_MSG_LEN+1)
// #define CONFIG_MAX_DESTINATION_SRC_BYTES (79+63+1)
// #define CONFIG_MAX_DESTINATION_NAME_BYTES (15+1)

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t io_GetClientSessionRef(void);
le_msg_SessionRef_t query_GetClientSessionRef(void);

le_result_t le_appInfo_GetName
(
    int32_t  pid,           ///< [IN]  PID of the process.
    char    *appNameStr,    ///< [OUT] Application name buffer.
    size_t   appNameSize    ///< [IN]  Buffer size.
);

#endif