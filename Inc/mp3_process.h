#ifndef MP3_PROCESS_H_
#define MP3_PROCESS_H_

// Data read from the USB and fed to the MP3 Decoder
#define READ_BUFFER_SIZE 2 * MAINBUF_SIZE

// Max size of a single frame
#define DECODED_MP3_FRAME_SIZE MAX_NGRAN * MAX_NCHAN * MAX_NSAMP

// Decoded data ready to be passed out to output (always have space to hold 2 frames)
#define AUDIO_OUT_BUFFER_SIZE 2 * DECODED_MP3_FRAME_SIZE

// State of the offset of the BSP output buffer
typedef enum BSP_BUFFER_STATE_TAG {
    BUFFER_OFFSET_NONE = 0,
    BUFFER_OFFSET_HALF,
    BUFFER_OFFSET_FULL,
} BSP_BUFFER_STATE;

uint32_t Mp3ReadId3V2Tag(FIL*, char*, uint32_t, char*, uint32_t);
int frame_process();

#endif /* MP3_PROCESS_H_ */
