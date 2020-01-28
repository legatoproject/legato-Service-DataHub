//--------------------------------------------------------------------------------------------------
/**
 * Periodic sensor scaffold.  Used to implement generic periodic sensors.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"


//--------------------------------------------------------------------------------------------------
/**
 * Sensor Scaffold object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct psensor
{
    bool isEnabled;
    double period;  ///< seconds (0.0 = not set yet)
    le_timer_Ref_t timer;
    void (*sampleFunc)(psensor_Ref_t, void *);
    void *sampleFuncContext;
    char name[PSENSOR_MAX_NAME_BYTES];

    dhubIO_TriggerPushHandlerRef_t triggerHandlerRef;
    dhubIO_NumericPushHandlerRef_t periodHandlerRef;
    dhubIO_BooleanPushHandlerRef_t enableHandlerRef;
}
Sensor_t;

/// Default number of psensors.  This may be overridden in the .cdef
#define DEFAULT_POOL_SIZE 10

//--------------------------------------------------------------------------------------------------
/**
 * Pool from which Sensor_t objects are allocated.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t SensorPool = NULL;
LE_MEM_DEFINE_STATIC_POOL(SensorPool, DEFAULT_POOL_SIZE, sizeof(Sensor_t));


//--------------------------------------------------------------------------------------------------
/**
 * Timer expiry handler function.
 */
