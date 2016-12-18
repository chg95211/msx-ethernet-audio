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

public class PacketRecorderConfig {

   private static PacketRecorderConfig instance = null;
   private int portNumber = 6502;
   private String multicastGroupAddress = null;
   private boolean useMulticast = false;

   public static PacketRecorderConfig getInstance() {
      if (instance == null) {
         instance = new PacketRecorderConfig();
      }
      return instance;
   }

   public void setPortNumber(int portNumber) {
      this.portNumber = portNumber;
   }

   public int getPortNumber() {
      return portNumber;
   }

   public void setMulticastGroupAddress(String multicastGroupAddress) {
      this.multicastGroupAddress = multicastGroupAddress;
      this.useMulticast = true;
   }

   public String getMulticastGroupAddress() {
      return multicastGroupAddress;
   }

   public boolean getUseMulticast() {
      return useMulticast;
   }
}
