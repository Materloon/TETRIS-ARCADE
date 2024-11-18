/********************************************************************************************
 * tetris_v5: Se agrega la caida de bloques y la reaccion de colision, game start, game over y la subida de datos a GS.
 * PERIFERICOS:
     * Nokia 5110 (PORTA)
         * RST : PA7
         * CE : PA3
         * DC: PA6
         * DIN: PA5
         * SCLK: PA2
     * Switches (PC7 - PC4)
         * SW1 (0x80): Azul, boton para mover a la izquierda
         * SW2 (0x40): Negro, boton para mover abajo
         * SW3 (0x20): Rojo, boton para mover a la derecha
         * SW4 (0x10): Negro, boton para rotar
     * Buzzer (PB4)
     * ESP32 C3:
         * GPIO 3 : PB0 (RX)
         * GPIO 1 : PB1 (TX)
 * NOTAS:
     *
 * PENDIENTES:
     * Agregar un teclado (lib) para el USER del jugador.
     * Corregir la musica
********************************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tm4c123gh6pm.h"
#include "Nokia5110.h"
#include "pieces.h"
#include "melodia.h"
#include "BuzzerLib.h"
#include "UART_ESP32C3.h"

#define f_clock 16000000 // 16MHz
#define DC_inicial 50    // 50% Duty Cycle inicial

// Definiciones de constantes
#define SCREEN_WIDTH 48
#define SCREEN_HEIGHT 84
#define BOARD_WIDTH 21
#define BOARD_HEIGHT 10

// Definiciones de los switches

#define SW1 0x80
#define SW2 0x40
#define SW3 0x20
#define SW4 0x10

// Tablero de juego
int board[BOARD_HEIGHT][BOARD_WIDTH] = {0};
// Pantalla del Nokia5110
//int screen[SCREEN_HEIGHT][SCREEN_WIDTH] = {0};
//Contador Tiempo
uint16_t contadortiempo = 0;
uint32_t guardartiempo;
uint8_t w=0;
// Posici�n y estado de la pieza actual
typedef struct {
    int x;
    int y;
    int type;
    int rotation;
} TetrisPiece;

TetrisPiece currentPiece;

//**************************************************************************************************************
//******************* Configurando el Puerto B pin 4 para funciones especiales *********************************
//**************************************************************************************************************
void Config_Puerto_B(void) {
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB; // Habilitamos Puerto B (PB)
    while(!(SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R1)); // Esperamos a que se active
    GPIO_PORTB_AMSEL_R &= ~(0X10); // Desactivando funciones analÃ³gicas para PB4
    GPIO_PORTB_AFSEL_R |= 0X10;    // Seleccionando PB4 para funciones especiales
    GPIO_PORTB_DIR_R |= 0X10;      // PB4 como salida
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0XFFF0FFFF) | 0X70000; // PB4 como T1CCP0
    GPIO_PORTB_DEN_R |= 0X10;      // Habilitando PB4
}

void Config_Puerto_C(void) {
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R2; // Activa el puerto C
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R2) == 0); // Espera a que se active
    GPIO_PORTC_DIR_R &= ~0xF0; // Configura PC7, PC6, PC5, PC4 como entrada
    GPIO_PORTC_DEN_R |= 0xF0; // Habilita PC7, PC6, PC5, PC4 como digital
    GPIO_PORTC_PUR_R |= 0xF0; // Activa el pull-up en PC7, PC6, PC5, PC4
}

// Funci�n para inicializar el microcontrolador y la pantalla
void InitSystem(void) {
    Nokia5110_Init();
    Nokia5110_Clear();
    Nokia5110_SetCursor(0, 0);
}


void ConfiguraTimer_1ms(void) {
    /*Inhabilitamos el m�dulo SysTick*/
    NVIC_ST_CTRL_R &= ~NVIC_ST_CTRL_ENABLE;
    /* TiempoDeseado=1ms; FreqCPU=16MHZ
    * valor que debe ir en el registro RELOAD=X
    * X=(TiempoDeseado)*(FreqCPU) -1
    * X= [[1*10^(-3)] * [16*10^(6)]] -1 =15999
    *15999 en hexadecimal es 00003E7F*/
    NVIC_ST_RELOAD_R = (NVIC_ST_RELOAD_R&0xFF000000)|0x00003E7F;
    // Iniciamos el contador con cero (escribiendo cualquier valor)
    NVIC_ST_CURRENT_R &= ~(0x00FFFFFF);
    // Habilitamos el m�dulo SysTick
    NVIC_ST_CTRL_R |= (NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_ENABLE);
}

// Funci�n para agregar una pieza al tablero
void addNewPiece(void) {
    currentPiece.x = 0; // Posici�n inicial en el centro
    currentPiece.y = 3; // Parte superior del tablero
    //currentPiece.type = 0; //Para probar
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
                if (boardY<0 || boardX < 0 || boardX >= BOARD_WIDTH || boardY >= BOARD_HEIGHT || (boardY >= 0 && board[boardY][boardX] == 1)) {
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

void delay(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 3180; j++) {
            // No hacer nada, simplemente retraso aproximado
        }
    }
}

