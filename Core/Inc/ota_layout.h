#ifndef OTA_LAYOUT_H
#define OTA_LAYOUT_H

/* Flash layout shared by the bootloader and the application.
 * Changing this requires reflashing the bootloader. */
#define OTA_META_ADDR    0x08008000U   /* sector 2, 16 KB */
#define OTA_META_SECTOR  2U
#define OTA_APP_ADDR     0x0800C000U   /* sector 3, 464 KB */
#define OTA_APP_SIZE     0x00074000U

/* metadata record states */
#define OTA_META_MAGIC   0x4F544131U   /* "OTA1": image downloaded and verified */
#define OTA_UPD_MAGIC    0x55504421U   /* "UPD!": update requested, bootloader must download */

#endif
