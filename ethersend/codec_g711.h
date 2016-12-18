#ifndef CODEC_G711_H_
#define CODEC_G711_H_

unsigned char linear2ulaw(short pcm_val);
short ulaw2linear(unsigned char u_val);

#endif
