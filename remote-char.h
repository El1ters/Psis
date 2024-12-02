typedef enum direction_t
{
    UP,
    DOWN,
    LEFT,
    RIGHT
} direction_t;

typedef struct remote_char_t
{
    int msg_type; /* 0 - join   1 - move   2 - Firing   3 - leave  */
    char ch;
    direction_t direction;
    /* data */
} remote_char_t;

typedef struct ch_info_t
{
    int ch;
    int pos_x, pos_y;
    int score;
    
    /*Variables used to know if a client can shoot or move*/
    bool fire;
    bool move;
} ch_info_t;
