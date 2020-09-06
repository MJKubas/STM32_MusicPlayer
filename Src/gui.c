/*
 *	GUI + Touchscreen
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "player.h"
#include "stm32746g_discovery_lcd.h"
#include "../Utilities/Fonts/fonts.h"
#include "stm32746g_discovery_ts.h"
#include "stm32f7xx_hal.h"


#define LCD_X	480
#define LCD_Y	272
#define LAYER_FG	1
#define LAYER_BG	0
#define BUTTON_SIZE		0.12 * LCD_X
#define SLIDER_X 	0.06 * LCD_X
#define SLIDER_Y 	0.6 * LCD_Y
#define BUTTONS_NUMBER	5
#define TICKS_DELTA 	150
#define XPix(x) x * LCD_X
#define YPix(x) x * LCD_Y


static TS_StateTypeDef newState;
static TS_StateTypeDef lastState;
uint16_t newX;
uint16_t newY;
uint16_t buttons[5][2] = {
	{XPix(0.08), YPix(0.7)},
	{XPix(0.23), YPix(0.7)},
	{XPix(0.38), YPix(0.7)},
	{XPix(0.53), YPix(0.7)},
	{XPix(0.90), YPix(0.3)} };
State_Mp3_Player playButton = PLAY;
uint32_t lastTicks = 0;
char gui_info_text[100];


void update_volume_slider(double);
void draw_buttons(void);
void update_play_button(void);


// Initialize the LCD display
void LCD_start(void)
{
  /* LCD Initialization */
  BSP_LCD_Init();

  /* LCD Initialization */
  BSP_LCD_LayerDefaultInit(LAYER_BG, (unsigned int)0xC0000000);
  BSP_LCD_LayerDefaultInit(LAYER_FG, (unsigned int)0xC0000000+(LCD_X*LCD_Y*4));

  /* Enable the LCD */
  BSP_LCD_DisplayOn();

  /* Select the LCD Background Layer  */
  BSP_LCD_SelectLayer(LAYER_BG);

  /* Clear the Background Layer */
  BSP_LCD_Clear(LCD_COLOR_DARKGRAY);
  BSP_LCD_SetBackColor(LCD_COLOR_DARKGRAY);
  BSP_LCD_SetColorKeying(LAYER_FG,LCD_COLOR_DARKGRAY);

  /* Select the LCD Foreground Layer  */
  BSP_LCD_SelectLayer(LAYER_FG);

  /* Clear the Foreground Layer */
  BSP_LCD_Clear(LCD_COLOR_DARKGRAY);
  BSP_LCD_SetBackColor(LCD_COLOR_DARKGRAY);

  /* Configure the transparency for foreground and background :
     Increase the transparency */
  BSP_LCD_SetTransparency(LAYER_BG, 255);
  BSP_LCD_SetTransparency(LAYER_FG, 255);
}

// Draw the screen background
void draw_background(void)
{
	BSP_LCD_SelectLayer(LAYER_BG);
	draw_buttons();
	char greeting[] = "STARTING...";
	BSP_LCD_DisplayStringAt(XPix(0.05), YPix(0.20), (unsigned char *)greeting, LEFT_MODE);
	BSP_LCD_SelectLayer(LAYER_FG);
}

// Check for a new touch input
struct Mp3_Player_Struct check_touchscreen()
{
	struct Mp3_Player_Struct mp3_player;
	uint32_t currentTicks = HAL_GetTick();

	if (currentTicks < lastTicks + TICKS_DELTA)
	{
		mp3_player.Mp3_Player_State = EMPTY;
		return mp3_player;
	}

	lastTicks = currentTicks;

    BSP_TS_GetState(&newState);
	if (newState.touchDetected == 0)
	{
		mp3_player.Mp3_Player_State = EMPTY;
		return mp3_player;
	}

	newX = newState.touchX[0];
	newY = newState.touchY[0];

	lastState.touchX[0] = newX;
	lastState.touchY[0] = newY;

	if ((newX <= buttons[0][0] + BUTTON_SIZE &&
		newY <= buttons[0][1] + BUTTON_SIZE &&
		newX > buttons[0][0] &&	newY > buttons[0][1]))
	{
		mp3_player.Mp3_Player_State = PREV;
		return mp3_player;
	}

