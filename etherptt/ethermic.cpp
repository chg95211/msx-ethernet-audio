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
#include <netdb.h>
#include <pthread.h>
#include <sys/signal.h>
#include <vector>

using namespace std;

struct UDP_Destination
{
    struct sockaddr_in dest_sock_addr;
    unsigned short dest_port;
    char *dest_addr;
};

static struct
{
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
} hwparams, rhwparams;

static snd_pcm_sframes_t (*readi_func)(snd_pcm_t *handle, void *buffer,
        snd_pcm_uframes_t size);

static snd_pcm_t *handle;

static int open_mode = 0;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int nonblock = 0;
static u_char *audiobuf = NULL;
static snd_pcm_uframes_t period_frames = 0;
static unsigned period_time = 0;
static unsigned buffer_time = 0;
static unsigned max_buffer_time = 75000; // 75 ms
static int start_delay = 0;
static int stop_delay = 0;
static int verbose = 0;
static int thread_sleep = 10000; // 10 ms
static size_t bits_per_sample, bits_per_frame;
static size_t period_bytes;
static snd_output_t *log;
int shutdown_req = 0;

/* socket configuration */
static int socket_desc = 0;
static vector<UDP_Destination> destination_points;
static unsigned long sample_buffer_size;
static pthread_t udpSendThread;
static pthread_t captureThread;
static int pkts_second;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

static void start_threads();

extern int pust_to_talk_active;
extern void pushtotalk();

