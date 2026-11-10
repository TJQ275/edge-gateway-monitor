#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

/* Modbus RTU slave on the second RS485 port (USART6).
 * PA11 = TX, PA12 = RX, PB8 = DE/RE, 9600 8N1.
 * LCD_RST was moved from PA11 to PB5. */

#define MB_SLAVE_ADDR   0x0A

void ModbusSlave_Task(void *arg);

#endif