	else if ((newX <= buttons[1][0] + BUTTON_SIZE &&
			newY <= buttons[1][1] + BUTTON_SIZE &&
			newX > buttons[1][0] && newY > buttons[1][1]))
	{
		if (playButton == PLAY)
		{
			playButton = PAUSE;
		}

		else
		{
			playButton = PLAY;
		}
		mp3_player.Mp3_Player_State = playButton;
		return mp3_player;
	}

	else if ((newX <= buttons[2][0] + BUTTON_SIZE &&
			newY <= buttons[2][1] + BUTTON_SIZE &&
			newX > buttons[2][0] && newY > buttons[2][1]))
	{

		playButton = PAUSE;
		mp3_player.Mp3_Player_State = STOP;
		return mp3_player;
	}

	else if ((newX <= buttons[3][0] + BUTTON_SIZE &&
			newY <= buttons[3][1] + BUTTON_SIZE &&
			newX > buttons[3][0] && newY > buttons[3][1]))
	{
		mp3_player.Mp3_Player_State = NEXT;
		return mp3_player;
	}

	else if ((newX <= buttons[4][0] + SLIDER_X &&
			newY <= buttons[4][1] + SLIDER_Y &&
			newX > buttons[4][0] && newY > buttons[4][1]))
	{
		mp3_player.Mp3_Player_State = VOLUME;
		mp3_player.Y_pos_slider = fabs(((newY-(YPix(0.3)))/(SLIDER_Y))-1)*100;
		update_volume_slider(newY);
		return mp3_player;
	}

	else
	{
		mp3_player.Mp3_Player_State = EMPTY;
		return mp3_player;
	}
}

// Refresh the song info
void main_info(const char *author_text, const char *title_text, const char *number_text)
{
	// Erase previous text
	BSP_LCD_SelectLayer(LAYER_FG);
	BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
	BSP_LCD_FillRect(0, YPix(0.10) - 1, LCD_X - 70, 30);
	BSP_LCD_FillRect(0, YPix(0.20) - 1, LCD_X - 70, 30);
	BSP_LCD_FillRect(0, YPix(0.30) - 1, LCD_X - 70, 30);

	// Workaround for not erasing all previous text when new one is shorter
	BSP_LCD_FillRect(0, YPix(0.10) - 1, LCD_X - 70, 30);
	BSP_LCD_FillRect(0, YPix(0.20) - 1, LCD_X - 70, 30);
	BSP_LCD_FillRect(0, YPix(0.30) - 1, LCD_X - 70, 30);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);

	if(author_text != NULL)
	{
		char buf[100];
		sprintf(buf, " %s", author_text);
		BSP_LCD_DisplayStringAt(XPix(0.05), YPix(0.10), (unsigned char *)buf,LEFT_MODE);
	}

	if(title_text != NULL)
	{
		char buf2[100];
		sprintf(buf2, " %s", title_text);
		BSP_LCD_DisplayStringAt(XPix(0.05), YPix(0.20), (unsigned char *)buf2,LEFT_MODE);
	}

	if(number_text != NULL)
	{
		char buf3[100];
		sprintf(buf3, " %s", number_text);
		BSP_LCD_DisplayStringAt(XPix(0.05), YPix(0.30), (unsigned char *)buf3,LEFT_MODE);
	}

	update_play_button();
}

