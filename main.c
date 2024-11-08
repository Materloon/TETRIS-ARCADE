/********************************************************************************************
 * tetris_v4: Se agrega la caida de bloques y la reaccion de colision, se agrega el sonido y los switches
 * PERIFERICOS:
     * Nokia 5110 (PORTA)
     * Switches (PC7 - PC4)
         * SW1 (0x80): Azul, boton para mover a la izquierda
         * SW2 (0x40): Negro, boton para mover abajo
         * SW3 (0x20): Rojo, boton para mover a la derecha
         * SW4 (0x10): Negro, boton para rotar
     * Buzzer (PB4)
 * NOTAS: Esta version posee las siguientes funciones:
     * addNewPiece: agrega una pieza al azar al tablero, en el centro superior
     * DrawPixelAsBlock: aumenta la resolucion de un pixel a 4x4
     * DrawBoard: dibuja la board en la Nokia, para ello usa DrawPixelAsBlock para aumentar la resolucion por pixel
     *
 * PENDIENTES:
     * Corregir NOTES.h para que la musica_tempo este de acuerdo al tiempo de la cancion original
********************************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include "tm4c123gh6pm.h"
#include "Nokia5110.h"
#include "pieces.h"
#include "NOTES.h"
#include "buzzer.h"

#define f_clock 16000000 // 16MHz
#define DC_inicial 50    // 50% Duty Cycle inicial

// Definiciones de constantes
#define SCREEN_WIDTH 48
#define SCREEN_HEIGHT 84
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20

// Tablero de juego
int board[BOARD_HEIGHT][BOARD_WIDTH] = {0};

// Posici�n y estado de la pieza actual
typedef struct {
    int x;
    int y;
    int type;
    int rotation;
} TetrisPiece;

TetrisPiece currentPiece;

// Funci�n para inicializar el microcontrolador y la pantalla
void InitSystem(void) {
    Nokia5110_Init();
    Nokia5110_Clear();
    Nokia5110_SetCursor(0, 0);
}

void Timer0A_Init(unsigned long period) {
    SYSCTL_RCGCTIMER_R |= 0x01;          // Habilita el reloj para Timer0
    TIMER0_CTL_R = 0x00000000;           // Deshabilita Timer0A durante la configuraci�n
    TIMER0_CFG_R = 0x00000000;           // Configuraci�n de 32 bits
    TIMER0_TAMR_R = 0x00000002;          // Modo peri�dico
    TIMER0_TAILR_R = period - 1;         // Recarga el valor
    TIMER0_IMR_R = 0x00000001;           // Habilita interrupci�n de timeout
    TIMER0_ICR_R = 0x00000001;           // Borra el flag de interrupci�n
    TIMER0_CTL_R |= 0x00000001;          // Habilita Timer0A
    NVIC_EN0_R |= 1 << 19;               // Habilita la interrupci�n en NVIC
}


// Funci�n para agregar una pieza al tablero
void addNewPiece(void) {
    currentPiece.x = 0; // Posici�n inicial en el centro
    currentPiece.y = 5; // Parte superior del tablero
    currentPiece.type = rand() % 7; // Tipo de pieza aleatorio (0 a 6)
    currentPiece.rotation = 0; // Rotaci�n inicial

    // Agrega la pieza al tablero
    int col,row;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            if (pieces[currentPiece.type][currentPiece.rotation][row][col] == 1) {
                int boardX = currentPiece.x + col;
                int boardY = currentPiece.y + row;
                if (boardX >= 0 && boardX < BOARD_WIDTH && boardY >= 0 && boardY < BOARD_HEIGHT) {
                    board[boardY][boardX] = 1;
                }
            }
        }
    }
}

// Funci�n para eliminar una pieza del tablero
void removePiece(void) {
    int col, row;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            // Si la celda de la pieza actual est� ocupada (valor 1), la elimina del tablero
            if (pieces[currentPiece.type][currentPiece.rotation][row][col] == 1) {
                int boardX = currentPiece.x + col;
                int boardY = currentPiece.y + row;
                // Asegura que las coordenadas est�n dentro de los l�mites del tablero
                if (boardX >= 0 && boardX < BOARD_WIDTH && boardY >= 0 && boardY < BOARD_HEIGHT) {
                    board[boardY][boardX] = 0;  // Borra la celda en el tablero
                }
            }
        }
    }
}

//Funci�n que a�ade una pieza al tablero
void addPieceToBoard(void) {
    int col, row;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            if (pieces[currentPiece.type][currentPiece.rotation][row][col] == 1) {
                int boardX = currentPiece.x + col;
                int boardY = currentPiece.y + row;
                if (boardX >= 0 && boardX < BOARD_WIDTH && boardY >= 0 && boardY < BOARD_HEIGHT) {
                    board[boardY][boardX] = 1; // Coloca la pieza en el tablero
                }
            }
        }
    }
}

//Funci�n que verifica colisiones

int verifyCollisions(TetrisPiece piece) {
    int col, row;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            if (pieces[piece.type][piece.rotation][row][col] == 1) {
                int boardX = piece.x + col;
                int boardY = piece.y + row;
                if (boardX < 0 || boardX >= BOARD_WIDTH || boardY >= BOARD_HEIGHT || board[boardY][boardX] == 1) {
                    return 0; // Posici�n inv�lida
                }
            }
        }
    }
    return 1; // Posici�n v�lida
}


// Funci�n para dibujar un p�xel de 4x4 en la pantalla
void DrawPixelAsBlock(int x, int y) {
    int offsetY, offsetX;
    for (offsetY = 0; offsetY < 4; offsetY++) {
        for (offsetX = 0; offsetX < 4; offsetX++) {
            if ((x + offsetX) < SCREEN_HEIGHT && (y + offsetY) < SCREEN_WIDTH) {
                setPixel(x + offsetX, y + offsetY);
            }
        }
    }
}

// Funci�n para dibujar el tablero en la pantalla
void DrawBoard(void) {
    Nokia5110_ClearBuffer();
    int x,y;
    // Dibuja el tablero
    for (y = 0; y < BOARD_HEIGHT; y++) {
        for (x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] == 1) {
                DrawPixelAsBlock(x * 4, y * 4); // Dibuja cada p�xel como un bloque de 4x4
            }
        }
    }
    Nokia5110_DisplayBuffer();
}

void Timer0A_Handler(void) {
    TIMER0_ICR_R = 0x00000001; // Borra el flag de interrupci�n

    removePiece();     // Elimina la pieza actual del tablero

    currentPiece.y++;  // Mueve la pieza hacia abajo

    // Verifica si la nueva posici�n es v�lida
    if (!verifyCollisions(currentPiece)) {
        currentPiece.y--;  // Si no es v�lida, restaura la posici�n anterior
        addPieceToBoard(); // Coloca la pieza permanentemente en el tablero
        addNewPiece();     // Genera una nueva pieza en la parte superior
    } else {
        addPieceToBoard(); // Si es v�lida, coloca la pieza en la nueva posici�n
    }

    DrawBoard(); // Actualiza la pantalla para mostrar el nuevo estado del tablero
}


// Funci�n principal
int main(void) {
    uint32_t ValDC = DC_inicial; // DC inicial de 50%
    int *pMelody = musica;
    int *pTempo = musica_tempo;
    int size = sizeof(musica) / sizeof(int);
    int playing = 0; // Estado inicial: m�sica apagada

    InitSystem();

    Config_Puerto_B();
    Config_Puerto_C();
    Config_PWM();
    Apaga_PWM(); // Asegura que el buzzer est� apagado inicialmente

    // Inicializa el generador de n�meros aleatorios
    srand(12345); // Puedes usar una semilla diferente para resultados variados

    // Agrega una nueva pieza al tablero
    addNewPiece();
    DrawBoard();

    // Muestra el tablero en la pantalla
    while (1) {
        // Enciende la m�sica con SW1 (PC7) y cambia a encendido
        if ((GPIO_PORTC_DATA_R & 0x80) == 0 && !playing) { // SW1 presionado y m�sica apagada
            Delay_ms(200); // Retardo antirrebote
            playing = 1; // Cambia el estado a "reproduciendo"
        }

        // Apaga la m�sica con SW2 (PC6) y cambia a apagado
        if ((GPIO_PORTC_DATA_R & 0x40) == 0 && playing) { // SW2 presionado y m�sica encendida
            Delay_ms(200); // Retardo antirrebote
            playing = 0; // Cambia el estado a "apagado"
            buzz(0, ValDC); // Apaga el buzzer
        }

        // Reproduce la m�sica continuamente si est� encendida
        if (playing) {
            playMelody(pMelody, pTempo, size, ValDC);
        }
    }
}
