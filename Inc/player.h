#ifndef PLAYER_H_
#define PLAYER_H_

typedef enum Mp3_Player_State_Tag {
    PLAY,
    PAUSE,
    STOP,
    NEXT,
    PREV,
    FINISH,
	EMPTY,
	VOLUME
} State_Mp3_Player;

struct Mp3_Player_Struct
{
	State_Mp3_Player Mp3_Player_State;
	int Y_pos_slider;
} Mp3_Player;

void player_logic(const char*);

#endif /* PLAYER_H_ */
