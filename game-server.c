
// Este programa é o servidor e que vai receber comunicações via ZeroMQ e manter o estado do jogo.

// Regras:
// 1. O jogo é jogado no maximo por 8 jogadores.
// 2. O jogo é jogado em um tabuleiro de 20x20.
// 3. Cada jogador tem um personagem que é representado por um caractere que é escolhido quando o jogador connecta-se ao server.
// 4. O personagem de cada jogador é colocado em uma posição no tabuleiro.
// 5. Os aliens movem-se aleatoriamente pelo tabuleiro e de 1 em 1 segundo.

#include <ncurses.h>
#include "remote-char.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zmq.h>
#include "zhelpers.h"
#include <unistd.h> // For usleep


#define WINDOW_SIZE 22 // o tabuleiro é de 20x20, mas o tamanho da janela é 22x22 para desenhar a borda

#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20
#define SCORE_WIDTH 15
#define MAX_CLIENTS 8

//ALien space - line >=3  && line <= 18 && column >=3 && column <= 18
#define IS_ALIEN_SPACE(line, column) (line >= 3 && line <= 18 && column >= 3 && column <= 18)

//Playing Areas
#define IS_AREA_A(line, column) (column == 1 && line >= 3 && line <= 18)
#define IS_AREA_B(line, column) (line == 19 && column >= 3 && column <= 18)
#define IS_AREA_C(line, column) (line == 20 && column >= 3 && column <= 18)
#define IS_AREA_D(line, column) (column == 19 && line >= 3 && line <= 18)
#define IS_AREA_E(line, column) (line == 1 && column >= 3 && column <= 18)
#define IS_AREA_F(line, column) (column == 20 && line >= 3 && line <= 18)
#define IS_AREA_G(line, column) (line == 2 && column >= 3 && column <= 18)
#define IS_AREA_H(line, column) (column == 2 && line >= 3 && line <= 18)

direction_t random_direction() // Para movimentar os aliens
{
    return random() % 4;
}

void new_position(int *x, int *y, direction_t direction)
{
    switch (direction)
    {
    case UP:
        (*x)--;
        if (*x == 0)
            *x = 1;
        break;
    case DOWN:
        (*x)++;
        if (*x == WINDOW_SIZE - 1)
            *x = WINDOW_SIZE - 2;
        break;
    case LEFT:
        (*y)--;
        if (*y == 0)
            *y = 1;
        break;
    case RIGHT:
        (*y)++;
        if (*y == WINDOW_SIZE - 1)
            *y = WINDOW_SIZE - 2;
        break;
    default:
        break;
    }
}

void draw_board(WINDOW *board_win)
{
    wclear(board_win);    // Limpa a janela antes de desenhar

    // Adiciona coordenadas no topo (na borda superior)
    for (int i = 1; i <= BOARD_WIDTH; i++)
    {
        mvwprintw(board_win, 0, i+1, "%d", i % 10); // Posição Y=0 para ficar na borda superior
    }

    // Adiciona coordenadas na lateral (na borda lateral esquerda)
    for (int i = 1; i <= BOARD_HEIGHT; i++)
    { 
        mvwprintw(board_win, i+1, 0, "%d", i % 10); // Posição X=0 para ficar na borda lateral esquerda
    }

    wrefresh(board_win); // Atualiza a janela para exibir o conteúdo
}

void draw_score(WINDOW *score_win, ch_info_t clients[], int client_count)
{

    wclear(score_win); // Clear the score window
    box(score_win, 0, 0); // Redraw the border
    mvwprintw(score_win, 1, 3, "Score");

    for (int i = 0; i < client_count; i++)
    {
        mvwprintw(score_win, i+2, 3, "%c - %d", clients[i].ch, clients[i].score);
    }

    wrefresh(score_win); // Atualiza a janela para exibir o conteúdo
}

//adicionar clientes
void add_client(ch_info_t clients[], int *client_count, int ch, int pos_x, int pos_y)
{
    clients[*client_count].ch = ch;
    clients[*client_count].pos_x = pos_x;
    clients[*client_count].pos_y = pos_y;
    clients[*client_count].score = 0;
    clients[*client_count].fire = true;
    clients[*client_count].move = true;
    (*client_count)++;
}
//remover clientes
void remove_client(ch_info_t clients[], int *client_count, int ch)
{
    for (int i = 0; i < *client_count; i++)
    {
        if (clients[i].ch == ch)
        {
            for (int j = i; j < *client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            (*client_count)--;
            break;
        }
    }
}

int find_ch_info(ch_info_t clients[], int client_count, int ch)
{
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].ch == ch)
        {
            return i;
        }
    }
    return -1;
}

