#include <ncurses.h>
#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h> 
#include "zhelpers.h"
#include "remote-char.h"

int main()
{
    // Initialize ZeroMQ context and socket
    void *context = zmq_ctx_new();
    // Connect to the server using ZMQ_REQ
    void *requester = zmq_socket(context, ZMQ_REQ);
    int rc = zmq_connect(requester, "ipc:///tmp/s1");
    assert(rc == 0);

    char ch;
    do
    {
        printf("what is your character?: "); // ask the user for a character
        ch = getchar();
        ch = tolower(ch);
    } while (!isalpha(ch));

    remote_char_t m;
    m.msg_type = 0;
    m.ch = ch;
    zmq_send(requester, &m, sizeof(m), 0);

    char response[10];
    zmq_recv(requester, response, sizeof(response), 0);

    // Initialize ncurses
    initscr();            /* Start curses mode 		*/
    cbreak();             /* Line buffering disabled	*/
    keypad(stdscr, TRUE); /* We get arrows etc...*/
    noecho();             /* Don't echo() while we do getch */
    curs_set(0);          // Hide the cursor

    int key;
    int n = 0;
    m.msg_type = 1; // Move message
    do
    {
        key = getch();
        n++;
        switch (key)
        {
            case KEY_LEFT:
                mvprintw(0, 0, "%d Left arrow is pressed", n);
                // TODO_9
                //  prepare the movement message
                m.direction = LEFT;
                break;
            case KEY_RIGHT:
                mvprintw(0, 0, "%d Right arrow is pressed", n);
                // TODO_9
                //  prepare the movement message
                m.direction = RIGHT;
                break;
            case KEY_DOWN:
                mvprintw(0, 0, "%d Down arrow is pressed", n);
                // TODO_9
                //  prepare the movement message
                m.direction = DOWN;
                break;
            case KEY_UP:
                mvprintw(0, 0, "%d :Up arrow is pressed", n);
                // TODO_9
                //  prepare the movement message
                m.direction = UP;
                break;
            case 'q' :
                mvprintw(0, 0, "Disconnecting...");
                m.msg_type = 3;
                break;

            default:
                key = '?';
                break;
        }

        // TODO_10
        // send the movement message
        if (key != '?')
        {   
            zmq_send(requester, &m, sizeof(m), 0);
            zmq_recv(requester, response, sizeof(response), 0);
            //Processar esta mensagem
        }
        if (key == 'q' || key == 'Q')
        {
            // Disconnect from the server
            zmq_close(requester);
            zmq_ctx_destroy(context);
            endwin(); /* End curses mode		  */
            exit(0);
        }
    } while (key != 27);
}
