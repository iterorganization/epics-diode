FROM co3-full-devel:7.2.0 AS epics-diode-img

COPY epics-diode /epics-diode
COPY utils/Makefile /epics-diode/test
COPY utils/buildme.sh /epics-diode
COPY utils/RELEASE /epics-diode/configure

USER root
WORKDIR /epics-diode

ENTRYPOINT ["/epics-diode/buildme.sh"]