char get_player_area(int line, int column)
{
    if (IS_AREA_A(line, column)) return 'A';
    if (IS_AREA_B(line, column)) return 'B';
    if (IS_AREA_C(line, column)) return 'C';
    if (IS_AREA_D(line, column)) return 'D';
    if (IS_AREA_E(line, column)) return 'E';
    if (IS_AREA_F(line, column)) return 'F';
    if (IS_AREA_G(line, column)) return 'G';
    if (IS_AREA_H(line, column)) return 'H';
    return '\0'; // Return null character if not in any area
}

bool are_coords_in_same_area(int line1, int column1, int line2, int column2)
{
    char area1 = get_player_area(line1, column1);
    char area2 = get_player_area(line2, column2);
    return area1 != '\0' && area1 == area2;
}

int ChoosePlayerArea(bool areas_occupied[])
{
    int area = (rand() % 8) + 1;

    while (areas_occupied[area] == true)
    {
        area = (rand() % 8) + 1;
    }

    return area;
}
//Funçao para escolher a posiçao do jogador - return x,y
void ChoosePlayerPosition(int area, int *x, int *y)
{
    switch (area)
    {

    // Area A    1-Esquerda
    case 0:
        /**x = 3;*/
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 1;
        break;
    // Area B    1-Baixo
    case 1:
        *x = 19;
        //*x = (rand() % (18 - 3 + 1)) + 3;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area C    2-Baixo
    case 2:
        *x = 20;
        *y = (rand() % (18 - 3 + 1)) + 3;
        break;
    // Area D    1-Direita
    case 3:
        /**x = 18;*/
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 19;
        break;
    // Area E    1-Cima
    case 4:
        *x = 1;
        *y = (rand() % (18 - 3 + 1)) + 3;
        /**y = 3;*/
        break;
    // Area F    2-Direita
    case 5:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 20;
        break;
    // Area G    2-Cima
    case 6:
        *x = 2;
        *y = (rand() % (18 - 3 + 1)) + 3;
        /**y = 3;*/
        break;
        // Area H    2-Esquerda

    case 7:
        *x = (rand() % (18 - 3 + 1)) + 3;
        *y = 2;
        /**y = 2;*/
        break;
    default:
        break;
    }
}

void Shoot(WINDOW *board_win, int x, int y, char ch,int client_count, ch_info_t clients[])
{
    int player = find_ch_info(clients, client_count, ch);
    if (IS_AREA_A(x, y) || IS_AREA_D(x, y) || IS_AREA_F(x, y) || IS_AREA_H(x, y))
    {
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, x, i) == ' ')
            {
                mvwaddch(board_win, x, i, '-');
            }
        }
        wrefresh(board_win);
        //remove every '-' after 0.5 seconds

        for (int i = 0; i < client_count; i++){
            if (clients[i].pos_x == x && i != player) // Verifica o outro cliente está na mesma coluna.
            {
                //adicionar pontos ao que disparou
                clients[player].score += 10;

                //stun the other player
                clients[i].move = false;
                clients[i].fire = false;
            }
        }

        usleep(500000);
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, x, i) == '-')
            {
                mvwaddch(board_win, x, i, ' ');
            }
        }
        wrefresh(board_win);

    }
    else if (IS_AREA_B(x, y) || IS_AREA_C(x, y) || IS_AREA_E(x, y) || IS_AREA_G(x, y))
    {
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, i, y) == ' ')
            {
                mvwaddch(board_win, i, y, '|');
            }
        }
        wrefresh(board_win);
        //remove every '-' after 0.5 seconds

        for (int i = 0; i < client_count; i++){
            if (clients[i].pos_y == y && i != player) // Verifica o outro cliente está na mesma coluna.
            {
                //adicionar pontos ao que disparou
                clients[player].score += 10;

                //stun the other player
                clients[i].move = false;
                clients[i].fire = false;
            }
        }

        usleep(500000);
        for (int i = 0; i <= 20; i++)
        {
            if (mvwinch(board_win, i, y) == '|')
            {
                mvwaddch(board_win, i, y, ' ');
            }
        }
        wrefresh(board_win);
    }
}

