#ifndef CODE_XF_ASR_AUDIO_H_
#define CODE_XF_ASR_AUDIO_H_

#include "zf_common_headfile.h"
#include "hmac_sha256.h" 
#include "base64.h"  
#include "../asr_ctrl.h"

void audio_init(void);
void audio_callback(void);
void audio_loop(void);

// TTS相关函数声明
extern void process_tts_message(const uint8 *text, uint16 len);
extern void send_utf8_text_to_tts(const uint8 *utf8_text, uint16 utf8_len);
extern bool starts_with(const char* str, const char* prefix, int prefix_len);

// 声明需要从hal_entry.c导入的函数
extern void process_cloud_response(const char *response, uint16_t len);

#endif /* CODE_XF_ASR_AUDIO_H_ */
