/*
 *   MSX Ethernet Audio
 *
 *   Copyright (C) 2014 Harlan Murphy
 *   Orbis Software - orbisoftware@gmail.com
 *   Based on aplay by Jaroslav Kysela and vplay by Michael Beck
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <alsa/asoundlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>
#include "ringbuffer.h"

enum
{
    FILE_PLAYBACK, NETWORK_PLAYBACK
};

/* snd pcm configuration */
#define FORMAT_RAW         0

static snd_pcm_sframes_t (*writei_func)(snd_pcm_t *handle, const void *buffer,
        snd_pcm_uframes_t size);

static snd_pcm_t *handle;
static struct
{
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
} hwparams, rhwparams;

static int file_fd = 0;
static int playback_mode = NETWORK_PLAYBACK;
static int playback_delay = 0; // 0 ms
static int open_mode = 0;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int nonblock = 0;
static char *audiobuf = NULL;
static snd_pcm_uframes_t period_frames = 0;
static unsigned period_time = 0;
static unsigned buffer_time = 0;
static unsigned max_buffer_time = 75000; // 75 ms
static int start_delay = 0;
static int stop_delay = 0;
static int verbose = 0;
static size_t bits_per_sample, bits_per_frame;
static size_t period_bytes;
static snd_output_t *log;

/* socket configuration */
static int udp_receive_port = 6502;
static unsigned long sample_buffer_size;
static int shutdown_req = 0;
static pthread_t udpRecThread;
static int sock_fd = 0;
static int packet_cnt = 0;
static int pkts_second;

/* ring buffer configuration */
static int ring_buffer_bytes;
static ringbuffer_t *rb;

/* prototypes */
static void file_playback(char *filename);
static void rb_playback();

static void signal_handler(int sig)
{
    shutdown_req = 1;

    exit(0);
}

static void prg_exit(int code)
{
    signal_handler(code);
}

static void print_usage()
{
    printf("\n");
    printf("Usage: etherplay [OPTION]...\n");
    printf("\n");
    printf("Audio playback from across a LAN or from a file");
    printf("\n");
    printf("   -l, list PCM device names\n");
    printf("   -i, select PCM input device\n");
    printf("   -f filename, file playback mode\n");
    printf("   -m n, audio configuration mode\n");
    printf("      1: mu-law au fmt  (8000 hz,  8 bit, 1 channel)\n");
    printf("      2: VOIP  wav fmt (16000 hz, 16 bit, 1 channel)\n");
    printf("      3: Music wav fmt (22050 hz, 16 bit, 2 channel)\n");
    printf(
            "   -p, UDP port to listen on for network audio packets (6502 default)\n");
    printf("   -h, show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("\n");
    printf("      etherplay -i default -p 6502 -m 1");
    printf("\n");
    printf("      etherplay -i plughw:0,0 -f sample.au -m 2");
    printf("\n");
}

static void pcm_list(void)
{
    void **hints, **n;
    char *name, *descr, *descr1, *io;
    const char *filter = "Input";

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return;
    n = hints;

    while (*n != NULL)
    {
        name = snd_device_name_get_hint(*n, "NAME");
        descr = snd_device_name_get_hint(*n, "DESC");
        io = snd_device_name_get_hint(*n, "IOID");
        if (io != NULL && strcmp(io, filter) != 0)
            goto __end;
        printf("%s\n", name);
        if ((descr1 = descr) != NULL)
        {
            printf("    ");
            while (*descr1)
            {
                if (*descr1 == '\n')
                    printf("\n    ");
                else
                    putchar(*descr1);
                descr1++;
            }
            putchar('\n');
        }
        __end: if (name != NULL)
            free(name);
        if (descr != NULL)
            free(descr);
        if (io != NULL)
            free(io);
        n++;
    }
    snd_device_name_free_hint(hints);
}

int main(int argc, char *argv[])
{
    char *pcm_name = "default";
    char *filename;
    int err;
    snd_pcm_info_t *info;

    /* Default to mu-law audio configuration */
    rhwparams.format = SND_PCM_FORMAT_MU_LAW;
    rhwparams.channels = 1;
    rhwparams.rate = 8000;
    sample_buffer_size = 256;
    period_frames = 256;

    /* Process command line options */
    while (argc > 1)
    {
        if (argv[1][0] == '-')
        {
            switch (argv[1][1])
            {

            case 'f':
                playback_mode = FILE_PLAYBACK;
                filename = &argv[1][3];
                break;

            case 'p':
                udp_receive_port = atoi(&argv[1][3]);
                break;

            case 'l':
                pcm_list();
                prg_exit(EXIT_SUCCESS);
                break;

            case 'i':
                pcm_name = &argv[1][3];
                break;

            case 'm':
                if (strcasecmp(&argv[1][3], "1") == 0)
                {
                    rhwparams.format = SND_PCM_FORMAT_MU_LAW;
                    rhwparams.channels = 1;
                    rhwparams.rate = 8000;
                    period_frames = 256;
                    sample_buffer_size = 256;
                }
                else if (strcasecmp(&argv[1][3], "2") == 0)
                {
                    rhwparams.format = SND_PCM_FORMAT_S16_LE;
                    rhwparams.channels = 1;
                    rhwparams.rate = 16000;
                    period_frames = 512;
                    sample_buffer_size = 1024;
                }
                else if (strcasecmp(&argv[1][3], "3") == 0)
                {
                    rhwparams.format = SND_PCM_FORMAT_S16_LE;
                    rhwparams.channels = 2;
                    rhwparams.rate = 22050;
                    period_frames = 256;
                    sample_buffer_size = 1024;
                }
                else
                {
                    printf("Unrecognized audio configuration mode %s\n",
                            &argv[1][3]);
                    prg_exit(EXIT_FAILURE);
                }
                break;

                /* show this help message */
            case 'h':
            default:
                print_usage();
                exit(0);
                break;
            }
        }

        argv++;
        argc--;
    }

    snd_pcm_info_alloca(&info);

    err = snd_output_stdio_attach(&log, stderr, 0);
    assert(err >= 0);

    stream = SND_PCM_STREAM_PLAYBACK;

    err = snd_pcm_open(&handle, pcm_name, stream, open_mode);
    if (err < 0)
    {
        printf("audio open error: %s", snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_info(handle, info)) < 0)
    {
        printf("info error: %s", snd_strerror(err));
        return 1;
    }

    hwparams = rhwparams;
    writei_func = snd_pcm_writei;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);

    if (playback_mode == NETWORK_PLAYBACK)
    {
        printf("Listening for audio packets on port: %i\n", udp_receive_port);
        rb_playback();
    }
    else if (playback_mode == FILE_PLAYBACK)
    {
        printf("Now playing file %s\n", filename);
        file_playback(filename);
    }

    snd_pcm_close(handle);

    free(audiobuf);

    return 0;
}