int score;//////////////////////SCOOOOOOOOOREEEEEE///////////////////////////

void RemoveLine() {
    int row, col, complete,k;
    for (row = 0; row < BOARD_WIDTH; row++) {
        complete = 1;  // Asumimos que la fila est� completa
        for (col = 0; col < BOARD_HEIGHT; col++) {
            if (board[col][row] == 0) {  // Si hay una celda vac�a, la fila no est� completa
                complete = 0;
                break;
            }
        }

        if (complete) {

            // Borrar la fila que contiene a la linea completa (se convierte en una nueva fila vac�a)
            for (col = 0; col < BOARD_HEIGHT; col++) {
                board[col][row] = 0;
            }
            // Desplazar todas las filas superiores hacia abajo
            for (k = row; k > 1; k--){
                for (col = 0; col < BOARD_HEIGHT; col++) {
                    board[col][k] = board[col][k-1];
                }
            }

            score += 100;  // Sumar puntos al marcador
            //updateScoreDisplay();  // Actualizar el marcador en la pantalla
        }
    }
}


void intToStr(int num, unsigned char *str) {
    int i = 0;
    int j;
    int temp;

    // Manejar el caso cuando el n�mero es 0
    if (num == 0) {
        str[i++] = '0';
    } else {
        // Convierte el n�mero a una cadena
        while (num > 0) {
            str[i++] = (num % 10) + '0';  // Extrae el d�gito y lo convierte a car�cter
            num /= 10;
        }
    }

    str[i] = '\0';  // A�ade el terminador de cadena

    // Invierte la cadena para obtener el n�mero en el orden correcto
    for (j = 0; j < i / 2; j++) {
        temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
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
    unsigned char scoreStr[7];
    intToStr(score, scoreStr);

    text(16, 43,  (unsigned char *)"SCORE: ", 0);
    text(40, 43,  scoreStr, 0);
    Nokia5110_DisplayBuffer();
}


// Funci�n para actualizar el contador de tiempo
void actualizarContador() {
//preguntamos si paso 1ms
    if((NVIC_ST_CTRL_R & NVIC_ST_CTRL_COUNT)) {
        //cada vez que pase 1ms, el contador de tiempo se incrementa
        contadortiempo++;
    }
}

void uploadScore(void){
    char user[8] = "PRUEBA2";                      // hay que agregar luego una funcion para obtener este array con un teclado
    char score_char[10];
    IntToCharArray(score, score_char);
    UART1_Transmite_Cadena("USER:");
    UART1_Transmite_Cadena(user);
    UART1_Transmite_Cadena(";");
    UART1_Transmite_Cadena("SCORE:");
    UART1_Transmite_Cadena(score_char);      // Transmite el puntaje como cadena completa
    UART1_Transmite_Cadena(";");
}

// Funci�n principal
int main(void) {
  //  uint32_t ValDC = DC_inicial; // DC inicial de 50%
  //  int *pMelody = musica;
  //  int *pTempo = musica_tempo;
  //  int size = sizeof(musica) / sizeof(int);
  //  int playing = 0; // Estado inicial: m�sica apagada

    int row,col;

    InitSystem();

    Config_Puerto_B();
    Config_Puerto_C();
    ConfiguraTimer_1ms();
    UART1_Init();
    /*InicializaBuzzer();
    Apaga_PWM(); // Asegura que el buzzer est� apagado inicialmente
    musicaBuzzer(tetris_musica, tetris_musica_tempo, TETRIS_MELODY_SIZE);*/

    // LOOP GENERAL
    while (1) {
        int game = 0;
        clearBuffer();
        Nokia5110_DisplayBuffer();
        int hold = 1;
        while(hold){
            text(0, 15,  (unsigned char *)"PRESS R", 0);
            text(0, 22,  (unsigned char *)"TO START", 0);
            Nokia5110_DisplayBuffer();
            if((GPIO_PORTC_DATA_R & SW4) == 0){
                while(((GPIO_PORTC_DATA_R & SW4) == 0));
                game = 1;
                // Inicializa el generador de n�meros aleatorios
                clearBuffer();
                Nokia5110_DisplayBuffer();
                srand(12345); // Puedes usar una semilla diferente para resultados variados

                // Agrega una nueva pieza al tablero
                addNewPiece();
                DrawBoard();
                delay(255);
                hold = 0;
            }
        }
        while(game){ // LOOP DEL JUEGO
            //inicializa contador
            actualizarContador();
            if(contadortiempo == 255){
                removePiece();     // Elimina la pieza actual del tablero
                currentPiece.x++;  // Mueve la pieza hacia abajo

                // Verifica si la nueva posici�n es v�lida
                if (!verifyCollisions(currentPiece)) {
                    currentPiece.x--;  // Si no es v�lida, restaura la posici�n anterior

                    // ACA SE VERIFICA SI SE PIERDE EL JUEGO
                        for (col = 0; col < BOARD_HEIGHT; col++) {
                            if (board[col][0] == 1){ // si se ha alcanzado el limite superior
                                for (row = 0; row < BOARD_WIDTH; row++){
                                    for (col = 0; col < BOARD_HEIGHT; col++) {
                                        board[col][row] = 0; // SE VACIA EL BOARD
                                    }
                                }
                                game = 0;//se apaga el juego
                                DrawBoard();
                                hold = 1;
                                while(hold){
                                    text(0, 15,  (unsigned char *)"SAVE SCORE?", 0);
                                    text(0, 22,  (unsigned char *)"<= YES    => NO", 0);
                                    Nokia5110_DisplayBuffer();
                                    if((GPIO_PORTC_DATA_R & SW1) == 0){
                                        while((GPIO_PORTC_DATA_R & SW1) == 0);
                                        clearBuffer();
                                        Nokia5110_DisplayBuffer();
                                        uploadScore();
                                        text(0, 15,  (unsigned char *)"SCORE SAVED", 0);
                                        Nokia5110_DisplayBuffer();
                                        for(int n = 0; n < 3600000; n++); //retardo
                                        hold = 0;
                                    }
                                    else if((GPIO_PORTC_DATA_R & SW3) == 0){
                                        while((GPIO_PORTC_DATA_R & SW3) == 0);
                                        clearBuffer();
                                        Nokia5110_DisplayBuffer();
                                        text(0, 15,  (unsigned char *)"SCORE NOT SAVED", 0);
                                        Nokia5110_DisplayBuffer();
                                        for(int n = 0; n < 3600000; n++); //retardo
                                        hold = 0;
                                    }
                                }
                            }
                        }
                    if(game != 0){ // si no termina el juego
                        addPieceToBoard(); // Coloca la pieza permanentemente en el tablero
                        RemoveLine();  // Limpia l�neas completas y actualiza el puntaje
                        addNewPiece();     // Genera una nueva pieza en la parte superior
                    }
                } else {
                    addPieceToBoard(); // Si es v�lida, coloca la pieza en la nueva posici�n
                }
                //Nokia5110_SetCursor(0, 0);
                DrawBoard(); // Actualiza la pantalla para mostrar el nuevo estado del tablero
            }
            else if(contadortiempo > 255){
                contadortiempo = 0;
            }
            // ARRIBA
            if ((GPIO_PORTC_DATA_R & SW1) == 0) {
                while(((GPIO_PORTC_DATA_R & SW1) == 0));
                //delay(50);
                removePiece();     // Elimina la pieza actual del tablero
                currentPiece.y++;  // Mueve la pieza hacia izquierda
                // Verifica si la nueva posici�n es v�lida
                if (!verifyCollisions(currentPiece)) {
                    currentPiece.y--;  // Si no es v�lida, restaura la posici�n anterior
                }
                addPieceToBoard();
                DrawBoard(); // Actualiza la pantalla para mostrar el nuevo estado del tablero
            }
            // ABAJO
            if ((GPIO_PORTC_DATA_R & SW2) == 0 ) {
                while(((GPIO_PORTC_DATA_R & SW2) == 0));
                //delay(50); // Retardo antirrebote
                removePiece();     // Elimina la pieza actual del tablero

                currentPiece.x++;  // Mueve la pieza hacia la derecha

                // Verifica si la nueva posici�n es v�lida
                if (!verifyCollisions(currentPiece)) {
                    currentPiece.x--;  // Si no es v�lida, restaura la posici�n anterior
                }
                addPieceToBoard();
                DrawBoard(); // Actualiza la pantalla para mostrar el nuevo estado del tablero
            }
            // IZQUIERDA
            if ((GPIO_PORTC_DATA_R & SW3) == 0 ) { // SW2 presionado y m�sica encendida
                while(((GPIO_PORTC_DATA_R & SW3) == 0));
                //delay(50); // Retardo antirrebote
                removePiece();     // Elimina la pieza actual del tablero

                currentPiece.y--;  // Mueve la pieza hacia la derecha

                // Verifica si la nueva posici�n es v�lida
                if (!verifyCollisions(currentPiece)) {
                    currentPiece.y++;  // Si no es v�lida, restaura la posici�n anterior
                }
                addPieceToBoard();
                DrawBoard(); // Actualiza la pantalla para mostrar el nuevo estado del tablero
            }
            // ROTAR
            if ((GPIO_PORTC_DATA_R & SW4) == 0 ) {
                while(((GPIO_PORTC_DATA_R & SW4) == 0));
                //delay(50); // Retardo antirrebote
                removePiece();     // Elimina la pieza actual del tablero

                currentPiece.rotation++;  // Realiza una rotaci�n de 90 grados baby
                if(currentPiece.rotation == 4){
                    currentPiece.rotation = currentPiece.rotation - 4;
                }
                // Verifica si la nueva posici�n es v�lida
                if (!verifyCollisions(currentPiece)) {
                    currentPiece.rotation--;  // Si no es v�lida, restaura la posici�n anterior
                    if(currentPiece.rotation < 0){
                        currentPiece.rotation = currentPiece.rotation + 5;
                    }
                }
                addPieceToBoard();
                DrawBoard(); // Actualiza la pantalla para mostrar el nuevo estado del tablero
            }
        }
    }
}

