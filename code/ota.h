#ifndef OTA_H
#define OTA_H

/* Write the "update requested" marker and reset; the bootloader then waits for XMODEM. */
void OTA_Trigger(void);

#endif
