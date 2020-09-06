#ifndef GUI_H_
#define GUI_H_

void LCD_start(void);
void draw_background(void);
struct Mp3_Player_Struct check_touchscreen();
void main_info(const char *, const char *, const char *);
void update_progress_bar(double, const char *);
void update_volume_slider(double);

#endif /* GUI_H_ */
