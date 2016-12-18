MSX Ethernet Audio - Orbis Software
-----------------------------------
Simple network audio streaming across a LAN.

License
-------
MSX Ethernet Audio is licensed under the GNU GPL 3.0 with some files licensed
under the MIT License.

Source code is available from the github repository at the project's
homepage: https://github.com/harlan3/msx-ethernet-audio.git

Setup
-----
Alsa audio is used for all sound processing.
gcc and make are required for ethersend and etherplay.
Ant and the Java Runtime are required for packet_player and packet_recorder.
Python is required to generate the playback database used by packet_player.

ethersend
---------
The ethersend application processes an audio mu-law or PCM wav file, by 
breaking it into chunks suitable for an audio DSP, and then sending the data
across a LAN for playback by etherplay.

Use the -h option on this tool to view usage instructions.

etherplay
---------
The etherplay application provides the capability to playback audio packets
streaming across a LAN.  Etherplay can optionally playback an audio mu-law or 
PCM file.

Use the -h option on this tool to view usage instructions.

ethermic/etherptt
-----------------
The ethermic and push-to-talk applications sample data coming from a 
microphone connected to an audio input as mu-law or PCM data, and then 
send the sample data across a LAN for playback by etherplay.

Use the -h option on this tool to view usage instructions.

packet_recorder
---------------
The packet_recorder application records UDP packets received on a socket port, 
and then dumps each packet containing a timestamp and the hex data contained 
within the packet which can be redirected to a text file.  This file is used 
for the playback of a recorded audio session by packet_player.  The 
udp_reflector tool can be used in combination with packet_recorder and 
etherplay, to support simultaneous audio session recording and playback.

Use the -h option on this tool to view usage instructions.

packet_player
-------------
The packet_player application provides the capability to playback a previously
recorded audio session.  The application uses the create_playback_db python 
script to generate a manifest file and a binary database of the data that was 
captured during the packet_recorder session.

Use case 1 - Audio playback of a mu-law file across the LAN
-----------------------------------------------------------
   1)  Start etherplay
       cd msx-ethernet-audio/etherplay/Debug
       etherplay -m 1 -p 6502

   2)  Start playback of mu-law file or other source of audio packets
       cd msx-ethernet-audio/ethersend/Debug
       ./ethersend -m 1 -f ../../audio_samples/sample.au -d 127.0.0.1:6502

Use case 2 - Audio playback of CD quality audio from microphone across the LAN
------------------------------------------------------------------------------
   1)  Start etherplay
       cd msx-ethernet-audio/etherplay/Debug
       etherplay -m 3 -p 6502

   2)  Start ethermic 
       cd msx-ethernet-audio/ethermic/Debug
       ./ethermic -m 3 -d 127.0.0.1:6502
       
Use case 3 - Session recording of audio sample packets sent across the LAN
--------------------------------------------------------------------------
   1)  Start packet_recorder with defaults, redirecting to output file
       cd msx-ethernet-audio/packet_recorder
       java -jar packet_recorder.jar -p 6502 > audio_capture.txt

   2)  Start playback of mu-law file or other source for audio packets
       cd msx-ethernet-audio/ethersend/Debug
       ./ethersend -m 1 -f ../../audio_samples/sample.au -d 127.0.0.1:6502
   
   3)  Copy file over to packet_player for future conversion and playback
       cp msx-ethernet-audio/packet_recorder/audio_capture.txt \
          msx-ethernet-audio/packet_player/playback_db/

Use case 4 - Playback of a saved audio session across the LAN
-------------------------------------------------------------
   1)  Use the create_playback_db script to generate manifest file and binary
       database, from the previously saved audio session in Use case 2.
       cd msx-ethernet-audio/packet_player/playback_db
       python create_playback_db audio_capture.txt
   2)  Start etherplay
       cd msx-ethernet-audio/etherplay/Debug
       etherplay -m 1 -p 6502
   3)  Start packet_player, selecting audio_capture.man manifest for playback
       java -jar packet_player.jar
   4)  Click start to begin playback

