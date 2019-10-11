//--------------------------------------------------------------------------------------------------
/**
 * @page c_periodicSensor Periodic Sensor Component
 *
 * The @b periodicSensor component can be included as part of a client program to dramatically
 * simplify the work of interfacing a periodic sensor to the Data Hub.
 *
 * The periodicSensor component is included in the client program by adding it to the client
 * application's .adef file. Add it to the list of components in an executable defined in the
 * "executables" section of the .adef.  E.g.,
 *
 * @verbatim
executables:
{
    myExe = ( myComponent periodicSensor )
}
@endverbatim
 *
 * The periodicSensor component uses the @ref c_dataHubIo, so a binding like the following is
 * required in the application's .adef file:
 *
 * @verbatim
bindings:
{
    myExe.periodicSensor.dhubIO -> dataHub.io
}
@endverbatim
 *
 * The client C code will also need to
 * @code
 * #include "periodicSensor.h".
 * @endcode
 *
 * @section c_periodicSensorCreate Creating a Periodic Sensor Interface
 *
 * Call psensor_Create() to create a sensor interface of any data type, with @b "value",
 * @b "enable", @b "period", and @b "trigger" resources.
 *
 * The @b "value" will be an @b input resource used to deliver the sensor samples to
 * the Data Hub, while the other resources will be @b outputs used to receive samples from the
 * Data Hub:
 * - The @b "period" output is used to configure the sampling period.
 * - The @b "enable" output is used to enable and disable the sensor.
 * - The @b "trigger" can be used to trigger a single sample of the sensor.
 *
 * The client provides a single call-back function to psensor_Create().  The Periodic Sensor
 * component will call that call-back function whenever it is time to take a sensor reading
 * and @ref c_periodicSensorPush "push" that reading into the Data Hub.
 *
 * psensor_CreateJson() can be used to create a sensor that delivers JSON samples to the Data Hub.
 *
 * @section c_periodicSensorPush Pushing a Sensor Reading to the Data Hub
 *
 * Both psensor_Create() and psensor_CreateJson() take a callback function that is called back
 * when it is time to read the sensor and push the sample to the Data Hub using one of the
 * following functions:
 * - psensor_PushBoolean()
 * - psensor_PushNumeric()
 * - psensor_PushString()
 * - psensor_PushJson()
 *
 * @section c_periodicSensorDestroy Destroying a Periodic Sensor Interface
 *
 * psensor_Destroy() can be used to destroy a previously created sensor interface.
 *
 * @note Clean-up will automatically happen if the process shuts down, so there's no need to
 *       call psensor_Destroy() before exiting.
 *
 * @section c_periodicSensorExample Examples
 *
 * @code
 * #include "legato.h"
 * #include "periodicSensor.h"
 *
 * static void PushSample(psensor_Ref_t ref, void *context)
 * {
 *     double sensorReading;
 *
 *     // Take sensor reading.
 *     ...
 *
 *     psensor_PushNumeric(ref, DHUB_IO_NOW, sensorReading);
 * }
 *
 * COMPONENT_INIT
 * {
 *     psensor_Create("mySensor", DHUB_IO_DATA_TYPE_NUMERIC, "", PushSample, NULL);
 * }
 * @endcode
 *
 * If the above is part of an app called "myApp", then the above would appear in the Data Hub
 * resource tree as follows:
 *
 * @verbatim
/app/myApp/mySensor/value = numeric input
/app/myApp/mySensor/enable = Boolean output
/app/myApp/mySensor/period = numeric output, units = 's'
/app/myApp/mySensor/trigger = trigger output
@endverbatim
 *
 * If the app only has one sensor in it, and the app name is already the same as the name of
 * the app (e.g., if the app were called "temperature"), then the name string passed to
 * psensor_Create() can be left empty.  The following example demonstrates this and the addition
 * of a units string "degC", which is standard for degrees Celcius:
 *
 * @code
 * COMPONENT_INIT
 * {
 *     psensor_Create("", DHUB_IO_DATA_TYPE_NUMERIC, "degC", PushSample, NULL);
 * }
 * @endcode
 *
 * This would appear in the Data Hub resource tree as follows:
 *
 * @verbatim
/app/temperature/value = numeric input, units = 'degC'
/app/temperature/enable = Boolean output
/app/temperature/period = numeric output, units = 's'
/app/temperature/trigger = trigger output
@endverbatim
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 * @file periodicSensor.h
 *
 * C header file containing the external interface for the @ref c_periodicSensor.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef PERIODIC_SENSOR_H_INCLUDE_GUARD
#define PERIODIC_SENSOR_H_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of bytes in a sensor name (including null terminator).
 */
//--------------------------------------------------------------------------------------------------
#ifdef MK_CONFIG_PERIODIC_SENSOR_ABSOLUTE
#define PSENSOR_RESERVED_MAX_BYTES   7   // Reserved bytes for suffix, based on strlen("trigger")
#define PSENSOR_MAX_NAME_BYTES  (IO_MAX_RESOURCE_PATH_LEN - PSENSOR_RESERVED_MAX_BYTES)
#else
#define PSENSOR_MAX_NAME_BYTES  32
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Reference to a periodic sensor scaffold.
 */
//--------------------------------------------------------------------------------------------------
typedef struct psensor* psensor_Ref_t;


//--------------------------------------------------------------------------------------------------
/**
 * Creates a periodic sensor scaffold for a sensor with a given name.
 *
 * This makes the sensor appear in the Data Hub and creates a timer for that sensor.
 * The sampleFunc will be called whenever it's time to take a sample.  The sampleFunc should
 * call one of the psensor_PushX() functions defined in this API to push its sample to the
 * Data Hub.
 *
 * @return Reference to the new periodic sensor scaffold.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED psensor_Ref_t psensor_Create
(
    const char* name,   ///< Name of the periodic sensor, or "" if the app name is sufficient.
    io_DataType_t dataType,
    const char* units,
    void (*sampleFunc)(psensor_Ref_t ref,
                       void *context), ///< Sample function to be called back periodically.
    void *sampleFuncContext  ///< Context pointer to be passed to the sample function
);


//--------------------------------------------------------------------------------------------------
/**
 * Creates a periodic sensor scaffold for a sensor with a given name that produces JSON samples.
 *
 * This makes the sensor appear in the Data Hub and creates a timer for that sensor.
 * The sampleFunc will be called whenever it's time to take a sample.  The sampleFunc is supposed
 * to call psensor_PushJson() to push the JSON sample.
 *
 * @return Reference to the new periodic sensor scaffold.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED psensor_Ref_t psensor_CreateJson
(
    const char* name,   ///< Name of the periodic sensor, or "" if the app name is sufficient.
    const char* jsonExample,    ///< String containing example JSON value.
    void (*sampleFunc)(psensor_Ref_t ref,
                       void *context), ///< Sample function to be called back periodically.
    void *sampleFuncContext  ///< Context pointer to be passed to the sample function
);


//--------------------------------------------------------------------------------------------------
/**
 * Removes a periodic sensor scaffold and all associated resources
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void psensor_Destroy
(
    psensor_Ref_t *ref
);


//--------------------------------------------------------------------------------------------------
/**
 * Push a boolean sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void psensor_PushBoolean
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    bool value
);


//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void psensor_PushNumeric
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    double value
);


//--------------------------------------------------------------------------------------------------
/**
 * Push a string sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void psensor_PushString
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    const char* value
);


//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void psensor_PushJson
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    const char* value
);


#endif // PERIODIC_SENSOR_H_INCLUDE_GUARD
