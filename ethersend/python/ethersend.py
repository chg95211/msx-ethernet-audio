#!/usr/bin/env python
#

# Audio Configuration (modify this section)
UDP_PORT         = 6502
IP_ADDR          = "127.0.0.1"
SAMPLE_BUF_SIZE  = 1378 
SAMPLE_HZ        = 44100.0
BYTES_SAMPLE     = 2 
CHANNELS         = 2 
DEBUG            = False 

# Presets (do not modify this section)
SAMPLE_TIME      = 1.0 / SAMPLE_HZ
PKTS_SECOND      = SAMPLE_HZ * BYTES_SAMPLE * CHANNELS / SAMPLE_BUF_SIZE
PERIOD_SLEEP     = 1.0 / PKTS_SECOND
PERIOD_ADJ_L     = 0.25;
PERIOD_ADJ_S     = 0.01;
PAL_CHK          = PERIOD_ADJ_L * PERIOD_SLEEP;

import socket, time, sys

def play(filename):
	
    f = open(filename, "rb")

    startTime = time.time()
    bytesTotal = 0
    break_cond = False
    
    period_sleep = PERIOD_SLEEP
    
    in_pal = False
    period_adj = PERIOD_ADJ_S
    delta = 0.0
    prev_delta = 0.0
    
    print "Playing %r" % filename
    while True:
       
        block = f.read(SAMPLE_BUF_SIZE)
        if not block:
            break

        elapsed = bytesTotal / (BYTES_SAMPLE * CHANNELS) * SAMPLE_TIME
        delta = (startTime + elapsed) - time.time()

        s.send(block)
        bytesTotal += SAMPLE_BUF_SIZE
        
        # Continuous sleep period adjustments, so that packets are sent
        # at the correct rate wrt the sample frequency.  Large and then
        # small adjustments are made to the sleep period, to reach the 
        # nominal sleep period as quickly as possible.
          
        if ((delta > 0 and delta > prev_delta) or \
            (delta > 0 and delta > PAL_CHK)):
	    period_sleep = period_sleep + (period_sleep * period_adj)

        if ((delta < 0 and delta < prev_delta) or \
            (delta < 0 and delta < -PAL_CHK)):
	    period_sleep = period_sleep - (period_sleep * period_adj)
	    
	prev_delta = delta
        
        if (DEBUG):
            print "elpased = " + str(elapsed)
            print "delta = " + str(delta)
            print "period_sleep = " + str(period_sleep)
            print "packets per second = " + str(PKTS_SECOND) + "\n"
            
        time.sleep(period_sleep)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect((IP_ADDR, UDP_PORT))

while True:
    for filename in sys.argv[1:]:
        play(filename)
