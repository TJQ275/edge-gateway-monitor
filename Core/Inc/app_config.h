#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Sensor table - this is the only place to edit when the hardware changes.
 *
 * addr : Modbus slave address
 * func : function code (0x03 = read holding registers)
 * reg  : first register
 * num  : number of registers to read
 * s1   : scale of the first value  (raw / s1)
 * s2   : scale of the second value (raw / s2)
 * kind : 0 = two 16-bit values, 1 = one 32-bit value in two registers
 *
 * Fields after "num" are: err_cnt, val1, val2, s1, s2, kind.
 */
#define APP_DEV_LIST { \
    {0x01U, 0x03U, 0x0001U, 2U, 0U, 0.0f, 0.0f, 10.0f,   10.0f, 0U}, \
    {0x02U, 0x03U, 0x0002U, 2U, 0U, 0.0f, 0.0f, 1000.0f, 1.0f,  1U}, \
}

/* Position in the table, used when publishing values */
#define APP_IDX_TEMP_HUMI   0U
#define APP_IDX_LIGHT       1U

#endif
