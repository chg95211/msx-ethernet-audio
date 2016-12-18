/*
 *   MSX Ethernet Audio
 *
 *   Copyright (C) 2012 Harlan Murphy
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
#include <sys/time.h>
#include <vector>

#include "codec_g711.h"

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
    unsigned int bytes_sample;
} rhwparams;

/* socket configuration */
static int socket_desc = 0;
static vector<UDP_Destination> destination_points;
static unsigned long sample_buffer_size;
static int packet_cnt = 0;

static int verbose_debug = 0;

static double get_time()
{
    struct timeval tv;
    double time;

    gettimeofday(&tv, NULL);

    time = tv.tv_sec + (tv.tv_usec / 1000000.0);

    return time;
}

static void period_delay(double time_sec)
{
    usleep(time_sec * 1000000);
}

static void play(char* file_name)
{
    double sample_time = 1.0 / rhwparams.rate;
    double pkts_second = rhwparams.rate * rhwparams.bytes_sample
            * rhwparams.channels / sample_buffer_size;
    double period_sleep = 1.0 / (pkts_second);
    double period_adj_l = 0.25;
    double period_adj_s = 0.01;
    double pal_chk = period_adj_l * period_sleep;

    FILE *file;
    int bytes_sent;
    unsigned long bytes_total;
    double start_time, period_adj;
    double elapsed, delta, prev_delta;

    /* Buffer allocation is 2 x sample size for runtime mu-law conversion */
    char buffer[sample_buffer_size * 2];
    char *buf_ptr = &buffer[0];

    /* Open file for binary read access */
    file = fopen(file_name, "rb");
    if (!file)
    {
        printf("Unable to open file %s\n", file_name);
        return;
    }

    start_time = get_time();
    bytes_total = 0;

    period_adj = period_adj_s;
    delta = 0.0;
    prev_delta = 0.0;

    printf("Sending %s\n", file_name);

    /* Continue sending audio packets, until fread completes */
    for (;;)
    {
        int read, i;

        /* Runtime 16bit to 8bit mu-law conversion */
        if (0)
        {
            read = fread(buf_ptr, 1, sample_buffer_size * 2, file);

            if (read < 1)
                break;

            for (i = 0; i < read; i++)
            {
                short *pcm_val = (short *) &buf_ptr[i * 2];
                buffer[i] = linear2ulaw(*pcm_val);
            }

            read *= 0.5;
        }
        else
        {
            read = fread(buf_ptr, 1, sample_buffer_size, file);

            if (read < 1)
                break;
        }

        elapsed = bytes_total / (rhwparams.bytes_sample * rhwparams.channels)
                * sample_time;
        delta = (start_time + elapsed) - get_time();

        /* Send sample packet to each destination point */
        for (unsigned i = 0; i < destination_points.size(); i++)
        {
            bytes_sent = sendto(socket_desc, (const char *) buf_ptr, read, 0,
                    (struct sockaddr *) &destination_points[i].dest_sock_addr,
                    sizeof(destination_points[i].dest_sock_addr));

            if (verbose_debug)
            {
                printf("sent %i bytes to %s:%i\n", bytes_sent,
                        destination_points[i].dest_addr,
                        destination_points[i].dest_port);
            }

            if (bytes_sent == -1)
            {
                printf("sendto() failed.  errno=%i\n", errno);
                perror("sendto");
                exit(EXIT_FAILURE);
            }
        }

        bytes_total += bytes_sent;
        packet_cnt++;

        // Calculation of phase lock loop sleep period adjustments, so that
        // packets are sent at the correct rate wrt the sample frequency.
        // Large and then small adjustments are made to the sleep period,
        // to reach the nominal sleep period as quickly as possible.

        if (((delta > 0) && (delta > prev_delta))
                || ((delta > 0) && (delta > pal_chk)))
            period_sleep += period_sleep * period_adj;

        if (((delta < 0) && (delta < prev_delta))
                || ((delta < 0) && (delta < -pal_chk)))
            period_sleep -= period_sleep * period_adj;

        prev_delta = delta;

        if (verbose_debug)
        {
            printf("\nelapsed = %f\n", elapsed);
            printf("delta = %f\n", delta);
            printf("period_sleep = %4.10f\n", period_sleep);
            if (elapsed > 0)
                printf("packets per second = %f\n", packet_cnt / elapsed);
        }

        period_delay(period_sleep);
    }
}

