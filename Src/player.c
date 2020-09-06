/*
 *	Main player logic
 */

#include <stdio.h>
#include "player.h"
#include "gui.h"
#include "mp3dec.h"
#include "fatfs.h"
#include "stm32746g_discovery_audio.h"
#include "mp3_process.h"


extern HMP3Decoder hMP3Decoder;
struct Mp3_Player_Struct state;
struct Mp3_Player_Struct newState;
extern MP3FrameInfo mp3FrameInfo;
extern short output_buffer[AUDIO_OUT_BUFFER_SIZE];
extern uint8_t *current_ptr;
extern BSP_BUFFER_STATE out_buf_offs;
extern FIL input_file;
char** paths;
static int bitrate = 0;
static int track_duration = 0;
static int track_duration_calc = 0;
static int mp3FilesCounter = 0;
static int currentFilePosition = -1;
static int currentFileBytes = 0;
extern int currentFileBytesRead;
extern int buffer_leftover;
int has_been_paused = 0;
char gui_info_text[100];
char gui_info_text2[100];
char gui_info_text3[100];
char szArtist[120];
char szTitle[120];


void BSP_init();
void mp3_play();
void player_reset();


void player_logic(const char* path)
{
    BSP_init();
    state.Mp3_Player_State = NEXT;

    DIR directory;
    FILINFO info;

    if (f_opendir(&directory, path) != FR_OK)
    	return;

    while(1)
    {
        if (f_readdir(&directory, &info) != FR_OK)
        	return;

        if (info.fname[0] == 0)
            break;

        if (strstr(info.fname, ".mp3"))
            mp3FilesCounter++;
    }

    f_closedir(&directory);

    int i = 0;
    paths = malloc(sizeof(char*) * mp3FilesCounter);

    if (paths == NULL)
    	return;

    if (f_opendir(&directory, path) != FR_OK)
    	return;

    while(1)
    {
        if (f_readdir(&directory, &info) != FR_OK)
        	return;

        if (info.fname[0] == 0)
            break;

        if (strstr(info.fname, ".mp3"))
        {
            paths[i] = malloc((strlen(info.fname) + 1) * sizeof(char));
			strcpy(paths[i], info.fname);
            i++;
        }
    }

	f_closedir(&directory);

	while(1)
	{
		switch(state.Mp3_Player_State)
		{
			case PLAY:
				if (f_findfirst(&directory, &info, path, paths[currentFilePosition]) != FR_OK) {
            		return;
        		}
				currentFileBytes = info.fsize;
				f_closedir(&directory);



				Mp3ReadId3V2Tag(&input_file, szArtist, sizeof(szArtist), szTitle, sizeof(szTitle));

				if(szTitle[0] == '\0')
				{
					strcpy(szTitle, paths[currentFilePosition]);
				}
				sprintf(gui_info_text, "%s", szArtist);
				sprintf(gui_info_text2, "%s", szTitle);
				sprintf(gui_info_text3, "%d / %d", currentFilePosition+1, mp3FilesCounter);
				main_info(gui_info_text, gui_info_text2, gui_info_text3);

				mp3_play();

                f_close(&input_file);
				currentFileBytesRead = 0;
				break;
			case NEXT:
			    player_reset();
			    if (currentFilePosition == mp3FilesCounter - 1)
                    currentFilePosition = 0;
                else
                    currentFilePosition++;
                if (f_open(&input_file, paths[currentFilePosition], FA_READ) != FR_OK) {
                    return;
                }
                state.Mp3_Player_State = PLAY;
				break;
            case PREV:
                player_reset();
                if (currentFilePosition == 0)
                    currentFilePosition = mp3FilesCounter - 1;
                else
                    currentFilePosition--;
                if (f_open(&input_file, paths[currentFilePosition], FA_READ) != FR_OK) {
                    return;
                }
                state.Mp3_Player_State = PLAY;
                break;
            case STOP:
				update_progress_bar(0, NULL);
                player_reset();
                currentFilePosition = 0;
				sprintf(gui_info_text, "STOPPED");
				main_info(gui_info_text, NULL, NULL);
                while(state.Mp3_Player_State == STOP)
                {
                    struct Mp3_Player_Struct newState = check_touchscreen();

                    if (newState.Mp3_Player_State != EMPTY)
                        state = newState;
                }
                if (state.Mp3_Player_State == PLAY)
                {
                	if (f_open(&input_file, paths[currentFilePosition], FA_READ) != FR_OK)
                    {
                        return;
                    }
                }
                break;
			case FINISH:
				return;
            default:
                return;
		}
	}
}

// Initialize AUDIO_OUT
void BSP_init()
{
	if(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, 30, AUDIO_FREQUENCY_44K) == 0)
	{
	  BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
	}
}

// Play state handler
void mp3_play()
{
	hMP3Decoder = MP3InitDecoder();
	if(frame_process() == 0)
	{
		state.Mp3_Player_State = PLAY;
		BSP_AUDIO_OUT_Play((uint16_t*)&output_buffer[0], AUDIO_OUT_BUFFER_SIZE * 2);
		while(1)
		{
			bitrate = mp3FrameInfo.bitrate;

			track_duration = ((((currentFileBytes*8)/bitrate)-((currentFileBytesRead*8)/bitrate))/60);
			track_duration_calc = ((((currentFileBytes*8)/bitrate)-((currentFileBytesRead*8)/bitrate))%60);
			sprintf(gui_info_text, "-%02d:%02d", track_duration, track_duration_calc);

			update_progress_bar(((double)currentFileBytesRead) / currentFileBytes, gui_info_text);
			newState = check_touchscreen();
			if (newState.Mp3_Player_State != EMPTY)
				state = newState;
            if (!has_been_paused && state.Mp3_Player_State == PAUSE)
            {
				sprintf(gui_info_text, "PAUSED");
				main_info(gui_info_text, NULL, NULL);
				if(BSP_AUDIO_OUT_Pause() != AUDIO_OK)
				{
					return;
				}
				has_been_paused = 1;
            }
            else if(has_been_paused && state.Mp3_Player_State == PLAY)
            {
				sprintf(gui_info_text, "%s", szArtist);
				sprintf(gui_info_text2, "%s", szTitle);
				sprintf(gui_info_text3, "%d", currentFilePosition+1);
				main_info(gui_info_text, gui_info_text2, gui_info_text3);
				if(BSP_AUDIO_OUT_Resume() != AUDIO_OK)
				{
					return;
				}
				has_been_paused = 0;
			}
            else if (has_been_paused && state.Mp3_Player_State == PAUSE)
				continue;
            else if(state.Mp3_Player_State == VOLUME)
			{
				BSP_AUDIO_OUT_SetVolume(state.Y_pos_slider);
				continue;
			}

			else if (state.Mp3_Player_State != PLAY)
			{
                break;
            }

		}
		BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
		out_buf_offs = BUFFER_OFFSET_NONE;
	}
	else
	{
		state.Mp3_Player_State = NEXT;
	}

	buffer_leftover = 0;
	current_ptr = NULL;
	MP3FreeDecoder(hMP3Decoder);
}

// reset all the used data structures
void player_reset()
{
	buffer_leftover = 0;
    current_ptr = NULL;
    out_buf_offs = BUFFER_OFFSET_NONE;
}