//--------------------------------------------------------------------------------------------------
static void HandleTimerExpiry
(
    le_timer_Ref_t timer
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = le_timer_GetContextPtr(timer);

    sensorPtr->sampleFunc(sensorPtr, sensorPtr->sampleFuncContext);
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle an "enable" update from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandleEnablePush
(
    double timestamp,   ///< Don't care about this.
    bool enable,
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = contextPtr;

    LE_UNUSED(timestamp);

    if (sensorPtr->isEnabled != enable)
    {
        sensorPtr->isEnabled = enable;

        if (enable)
        {
            // If the period has been set, take a sample and start the timer.
            if (sensorPtr->period > 0.0)
            {
                sensorPtr->sampleFunc(sensorPtr, sensorPtr->sampleFuncContext);
                le_timer_Start(sensorPtr->timer);
            }
        }
        else
        {
            le_timer_Stop(sensorPtr->timer);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a "period" update from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandlePeriodPush
(
    double timestamp,   ///< Don't care about this.
    double period,      ///< seconds
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = contextPtr;

    LE_UNUSED(timestamp);

    // If the new value is the same as the old value, ignore the push.
    if (sensorPtr->period != period)
    {
        // Sanity check the period.
        // If it's invalid, stop the timer and set the period to 0.0.
        if ((period <= 0.0) || isnan(period))
        {
            LE_ERROR("Timer period %lf is out of range. Must be > 0.", period);
            le_timer_Stop(sensorPtr->timer);
            sensorPtr->period = 0.0;
        }
        else if (period > (double)(0x7FFFFFFF)) // Don't know how big time_t is, assume 32-bits.
        {
            LE_ERROR("Timer period %lf is too high.", period);
            le_timer_Stop(sensorPtr->timer);
            sensorPtr->period = 0.0;
        }
        else
        {
            // The new period is good.
            // Set the timer's interval to the period.
            le_clk_Time_t interval;
            interval.sec = (time_t)period;
            interval.usec = (period - interval.sec) * 1000000;
            le_timer_SetInterval(sensorPtr->timer, interval);

            // If the old value was zero and the sensor is enabled, take a sample and
            // start the timer now.
            if ((sensorPtr->period == 0) && sensorPtr->isEnabled)
            {
                sensorPtr->sampleFunc(sensorPtr, sensorPtr->sampleFuncContext);
                le_timer_Start(sensorPtr->timer);
            }

            sensorPtr->period = period;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a "trigger" update from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandleTriggerPush
(
    double timestamp,   ///< Don't care about this.
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = contextPtr;

    LE_UNUSED(timestamp);

    if (sensorPtr->isEnabled)
    {
        sensorPtr->sampleFunc(sensorPtr, sensorPtr->sampleFuncContext);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Build up the path to a resource from the sensor name and the resource (leaf) name.
 */
//--------------------------------------------------------------------------------------------------
static void BuildResourcePath
(
    char* pathBuffPtr,  ///< Ptr to the buffer into which the path will be built.
    size_t pathBuffSize,    ///< Size (in bytes) of the buffer pointed to by pathBuffPtr.
    Sensor_t* sensorPtr, ///< Ptr to the Sensor scaffold object.
    const char* resourceName  ///< E.g., "value" or "enable"
)
//--------------------------------------------------------------------------------------------------
{
    if (sensorPtr->name[0] == '\0')
    {
        LE_ASSERT(LE_OK == le_utf8_Copy(pathBuffPtr, resourceName, pathBuffSize, NULL));
    }
    else
    {
        int result = snprintf(pathBuffPtr, pathBuffSize, "%s/%s", sensorPtr->name, resourceName);
        LE_ASSERT(result > 0);
        LE_ASSERT((size_t) result < pathBuffSize);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Create an output resource.
 */
//--------------------------------------------------------------------------------------------------
static void CreateOutput
(
    const char* path,   ///< Resource path at which to create the output.
    dhubIO_DataType_t dataType, ///< Data type of the resource.
    const char* units   ///< Units string of the resource.
)
//--------------------------------------------------------------------------------------------------
{
    le_result_t result = dhubIO_CreateOutput(path, dataType, units);
    if (result == LE_DUPLICATE)
    {
        LE_WARN("An output already existed in the Data Hub at path '%s'.", path);
    }
    else if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub output '%s' (%s).", path, LE_RESULT_TXT(result));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Creates a periodic sensor scaffold for a sensor with a given name.
 *
 * This makes the sensor appear in the Data Hub and creates a timer for that sensor.
 * The sampleFunc will be called whenever it's time to take a sample.  The sampleFunc must
 * call one of the psensor_PushX() functions below.
 *
 * @return Reference to the new periodic sensor scaffold.
 */
//--------------------------------------------------------------------------------------------------
psensor_Ref_t psensor_Create
(
    const char* name,   ///< Name of the periodic sensor, or "" if the app name is sufficient.
    io_DataType_t dataType,
    const char* units,
    void (*sampleFunc)(psensor_Ref_t ref,
                       void *context), ///< Sample function to be called back periodically.
    void *sampleFuncContext  ///< Context pointer to be passed to the sample function
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = le_mem_Alloc(SensorPool);

    sensorPtr->isEnabled = false;
    sensorPtr->period = 0.0;

    sensorPtr->timer = le_timer_Create(name);
    le_timer_SetRepeat(sensorPtr->timer, 0); // Repeat an infinite number of times.
    le_timer_SetHandler(sensorPtr->timer, HandleTimerExpiry);
    le_timer_SetContextPtr(sensorPtr->timer, sensorPtr);

    sensorPtr->sampleFunc = sampleFunc;
    sensorPtr->sampleFuncContext = sampleFuncContext;

    if (le_utf8_Copy(sensorPtr->name, name, sizeof(sensorPtr->name), NULL) != LE_OK)
    {
        LE_FATAL("Sensor name too long (%s)", name);
    }

    // Create the Data Hub resources "value", "enable", "period", and "trigger" for this sensor.
    char path[IO_MAX_RESOURCE_PATH_LEN];
    BuildResourcePath(path, sizeof(path), sensorPtr, "value");
    le_result_t result = dhubIO_CreateInput(path, dataType, units);
    if (result == LE_DUPLICATE)
    {
        LE_WARN("An input already existed in the Data Hub at path '%s'.", path);
    }
    else if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub input '%s' (%s).", path, LE_RESULT_TXT(result));
    }

    BuildResourcePath(path, sizeof(path), sensorPtr, "enable");
    CreateOutput(path, IO_DATA_TYPE_BOOLEAN, "");
    sensorPtr->enableHandlerRef = dhubIO_AddBooleanPushHandler(path, HandleEnablePush, sensorPtr);


    BuildResourcePath(path, sizeof(path), sensorPtr, "period");
    CreateOutput(path, IO_DATA_TYPE_NUMERIC, "s");
    sensorPtr->periodHandlerRef = dhubIO_AddNumericPushHandler(path, HandlePeriodPush, sensorPtr);

    BuildResourcePath(path, sizeof(path), sensorPtr, "trigger");
    CreateOutput(path, IO_DATA_TYPE_TRIGGER, "");
    sensorPtr->triggerHandlerRef = dhubIO_AddTriggerPushHandler(path, HandleTriggerPush, sensorPtr);
    dhubIO_MarkOptional(path);

    return sensorPtr;
}


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
psensor_Ref_t psensor_CreateJson
(
    const char* name,   ///< Name of the periodic sensor, or "" if the app name is sufficient.
    const char* jsonExample,    ///< String containing example JSON value.
    void (*sampleFunc)(psensor_Ref_t ref,
                       void *context), ///< Sample function to be called back periodically.
    void *sampleFuncContext  ///< Context pointer to be passed to the sample function
)
//--------------------------------------------------------------------------------------------------
{
    psensor_Ref_t ref = psensor_Create(name,
                                       IO_DATA_TYPE_JSON,
                                       "", // units
                                       sampleFunc,
                                       sampleFuncContext);

    char path[IO_MAX_RESOURCE_PATH_LEN];
    BuildResourcePath(path, sizeof(path), ref, "value");
    dhubIO_SetJsonExample(path, jsonExample);

    return ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Removes a periodic sensor scaffold and all associated resources
 */
//--------------------------------------------------------------------------------------------------
void psensor_Destroy
(
    psensor_Ref_t *ref
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr;
    char path[IO_MAX_RESOURCE_PATH_LEN];

    LE_ASSERT(NULL != ref);

    sensorPtr = *ref;
    *ref = NULL;

    if (sensorPtr)
    {
        sensorPtr->isEnabled = false;

        // Stop and delete timer
        (void)le_timer_Stop (sensorPtr->timer);
        le_timer_Delete(sensorPtr->timer);

        // Deregister handlers and remove resources
        BuildResourcePath(path, sizeof(path), sensorPtr, "trigger");
        dhubIO_RemoveTriggerPushHandler(sensorPtr->triggerHandlerRef);
        dhubIO_DeleteResource(path);

        BuildResourcePath(path, sizeof(path), sensorPtr, "period");
        dhubIO_RemoveNumericPushHandler(sensorPtr->periodHandlerRef);
        dhubIO_DeleteResource(path);

        BuildResourcePath(path, sizeof(path), sensorPtr, "enable");
        dhubIO_RemoveBooleanPushHandler(sensorPtr->enableHandlerRef);
        dhubIO_DeleteResource(path);

        BuildResourcePath(path, sizeof(path), sensorPtr, "value");
        dhubIO_DeleteResource(path);

        le_mem_Release(sensorPtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a boolean sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushBoolean
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    bool value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = ref;

    char path[IO_MAX_RESOURCE_PATH_LEN];
    LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < (int) sizeof(path));

    dhubIO_PushBoolean(path, timestamp, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushNumeric
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    double value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = ref;

    char path[IO_MAX_RESOURCE_PATH_LEN];
    LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < (int) sizeof(path));

    dhubIO_PushNumeric(path, timestamp, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a string sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushString
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    const char* value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = ref;

    char path[IO_MAX_RESOURCE_PATH_LEN];
    LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < (int) sizeof(path));

    dhubIO_PushString(path, timestamp, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushJson
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    const char* value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = ref;

    char path[IO_MAX_RESOURCE_PATH_LEN];
    LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < (int) sizeof(path));

    dhubIO_PushJson(path, timestamp, value);
}


COMPONENT_INIT
{
    SensorPool = le_mem_InitStaticPool(SensorPool, DEFAULT_POOL_SIZE, sizeof(Sensor_t));
}