static void create_socket()
{
    /* Create socket descriptor */
    if ((socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        fprintf(stderr, "Couldn't create socket descriptor\n");
        exit(EXIT_FAILURE);
    }

    /* Allow broadcast packets to be sent */
    int broadcast = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_BROADCAST, (char *) &broadcast,
            sizeof broadcast) == -1)
    {
        perror("setsockopt (SO_BROADCAST)");
        exit(EXIT_FAILURE);
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

        if (verbose_debug)
        {
            printf("  dest address: %s:%i\n", destination_points[i].dest_addr,
                    destination_points[i].dest_port);
        }
    }
}

static void print_usage()
{
    printf("\n");
    printf("Usage: ethersend [OPTION]...\n");
    printf("\n");
    printf("Stream audio packets across a LAN from an audio file");
    printf("\n");
    printf("   -f, the audio filename\n");
    printf("   -m n, audio file format\n");
    printf("      1: mu-law au fmt  (8000 hz,  8 bit, 1 channel)\n");
    printf("      2: VOIP  wav fmt (16000 hz, 16 bit, 1 channel)\n");
    printf("      3: Music wav fmt (22050 hz, 16 bit, 2 channel)\n");
    printf("   -d ip_addr:port, destination ip address and port\n");
    printf("   -h, show this help message\n");
    printf("\n");
    printf("Example:\n");
    printf("\n");
    printf("      ethersend -f sample.au -d 127.0.0.1:6502 ");
    printf("\n");
}

int main(int argc, char *argv[])
{
    char *filename = 0;

    /* Print usage if no options were given */
    if (argc < 2)
    {
        print_usage();
        exit(EXIT_FAILURE);
    }

    rhwparams.format = SND_PCM_FORMAT_MU_LAW;
    rhwparams.channels = 1;
    rhwparams.rate = 8000;
    rhwparams.bytes_sample = 1;
    sample_buffer_size = 256;

    /* Process command line options */
    while (argc > 1)
    {
        if (argv[1][0] == '-')
        {
            switch (argv[1][1])
            {

            case 'f':
                filename = &argv[1][3];
                break;

            case 'm':
                if (strcasecmp(&argv[1][3], "1") == 0)
                {
                    rhwparams.format = SND_PCM_FORMAT_MU_LAW;
                    rhwparams.channels = 1;
                    rhwparams.rate = 8000;
                    rhwparams.bytes_sample = 1;
                    sample_buffer_size = 256;
                }
                else if (strcasecmp(&argv[1][3], "2") == 0)
                {
                    rhwparams.format = SND_PCM_FORMAT_S16_LE;
                    rhwparams.channels = 1;
                    rhwparams.rate = 16000;
                    rhwparams.bytes_sample = 2;
                    sample_buffer_size = 1024;
                }
                else if (strcasecmp(&argv[1][3], "3") == 0)
                {
                    rhwparams.format = SND_PCM_FORMAT_S16_LE;
                    rhwparams.channels = 2;
                    rhwparams.rate = 22050;
                    rhwparams.bytes_sample = 2;
                    sample_buffer_size = 1024;
                }
                else
                {
                    printf("Unrecognized audio configuration mode %s\n",
                            &argv[1][3]);
                    exit(EXIT_FAILURE);
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
                exit(EXIT_SUCCESS);
                break;
            }
        }

        argv++;
        argc--;
    }

    if (destination_points.size() == 0)
    {
        print_usage();
        exit(EXIT_FAILURE);
    }

    create_socket();

    if (filename != 0)
        play(filename);
    else
    {
        print_usage();
        exit(EXIT_FAILURE);
    }

    return 0;
}