static void set_params(void)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size;
    int err;
    size_t n;
    unsigned int rate;
    snd_pcm_uframes_t start_threshold, stop_threshold;

    /* start hw params */
    snd_pcm_hw_params_alloca(&params);

    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0)
    {
        printf(
                "Broken configuration for this PCM: no configurations available");
        prg_exit(EXIT_FAILURE);
    }
    err = snd_pcm_hw_params_set_access(handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        printf("Access type not available");
        prg_exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);
    if (err < 0)
    {
        printf("Sample format non available");
        prg_exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);
    if (err < 0)
    {
        printf("Channels count non available");
        prg_exit(EXIT_FAILURE);
    }

    rate = hwparams.rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_period_size(handle, params, period_frames, 0);
    //assert(err >= 0);

    err = snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
    assert(err >= 0);
    if (buffer_time > max_buffer_time)
        buffer_time = max_buffer_time;

    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time,
            0);
    assert(err >= 0);

    /* apply desired hw params to handle */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0)
    {
        printf("Unable to install hw params:");
        snd_pcm_hw_params_dump(params, log);
        prg_exit(EXIT_FAILURE);
    }

    /* retrieve period_time and buffer_size from configuration */
    snd_pcm_hw_params_get_period_time(params, &period_time, 0);
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);

    /* start sw params */
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(handle, swparams);

    n = period_frames;
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);
    assert(err >= 0);

    /* round up to closest transfer boundary */
    n = buffer_size;
    if (start_delay <= 0)
    {
        start_threshold = n + (double) rate * start_delay / 1000000;
    }
    else
        start_threshold = (double) rate * start_delay / 1000000;
    if (start_threshold < 1)
        start_threshold = 1;
    if (start_threshold > n)
        start_threshold = n;
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams,
            start_threshold);
    assert(err >= 0);
    if (stop_delay <= 0)
        stop_threshold = buffer_size + (double) rate * stop_delay / 1000000;
    else
        stop_threshold = (double) rate * stop_delay / 1000000;
    err = snd_pcm_sw_params_set_stop_threshold(handle, swparams,
            stop_threshold);
    assert(err >= 0);

    if (snd_pcm_sw_params(handle, swparams) < 0)
    {
        printf("unable to install sw params:");
        snd_pcm_sw_params_dump(swparams, log);
        prg_exit(EXIT_FAILURE);
    }

    if (verbose)
        snd_pcm_dump(handle, log);

    bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
    bits_per_frame = bits_per_sample * hwparams.channels;
    period_bytes = period_frames * bits_per_frame / 8;
    audiobuf = malloc(period_bytes);
    if (audiobuf == NULL)
    {
        printf("not enough memory");
        prg_exit(EXIT_FAILURE);
    }

    /* ring buffer configuration */
    pkts_second = rate * (bits_per_frame / 8) / sample_buffer_size;

    /* ring buffer to accommodate 2 seconds of audio packets */
    ring_buffer_bytes = sample_buffer_size * pkts_second * 2;
}

