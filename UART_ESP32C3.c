#include "UART_ESP32C3.h"

void UART1_Init(void) {
    SYSCTL_RCGCUART_R |= 0x02;             // Activa el reloj UART1
    SYSCTL_RCGCGPIO_R |= 0x02;             // Activa el reloj para el puerto B
    UART1_CTL_R &= ~0x01;                  // Desactiva UART1 para configuraci�n
    UART1_IBRD_R = 104;                    // Configuraci�n para 9600 baudios (asumiendo 16 MHz)
    UART1_FBRD_R = 11;
    UART1_LCRH_R = 0x70;                   // 8 bits, sin paridad, 1 bit de parada
    UART1_CTL_R |= 0x301;                  // Activa UART1, RX y TX
    GPIO_PORTB_AFSEL_R |= 0x03;            // Habilita funci�n alterna en PB1 y PB0
    GPIO_PORTB_DEN_R |= 0x03;              // Habilita I/O digital en PB1 y PB0
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xFFFFFF00) + 0x00000011; // Configura PB0 y PB1 como UART
}

void UART1_WriteChar(char data) {
    while((UART1_FR_R & 0x20) != 0);       // Espera hasta que el buffer est� vac�o
    UART1_DR_R = data;
}

void UART1_Transmite_Cadena(char Arreglo[]) {    // Cambiar a char[]
    uint8_t i = 0;
    while (Arreglo[i] != '\0') {           // Env�a todos los caracteres hasta el fin de cadena
        UART1_WriteChar(Arreglo[i++]);
    }
}

void IntToCharArray(int num, char* str) {
    int i = 0;
    char tempStr[10];  // Array temporal para almacenar los d�gitos en orden inverso

    // Extraer los d�gitos del n�mero y almacenarlos en orden inverso
    do {
        tempStr[i++] = (num % 10) + '0';   // Obtiene el d�gito menos significativo y lo convierte a car�cter
        num /= 10;                         // Elimina el d�gito menos significativo
    } while (num > 0);

    // Invertir el array temporal en `str` para obtener los d�gitos en el orden correcto
    for (int j = 0; j < i; j++) {
        str[j] = tempStr[i - j - 1];
    }

    // A�adir el terminador nulo para indicar el fin de la cadena
    str[i] = '\0';
}
