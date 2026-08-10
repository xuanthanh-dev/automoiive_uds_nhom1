/**
 * @file    uart_log.h
 * @brief   Tien ich in chuoi ra UART.
 *
 * @note    Tach rieng khoi CAN_if.h vi day KHONG phai chuc nang CAN.
 *          Ham uartlog duoc dinh nghia trong main.c (do CubeMX sinh huart1).
 */

#ifndef UART_LOG_H
#define UART_LOG_H

/**
 * @brief  Gui mot chuoi ket thuc bang ky tu rong ra UART.
 * @param  message  Chuoi can gui.
 */
void uartlog(char *message);

#endif /* UART_LOG_H */
