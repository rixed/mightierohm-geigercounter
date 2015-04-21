top_srcdir = .
all: event_test.hex led_test.hex bubble_test.hex bubble_serial_test.hex

CPPFLAGS = -DBUBBLE_LED_VIA_SHIFT_REGISTER=1 -DBB_PORT_SHIFT_REG=B

include $(top_srcdir)/make.common

clean-spec:

.PHONY: cscope
cscope:
	cscope -Rb -I/usr/avr/include -I/usr/lib/avr/include

-include .depend
