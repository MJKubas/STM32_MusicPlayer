/*
 *	Processing mp3 file
 */

#include <stdio.h>
#include "mp3dec.h"
#include "fatfs.h"
#include "stm32746g_discovery_audio.h"
#include "mp3_process.h"
#include "player.h"


HMP3Decoder hMP3Decoder;
struct Mp3_Player_Struct state;
MP3FrameInfo mp3FrameInfo;
short output_buffer[AUDIO_OUT_BUFFER_SIZE];
uint8_t input_buffer[READ_BUFFER_SIZE];
uint8_t *current_ptr;
BSP_BUFFER_STATE out_buf_offs = BUFFER_OFFSET_NONE;
FIL input_file;
int currentFileBytesRead = 0;
int buffer_leftover = 0;
static int in_buf_offs;
static int decode_result;


int frame_process();
int fill_input_buffer();
void copy_leftover();
static uint32_t Mp3ReadId3V2Text(FIL*, uint32_t, char*, uint32_t);

// Process next mp3 frame from the main file
int frame_process(void)
{
	if (current_ptr == NULL && fill_input_buffer() != 0) return EOF;

	in_buf_offs = MP3FindSyncWord(current_ptr, buffer_leftover);

	while(in_buf_offs < 0)
	{
		if(fill_input_buffer() != 0) return EOF;
		if(buffer_leftover > 0)
		{
			buffer_leftover--;
			current_ptr++;
		}
		in_buf_offs = MP3FindSyncWord(current_ptr, buffer_leftover);
	}

	current_ptr += in_buf_offs;
	buffer_leftover -= in_buf_offs;

	// get data from the frame header
	if(!(MP3GetNextFrameInfo(hMP3Decoder, &mp3FrameInfo, current_ptr) == 0 && mp3FrameInfo.nChans == 2 && mp3FrameInfo.version == 0))
	{
		// if header has failed
		if(buffer_leftover > 0)
		{
			buffer_leftover--;
			current_ptr++;
		}
		return 0;
	}

	// if feel the buffer with actual non-frame-header data if necessary
	if (buffer_leftover < MAINBUF_SIZE && fill_input_buffer() != 0) return EOF;

	// decode the right portion of the buffer
	if(out_buf_offs == BUFFER_OFFSET_HALF)
	{
		decode_result = MP3Decode(hMP3Decoder, &current_ptr, &buffer_leftover, output_buffer, 0);
		out_buf_offs = BUFFER_OFFSET_NONE;
	}

	if(out_buf_offs == BUFFER_OFFSET_FULL)
	{
		decode_result = MP3Decode(hMP3Decoder, &current_ptr, &buffer_leftover, &output_buffer[DECODED_MP3_FRAME_SIZE], 0);
		out_buf_offs = BUFFER_OFFSET_NONE;
	}

	// check results of the decoding process
	if(decode_result != ERR_MP3_NONE)
	{
		if(decode_result == ERR_MP3_INDATA_UNDERFLOW)
		{
			buffer_leftover = 0;
			if(fill_input_buffer() != 0) return EOF;
		}
	}

	return 0;
}

// Callback when half of audio out buffer is transefered
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    out_buf_offs = BUFFER_OFFSET_FULL;

	if(frame_process() != 0)
	{
		BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
		state.Mp3_Player_State = NEXT;
	}
}

// Callback when all of audio out buffer is transefered
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    out_buf_offs = BUFFER_OFFSET_HALF;

	if(frame_process() != 0)
	{
		BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
		state.Mp3_Player_State = NEXT;
	}
}

// Fill the input buffer with mp3 data from the USB for the decoder
int fill_input_buffer()
{
	unsigned int actually_read, how_much_to_read;

	copy_leftover();

	how_much_to_read = READ_BUFFER_SIZE - buffer_leftover;

	// read from the input_file to fill the input_buffer fully
	f_read(&input_file, (BYTE *)input_buffer + buffer_leftover, how_much_to_read, &actually_read);

	currentFileBytesRead += actually_read;

	// if there's still data in  the file
	if (actually_read == how_much_to_read)
	{
		current_ptr = input_buffer;
		in_buf_offs = 0;
		buffer_leftover = READ_BUFFER_SIZE;
		return 0;
	}
	else return EOF;
}

// if there is some data left in the buffer copy it to the beginning
void copy_leftover()
{
	if(buffer_leftover > 0)
	{
		memcpy(input_buffer, current_ptr, buffer_leftover);
	}
}