static ssize_t pcm_write(char *data, size_t count)
{
    ssize_t r;
    ssize_t result = 0;

    if (count < period_frames)
    {
        snd_pcm_format_set_silence(hwparams.format,
                data + count * bits_per_frame / 8,
                (period_frames - count) * hwparams.channels);
        count = period_frames;
    }
    while ((count > 0) && !shutdown_req)
    {
        r = writei_func(handle, data, count);

        if (r == -EPIPE)
        {
            snd_pcm_recover(handle, -EPIPE, 1);
            r = writei_func(handle, data, count);
        }

        if (r < 0)
        {
            printf("write error: %s", snd_strerror(r));
            prg_exit(EXIT_FAILURE);
        }

        if (r > 0)
        {
            result += r;
            count -= r;
            data += r * bits_per_frame / 8;
        }
    }
    return result;
}

static void header()
{
    printf("%s, ", snd_pcm_format_description(hwparams.format));
    printf("Rate %d Hz, ", hwparams.rate);
    if (hwparams.channels == 1)
        printf("Mono");
    else if (hwparams.channels == 2)
        printf("Stereo");
    else
        printf("Channels %i", hwparams.channels);
    printf("\n");

    printf("DSP chunk size = %i", (int) period_bytes);
    printf(", UDP buffer size = %lu", sample_buffer_size);
    printf(", Pkts/Sec = %i", pkts_second);
    printf("\n");
}

static int elapsed(struct timeval *period_start)
{
    struct timeval now;
    unsigned long elapsed_usec;

    gettimeofday(&now, NULL);

    elapsed_usec = ((now.tv_sec - period_start->tv_sec) * 1000000) + now.tv_usec
            - period_start->tv_usec;

    return elapsed_usec;
}

static void start_playback(int fd)
{
    int pcm_out, read_cnt, last_read, bytes_read;
    struct timeval period_start;

    while (!shutdown_req)
    {
        read_cnt = 0;
        last_read = 0;
        bytes_read = 0;

        /* Set the initial start time for this period */
        gettimeofday(&period_start, NULL);

        if (playback_mode == NETWORK_PLAYBACK)
        {
            /* Continue to read buffers for the period, or timeout if the network
             * throughput is not meeting the DSP timing requirements. */
            while ((bytes_read < period_bytes)
                    && (elapsed(&period_start) < (period_time * 4)))
            {
                if (ringbuffer_read_space(rb) >= sample_buffer_size)
                {
                    last_read = ringbuffer_read(rb, audiobuf + bytes_read,
                            sample_buffer_size);
                    bytes_read += last_read;
                }
                else
                    usleep(10000);
            }
        }
        else if (playback_mode == FILE_PLAYBACK)
            bytes_read = read(fd, audiobuf, period_bytes);

        if (bytes_read != period_bytes)
            break;

        read_cnt += bytes_read;
        read_cnt = read_cnt * 8 / bits_per_frame;
        pcm_out = pcm_write(audiobuf, read_cnt);

        if (pcm_out != read_cnt)
            break;
    }

    snd_pcm_nonblock(handle, 0);
    snd_pcm_drain(handle);
    snd_pcm_nonblock(handle, nonblock);
}

static void *rcv_data_function(void *ptr)
{
    int sock_rcvd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len;
    char sample_buffer[sample_buffer_size];

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(udp_receive_port);
    bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    while (!shutdown_req)
    {
        sock_rcvd = 0;

        len = sizeof(client_addr);
        sock_rcvd = recvfrom(sock_fd, sample_buffer, sample_buffer_size, 0,
                (struct sockaddr *) &client_addr, &len);

        if (sock_rcvd == sample_buffer_size)
        {
            packet_cnt++;
            ringbuffer_write(rb, (const char *) sample_buffer, sock_rcvd);
        }
    }

    if (sock_fd > 1)
        close(sock_fd);

    pthread_exit(0);
}

static void rb_playback()
{
    int prev_packet_cnt = 0;

    /* setup sound hardware */
    set_params();

    /* display header info */
    header();

    /* initialize ring buffer */
    rb = ringbuffer_create(ring_buffer_bytes);

    /* spawn rcv data thread */
    pthread_create(&udpRecThread, NULL, rcv_data_function, 0);

    /* rb playback */
    while (!shutdown_req)
    {
        /* wait for packets to arrive, before starting playback */
        if (packet_cnt != prev_packet_cnt)
        {
            usleep(playback_delay);
            snd_pcm_recover(handle, -EPIPE, 1);
            start_playback(0);
        }

        prev_packet_cnt = packet_cnt;
        usleep(10000);
    }

    ringbuffer_free(rb);
}

static void file_playback(char *name)
{
    if ((file_fd = open(name, O_RDONLY, 0)) == -1)
    {
        perror(name);
        prg_exit(EXIT_FAILURE);
    }

    /* setup sound hardware */
    set_params();

    /* display header info */
    header();

    /* file playback */
    start_playback(file_fd);

    if (file_fd > 0)
        close(file_fd);
}