// Draw all necessary control buttons
void draw_buttons()
{
	BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
	BSP_LCD_FillRect(buttons[0][0], buttons[0][1],  BUTTON_SIZE, BUTTON_SIZE);
	BSP_LCD_FillRect(buttons[1][0], buttons[1][1],  BUTTON_SIZE, BUTTON_SIZE);
	BSP_LCD_FillRect(buttons[2][0], buttons[2][1],  BUTTON_SIZE, BUTTON_SIZE);
	BSP_LCD_FillRect(buttons[3][0], buttons[3][1],  BUTTON_SIZE, BUTTON_SIZE);

	// Volume slider
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DrawRect(buttons[4][0], buttons[4][1],  SLIDER_X, SLIDER_Y);

	// Progress bar
	BSP_LCD_DrawRect(30, YPix(0.45), (LCD_X - 180), 19);

	BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGRAY);
	uint16_t xButton, yButton;

	// Previous button label
	xButton = buttons[0][0];
	yButton = buttons[0][1];
	Point Points1[]= {{xButton + 4, yButton + BUTTON_SIZE / 2}, {xButton + BUTTON_SIZE / 2, yButton + 4},
		{xButton + BUTTON_SIZE / 2, yButton + 4 + 48}};
	BSP_LCD_FillPolygon(Points1, 3);
	Points1[0].X += 24;
	Points1[1].X += 24;
	Points1[2].X += 24;
	BSP_LCD_FillPolygon(Points1, 3);

	// Play/Pause button label
	update_play_button();

	BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGRAY);
	// Stop button label
	xButton = buttons[2][0];
	yButton = buttons[2][1];
	BSP_LCD_FillRect(xButton + 4, yButton + 4, 49, 49);

	// Next button label
	xButton = buttons[3][0];
	yButton = buttons[3][1];
	Point Points3[]= {{xButton + 4, yButton + 4}, {xButton + 4, yButton + 4 + 48},
		{xButton + BUTTON_SIZE / 2 , yButton + BUTTON_SIZE / 2}};
	BSP_LCD_FillPolygon(Points3, 3);
	Points3[0].X += 24;
	Points3[1].X += 24;
	Points3[2].X += 24;
	BSP_LCD_FillPolygon(Points3, 3);

	update_volume_slider(195);
}

// Update volume slider fill and %
void update_volume_slider(double Y_pos)
{
	BSP_LCD_SelectLayer(LAYER_FG);
	BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
	BSP_LCD_FillRect(buttons[4][0], buttons[4][1],  SLIDER_X, SLIDER_Y);
	BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGRAY);
	BSP_LCD_FillRect(buttons[4][0], Y_pos,  SLIDER_X, ((SLIDER_Y) + (YPix(0.3))) - (Y_pos));

	double vol_display = fabs(((Y_pos-(YPix(0.3)))/(SLIDER_Y))-1)*100;
	char buf[100];
	sprintf(buf, " %02d", (int)vol_display);
	BSP_LCD_DisplayStringAt(XPix(0.85), YPix(0.20), (unsigned char *)buf,LEFT_MODE);
}

// Update progress bar for the current song
void update_progress_bar(double progress, const char *time )
{
	BSP_LCD_SelectLayer(LAYER_FG);

	if(progress <= 0.001)
	{
		BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
		BSP_LCD_FillRect(30, YPix(0.45) - 1, (LCD_X - 179), 21);
		return;
	}

	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_FillRect(30, YPix(0.45),  (uint16_t)(progress * (LCD_X - 180)), 19);

	if(time != NULL)
	{
		char buf[100];
		sprintf(buf, " %s", time);
		BSP_LCD_DisplayStringAt(LCD_X - 230, YPix(0.57), (unsigned char *)buf,LEFT_MODE);
	}
}

// Refresh PLAY/PAUSE button
void update_play_button()
{
	BSP_LCD_SelectLayer(LAYER_BG);
	BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
	BSP_LCD_FillRect(buttons[1][0], buttons[1][1],  BUTTON_SIZE, BUTTON_SIZE);
	BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGRAY);
	if(playButton == PAUSE)
	{
		Point Points2[]= {{buttons[1][0] + 4, buttons[1][1] + 4}, {buttons[1][0] + 4, buttons[1][1] + 4 + 48},
			{buttons[1][0] + 4 + 48, buttons[1][1] + BUTTON_SIZE / 2}};
		BSP_LCD_FillPolygon(Points2, 3);
	}
	else if(playButton == PLAY)
	{
		BSP_LCD_FillRect(buttons[1][0] + 4, buttons[1][1] + 4, 22, BUTTON_SIZE - 8);
		BSP_LCD_FillRect(buttons[1][0] + 4 + 27, buttons[1][1] + 4, 22, BUTTON_SIZE - 8);
	}

}
