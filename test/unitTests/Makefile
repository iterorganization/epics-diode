TOP = ../..

include $(TOP)/configure/CONFIG

TESTPROD += test_diode
test_diode_SRCS += test_diode.cpp
test_diode_LIBS = Com epics-diode

TESTPROD += test_sender_receiver
test_sender_receiver_SRCS += test_sender_receiver.cpp test_socket_utils.cpp test_sequence_tracker.cpp
test_sender_receiver_LIBS = Com epics-diode

include $(TOP)/configure/RULES

