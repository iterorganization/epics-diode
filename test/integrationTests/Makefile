TOP = ../..

include $(TOP)/configure/CONFIG

BIN = ../bin

build:
	@if [ "$(EPICS_DIODE_INTEGRATION_TESTS)" = "ON" ]; then \
		$(MAKE) docker_image; \
	else \
		echo "Integration tests disabled. Set EPICS_DIODE_INTEGRATION_TESTS=ON to enable."; \
	fi

docker_image:
	@if command -v docker >/dev/null 2>&1; then \
		echo "Building docker image for epics-diode"; \
		exec $(BIN)/build_image.sh; \
	else \
		echo "Docker not found, skipping image build."; \
	fi

clean:
	rm -rf images/epics-diode/epics-diode/*

realclean: clean

include $(TOP)/configure/RULES