static void print_usage()
{
    printf("\n");
    printf("Usage: etherptt [OPTION]...\n");
    printf("\n");
    printf("Capture audio from microphone and send across a LAN");
    printf("\n");
    printf("   -l, list PCM device names\n");
    printf("   -i, select PCM input device by name\n");
    printf("   -m n, audio configuration mode\n");
    printf("      1: mu-law au fmt  (8000 hz,  8 bit, 1 channel)\n");
    printf("      2: VOIP  wav fmt (16000 hz, 16 bit, 1 channel)\n");
    printf("      3: Music wav fmt (22050 hz, 16 bit, 2 channel)\n");
    printf("   -d ip_addr:port, destination ip address and port\n");
    printf("   -h, show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("\n");
    printf("      etherptt -i default -d 127.0.0.1:6502");
    printf("\n");
    printf("      etherptt -i plughw:0,0 -m 1 -d 127.0.0.1:6502");
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

static void signal_handler(int sig)
{
    shutdown_req = 1;

    exit(EXIT_SUCCESS);
}

static void prg_exit(int code)
{
    signal_handler(code);
}

int main(int argc, char *argv[])
{
    const char *pcm_name = "default";
    snd_pcm_info_t *info;
    int err;

    snd_pcm_info_alloca(&info);

    snd_output_stdio_attach(&log, stderr, 0);

    stream = SND_PCM_STREAM_CAPTURE;
    start_delay = 1;

    rhwparams.format = SND_PCM_FORMAT_MU_LAW;
    rhwparams.channels = 1;
    rhwparams.rate = 8000;
    period_frames = 256;
    sample_buffer_size = 256;

    /* Process command line options */
    while (argc > 1)
    {
        if (argv[1][0] == '-')
        {
            switch (argv[1][1])
            {

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

            case 'd':
                struct UDP_Destination udp_dest;

                udp_dest.dest_addr = strtok(&argv[1][3], ":");
                udp_dest.dest_port = atoi(strtok(NULL, "\n"));

                destination_points.push_back(udp_dest);
                break;

            case 'h':
            default:
                print_usage();
                prg_exit(EXIT_SUCCESS);
                break;
            }
        }

        argv++;
        argc--;
    }

    if (destination_points.size() == 0)
    {
        print_usage();
        prg_exit(EXIT_SUCCESS);
    }

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
    readi_func = snd_pcm_readi;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);

    start_threads();

    pushtotalk();
            
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
    audiobuf = (u_char *) malloc(period_bytes);
    if (audiobuf == NULL)
    {
        printf("not enough memory");
        prg_exit(EXIT_FAILURE);
    }

    /* ring buffer configuration */
    pkts_second = rate * (bits_per_frame / 8) / sample_buffer_size;
}

static ssize_t pcm_read(u_char *data)
{
    int r = readi_func(handle, data, period_frames);

    if (r < 0)
    {
        printf("read error: %s", snd_strerror(r));
        prg_exit(EXIT_FAILURE);
    }

    return period_frames;
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

static void create_socket()
{
    /* Create socket descriptor */
    if ((socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        fprintf(stderr, "Couldn't create socket descriptor\n");
        prg_exit(EXIT_FAILURE);
    }

    /* Allow broadcast packets to be sent */
    int broadcast = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_BROADCAST, (char *) &broadcast,
            sizeof broadcast) == -1)
    {
        perror("setsockopt (SO_BROADCAST)");
        prg_exit(EXIT_FAILURE);
    }

    /* Set socket address attributes for destination points */
    for (unsigned i = 0; i < destination_points.size(); i++)
    {
        struct hostent *dest_host_info = gethostbyname(
                destination_points[i].dest_addr);

        destination_points[i].dest_sock_addr.sin_family = AF_INET;
        destination_points[i].dest_sock_addr.sin_port = htons(
                destination_points[i].dest_port);
        memcpy((char *) &destination_points[i].dest_sock_addr.sin_addr,
                (char *) dest_host_info->h_addr, dest_host_info->h_length);

        if (verbose)
        {
            printf("  dest address: %s:%i\n", destination_points[i].dest_addr,
                    destination_points[i].dest_port);
        }
    }
}

static void *send_data_function(void *ptr)
{
    char *read_buf = (char *) malloc(period_bytes);
    int bytes_sent;
    int num_sample_buffers = period_bytes / sample_buffer_size;

    create_socket();

    while (!shutdown_req)
    {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond_var, &mutex);
        memcpy(read_buf, audiobuf, period_bytes);
        pthread_mutex_unlock(&mutex);

        /* Send the DSP audio buffer as a stream of audio sample packets */
        for (int i = 0; i < num_sample_buffers; i++)
        {
            /* Send sample packet to each destination point */
            for (unsigned j = 0; j < destination_points.size(); j++)
            {
                bytes_sent =
                        sendto(socket_desc,
                                (const char *) read_buf
                                        + (i * sample_buffer_size),
                                sample_buffer_size, 0,
                                (struct sockaddr *) &destination_points[j].dest_sock_addr,
                                sizeof(destination_points[j].dest_sock_addr));

                if (verbose)
                {
                    printf("sent %i bytes to %s:%i\n", bytes_sent,
                            destination_points[j].dest_addr,
                            destination_points[j].dest_port);
                }

                if (bytes_sent == -1)
                {
                    printf("sendto() failed.  errno=%i\n", errno);
                    perror("sendto");
                    shutdown_req = true;
                }
            }
        }
    }

    return 0;
}

static void *capture_function(void *ptr)
{
    /* capture */
    while (!shutdown_req)
    {
        if (pust_to_talk_active)
        {
            snd_pcm_recover(handle, -EPIPE, 1);

            while (pust_to_talk_active)
            {
                pcm_read(audiobuf);

                pthread_mutex_lock(&mutex);
                pthread_cond_signal(&cond_var);
                pthread_mutex_unlock(&mutex);

                usleep(thread_sleep);
            }

            snd_pcm_nonblock(handle, 0);
            snd_pcm_drain(handle);
            snd_pcm_nonblock(handle, nonblock);
        }

        usleep(thread_sleep);
    }

    snd_pcm_close(handle);

    free(audiobuf);

    return 0;
}

static void start_threads()
{
    /* setup sound hardware */
    set_params();

    /* display header info */
    header();

    /* spawn send data thread */
    pthread_create(&udpSendThread, NULL, send_data_function, 0);

    /* capture thread */
    pthread_create(&captureThread, NULL, capture_function, 0);
}
