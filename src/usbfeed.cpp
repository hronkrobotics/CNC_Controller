#include "usbfeed.h"
#include "config.h"
#include "grbl.h"

#if ENABLE_USB_FEED

static String s_line;

void usb_feed_init()
{
    USB_FEED_PORT.begin(115200);
    s_line.reserve(96);
}

void usb_feed_loop()
{
    // Backpressure: stop draining the USB port while GRBL's outbound queue is
    // deep. The USB CDC FIFO then holds the sender off until GRBL catches up.
    while (USB_FEED_PORT.available() && Grbl.queueDepth() < 8) {
        uint8_t b = (uint8_t)USB_FEED_PORT.read();

        // Real-time commands act immediately and never enter the line buffer.
        switch (b) {
            case '?':  Grbl.requestStatus(); continue;
            case '!':  Grbl.feedHold();      continue;
            case '~':  Grbl.resume();        continue;
            case 0x18: Grbl.softReset();     continue;   // Ctrl-X
            case 0x85: Grbl.jogCancel();     continue;
        }

        if (b == '\n' || b == '\r') {
            if (s_line.length()) {
                Grbl.sendLine(s_line);
                s_line = "";
            }
        } else {
            s_line += (char)b;
            if (s_line.length() > 90) s_line = "";       // runaway guard
        }
    }
}

void usb_feed_out(const char *line, bool outgoing)
{
    // Echo GRBL's replies to the PC. We forward what GRBL says (ok/error/[MSG]/
    // status) so a line sender gets acknowledgements; our own outgoing echoes
    // are suppressed to avoid confusing the sender.
    if (outgoing) return;
    USB_FEED_PORT.println(line);
}

#else

void usb_feed_init() {}
void usb_feed_loop() {}
void usb_feed_out(const char *, bool) {}

#endif