int main()
{
    // usar ZeroMQ TCP sockets
    void *context = zmq_ctx_new();
    void *requester = zmq_socket(context, ZMQ_REP);
    int rc = zmq_bind(requester, "ipc:///tmp/s1");
    assert(rc == 0);
    if (rc != 0)
    {
        printf("Error: %d\n", rc);
        exit(1);
    }

    //void *publisher = zmq_socket(context, ZMQ_PUB);
    //int rc = zmq_bind(publisher, "ipc:///tmp/s2");

    // Inicializa ncurses
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();

    // Cria janelas para o tabuleiro e o placar
    WINDOW *numbers = newwin(BOARD_HEIGHT + 3 , BOARD_WIDTH + 3 , 0, 0); // +3 para incluir bordas e o numero das coordenadas
    WINDOW *board_win = derwin(numbers, BOARD_HEIGHT + 2, BOARD_WIDTH + 2, 1, 1);
    WINDOW *score_win = newwin(BOARD_HEIGHT + 2, SCORE_WIDTH, 1, BOARD_WIDTH + 4);

    // Inicializa tabuleiro e placar
    draw_board(numbers);
    box(board_win, 0, 0);
    draw_score(score_win, NULL, 0);
    wrefresh(board_win);

    srand(time(NULL));
    ch_info_t clients[MAX_CLIENTS];
    bool areas_occupied[8] = {false, false, false, false, false, false, false, false};
    int client_count = 0;
    int area = -1;

    while(1){
        remote_char_t buffer;
        int pos_x, pos_y;

        if(zmq_recv(requester, &buffer, sizeof(remote_char_t), 0) == -1){
            printf("Error receiving the message: %d\n", rc);
            exit(1);
        }

        // Verifica o tipo de mensagem Tipo 0 - join, 1 - move, 2 - Firing, 3 - leave
        if (buffer.msg_type == 0)
        {
            /* int area = ChoosePlaayerArea(areas_occupied);*/
            area = ChoosePlayerArea(areas_occupied);
            areas_occupied[area] = true;
            ChoosePlayerPosition(area, &pos_x, &pos_y);

            if (client_count < 8)
                add_client(clients, &client_count, buffer.ch, pos_x, pos_y);

            wmove(board_win, pos_x, pos_y);
            waddch(board_win, buffer.ch | A_BOLD);
        }

        if (buffer.msg_type == 1)
        {
            int index = find_ch_info(clients, client_count, buffer.ch);
            if (index != -1 && clients[index].move)
            {
                pos_x = clients[index].pos_x;
                pos_y = clients[index].pos_y;
                wmove(board_win, pos_x, pos_y);
                waddch(board_win, ' ');
                new_position(&pos_x, &pos_y, buffer.direction);

                if(!are_coords_in_same_area(pos_x, pos_y, clients[index].pos_x, clients[index].pos_y)){
                    pos_x = clients[index].pos_x;
                    pos_y = clients[index].pos_y;
                }

                //Atualiza a posiçao do jogador
                clients[index].pos_x = pos_x;
                clients[index].pos_y = pos_y;

                wmove(board_win, pos_x, pos_y);
                waddch(board_win, buffer.ch | A_BOLD);
            }
        }
        else if (buffer.msg_type == 2)
        {
            int index = find_ch_info(clients, client_count, buffer.ch);
            int x = clients[index].pos_x;
            int y = clients[index].pos_y;

            Shoot(board_win, x, y, buffer.ch, client_count, clients);
            //verify if a player was stunned
            for (int i = 0; i < client_count; i++)
            {
                if (clients[i].move == false){
                    //start the player after 10 seconds

                }
            }
        }
        else if (buffer.msg_type == 3)
        {
            int index = find_ch_info(clients, client_count, buffer.ch);
            wmove(board_win, clients[index].pos_x, clients[index].pos_y);
            waddch(board_win, ' ');
            areas_occupied[get_player_area(clients[index].pos_x, clients[index].pos_y) - 'A'] = false;
            remove_client(clients, &client_count, buffer.ch);
        }

        if(zmq_send(requester, "OK", 2, 0) == -1){
            printf("Error sending the message: %d\n", rc);
            exit(1);
        }

        //Atualiando o placar e o tabuleiro
        draw_score(score_win, clients, client_count);
        wrefresh(board_win);
    }

    // Finaliza ncurses
    delwin(board_win);
    delwin(numbers);
    delwin(score_win);
    endwin();

    return 0;
}