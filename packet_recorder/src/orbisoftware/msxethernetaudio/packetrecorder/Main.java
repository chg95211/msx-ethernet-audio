/*
 *  MSX Ethernet Audio
 *
 *  Copyright (C) 2012 Harlan Murphy
 *  Orbis Software - orbisoftware@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package orbisoftware.msxethernetaudio.packetrecorder;

import jargs.gnu.CmdLineParser;

public class Main {

   private static void printUsage() {

      System.out.println("Usage: packet_recorder [OPTION]...");
      System.out
            .println("Record datagram packets based on specified options.");
      System.out.println();
      System.out.println("   -p, --port         port number");
      System.out.println("   -m, --multicast    multicast group");
      System.out.println("   -h, --help         show this help message");

   }

   /**
    * @param args
    *           the command line arguments
    */
   public static void main(String[] args) {

      ProcessDatagramThread processDatagramThread = new ProcessDatagramThread();
      CmdLineParser parser = new CmdLineParser();
      PacketRecorderConfig packetRecorderConfig;

      CmdLineParser.Option help = parser.addBooleanOption('h', "help");
      CmdLineParser.Option port = parser.addIntegerOption('p', "port");
      CmdLineParser.Option multicastGroup = parser.addStringOption('m',
            "multicast");

      Boolean helpValue;
      Integer portValue;
      String multicastGroupAddress;

      try {
         parser.parse(args);
      } catch (CmdLineParser.OptionException e) {
         System.out.println(e.getMessage());
         printUsage();
         System.exit(0);
      }

      helpValue = (Boolean) parser.getOptionValue(help);
      portValue = (Integer) parser.getOptionValue(port);
      multicastGroupAddress = (String) parser.getOptionValue(multicastGroup);

      if (helpValue != null) {
         printUsage();
         System.exit(0);
      }

      packetRecorderConfig = PacketRecorderConfig.getInstance();

      if (portValue != null)
         packetRecorderConfig.setPortNumber(portValue);

      if (multicastGroupAddress != null)
         packetRecorderConfig.setMulticastGroupAddress(multicastGroupAddress);

      System.out.println("  Listening on port: "
            + packetRecorderConfig.getPortNumber());

      if (packetRecorderConfig.getUseMulticast())
         System.out.println("    Multicast Group: "
               + packetRecorderConfig.getMulticastGroupAddress());

      System.out.println();

      processDatagramThread.start();

   }
}
