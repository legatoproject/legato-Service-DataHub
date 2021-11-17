
TARGET ?= localhost

DBG :=
ifeq ($(D),1)
    DBG := -d $(LEGATO_ROOT)/build/$(TARGET)/debug
endif

OCTAVE :=
ifeq ($(O),1)
    OCTAVE := -C -DWITH_OCTAVE -C -Icomponents/octaveFormatter
endif

DATAHUB_INTF_INCLUDES := -i $(CURDIR)/interfaces -i $(CURDIR)/interfaces/linux

.PHONY: all dataHub appInfoStub sensor actuator snapshot
all: dataHub appInfoStub sensor actuator snapshot configTest

dataHub:
	mkapp -t $(TARGET) dataHub.adef -i $(LEGATO_ROOT)/interfaces/supervisor $(DATAHUB_INTF_INCLUDES) $(DBG) ${OCTAVE}

appInfoStub:
	mkapp -t $(TARGET) test/appInfoStub.adef -i $(LEGATO_ROOT)/interfaces/supervisor $(DBG)

sensor:
	mkapp -t $(TARGET) test/sensor.adef -s components $(DATAHUB_INTF_INCLUDES) $(DBG)

actuator:
	mkapp -t $(TARGET) test/actuator.adef $(DATAHUB_INTF_INCLUDES) $(DBG)

snapshot:
	mkapp -t $(TARGET) test/snapshot.adef $(DATAHUB_INTF_INCLUDES) $(DBG) ${OCTAVE}

configTest:
	mkapp -t $(TARGET) test/configTest.adef $(DATAHUB_INTF_INCLUDES) $(DBG)

.PHONY: clean
clean:
	rm -rf _build* *.update docs backup

DHUB = _build_dataHub/$(TARGET)/app/dataHub/staging/read-only/bin/dhub

.PHONY: start stop test
start: stop all
	# LE_LOG_LEVEL=DEBUG startlegato
	startlegato
	sdir bind "<$(USER)>.le_appInfo" "<$(USER)>.le_appInfo"
	sdir bind "<$(USER)>.sensord.sensor.io" "<$(USER)>.io"
	sdir bind "<$(USER)>.sensord.periodicSensor.dhubIO" "<$(USER)>.io"
	sdir bind "<$(USER)>.actuatord.actuator.io" "<$(USER)>.io"
	sdir bind "<$(USER)>.actuatord.actuator.admin" "<$(USER)>.admin"
	sdir bind "<$(USER)>.dhubToolAdmin" "<$(USER)>.admin"
	sdir bind "<$(USER)>.dhubToolIo" "<$(USER)>.io"
	sdir bind "<$(USER)>.dhubToolQuery" "<$(USER)>.query"
	sdir bind "<$(USER)>.dsnap.snapshot.query" "<$(USER)>.query"
	test/supervisor
	$(DHUB) set backupPeriod temp 5
	$(DHUB) set bufferSize temp 100
	$(DHUB) set default /app/sensor/counter/period 0.25
	$(DHUB) set source /app/sensor/temperature/trigger /app/sensor/counter/value
	$(DHUB) set source /obs/temp /app/sensor/temperature/value
	$(DHUB) set default /app/sensor/counter/enable true

stop:
	stoplegato


test: stop all
	LE_LOG_LEVEL=DEBUG startlegato
	sdir bind "<$(USER)>.le_appInfo" "<$(USER)>.le_appInfo"
	sdir bind "<$(USER)>.configTestD.configTest.config" "<$(USER)>.config"
	sdir bind "<$(USER)>.configTestD.configTest.io" "<$(USER)>.io"
	sdir bind "<$(USER)>.configTestD.configTest.admin" "<$(USER)>.admin"
	test/configTest/run_test.sh $(TEST_ARGS)


IFGEN_FLAGS = --gen-interface \
              --gen-common-interface \
              --import-dir interfaces/linux \
              --import-dir $(LEGATO_ROOT)/interfaces \
              --output-dir _build_docs

.PHONY: docs
docs:
	mkdir -p _build_docs
	ifgen $(IFGEN_FLAGS) interfaces/admin.api
	ifgen $(IFGEN_FLAGS) interfaces/io.api
	ifgen $(IFGEN_FLAGS) interfaces/query.api
	ifgen $(IFGEN_FLAGS) interfaces/config.api
	doxygen