static uint32_t Mp3ReadId3V2Text(FIL* pInFile, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize)
{
	UINT unRead = 0;
	BYTE byEncoding = 0;
	if((f_read(pInFile, &byEncoding, 1, &unRead) == FR_OK) && (unRead == 1))
	{
		unDataLen--;
		if(unDataLen <= (unBufferSize - 1))
		{
			if((f_read(pInFile, pszBuffer, unDataLen, &unRead) == FR_OK) ||
					(unRead == unDataLen))
			{
				if(byEncoding == 0)
				{
					// ISO-8859-1 multibyte
					// just add a terminating zero
					pszBuffer[unDataLen] = 0;
				}
				else if(byEncoding == 1)
				{
					// UTF16LE unicode
					uint32_t r = 0;
					uint32_t w = 0;
					if((unDataLen > 2) && (pszBuffer[0] == 0xFF) && (pszBuffer[1] == 0xFE))
					{
						// ignore BOM, assume LE
						r = 2;
					}
					for(; r < unDataLen; r += 2, w += 1)
					{
						// should be acceptable for 7 bit ascii
						pszBuffer[w] = pszBuffer[r];
					}
					pszBuffer[w] = 0;
				}
			}
			else
			{
				return 1;
			}
		}
		else
		{
			// we won't read a partial text
			if(f_lseek(pInFile, f_tell(pInFile) + unDataLen) != FR_OK)
			{
				return 1;
			}
		}
	}
	else
	{
		return 1;
	}
	return 0;
}

uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize)
{
	pszArtist[0] = 0;
	pszTitle[0] = 0;

	BYTE id3hd[10];
	UINT unRead = 0;
	if((f_read(pInFile, id3hd, 10, &unRead) != FR_OK) || (unRead != 10))
	{
		return 1;
	}
	else
	{
		uint32_t unSkip = 0;
		if((unRead == 10) &&
				(id3hd[0] == 'I') &&
				(id3hd[1] == 'D') &&
				(id3hd[2] == '3'))
		{
			unSkip += 10;
			unSkip = ((id3hd[6] & 0x7f) << 21) | ((id3hd[7] & 0x7f) << 14) | ((id3hd[8] & 0x7f) << 7) | (id3hd[9] & 0x7f);

			// try to get some information from the tag
			// skip the extended header, if present
			uint8_t unVersion = id3hd[3];
			if(id3hd[5] & 0x40)
			{
				BYTE exhd[4];
				f_read(pInFile, exhd, 4, &unRead);
				size_t unExHdrSkip = ((exhd[0] & 0x7f) << 21) | ((exhd[1] & 0x7f) << 14) | ((exhd[2] & 0x7f) << 7) | (exhd[3] & 0x7f);
				unExHdrSkip -= 4;
				if(f_lseek(pInFile, f_tell(pInFile) + unExHdrSkip) != FR_OK)
				{
					return 1;
				}
			}
			uint32_t nFramesToRead = 2;
			while(nFramesToRead > 0)
			{
				char frhd[10];
				if((f_read(pInFile, frhd, 10, &unRead) != FR_OK) || (unRead != 10))
				{
					return 1;
				}
				if((frhd[0] == 0) || (strncmp(frhd, "3DI", 3) == 0))
				{
					break;
				}
				char szFrameId[5] = {0, 0, 0, 0, 0};
				memcpy(szFrameId, frhd, 4);
				uint32_t unFrameSize = 0;
				uint32_t i = 0;
				for(; i < 4; i++)
				{
					if(unVersion == 3)
					{
						// ID3v2.3
						unFrameSize <<= 8;
						unFrameSize += frhd[i + 4];
					}
					if(unVersion == 4)
					{
						// ID3v2.4
						unFrameSize <<= 7;
						unFrameSize += frhd[i + 4] & 0x7F;
					}
				}

				if(strcmp(szFrameId, "TPE1") == 0)
				{
					// artist
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszArtist, unArtistSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else if(strcmp(szFrameId, "TIT2") == 0)
				{
					// title
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszTitle, unTitleSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else
				{
					if(f_lseek(pInFile, f_tell(pInFile) + unFrameSize) != FR_OK)
					{
						return 1;
					}
				}
			}
		}
		if(f_lseek(pInFile, unSkip) != FR_OK)
		{
			return 1;
		}
	}

	return 0;
}
