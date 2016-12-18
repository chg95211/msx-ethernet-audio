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

package orbisoftware.msxethernetaudio.packetplayer;

import java.beans.PropertyChangeSupport;
import java.io.DataInputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.List;
import java.util.ArrayList;

public class Manifest {

   private static Manifest instance = null;

   private List<ManifestEntry> manifestList = new ArrayList<ManifestEntry>();
   private long initialTimestamp = 0;
   private int numberPackets = 0;
   private RandomAccessFile packetContentRAF;
   private PropertyChangeSupport propertyChangeSupport;

   protected Manifest() {
      propertyChangeSupport = new PropertyChangeSupport(this);
   }

   public PropertyChangeSupport getPropertyChangeSupport() {
      return propertyChangeSupport;
   }

   public static Manifest getInstance() {
      if (instance == null) {
         instance = new Manifest();
      }
      return instance;
   }

   public void loadFile(String filename) {

      DataInputStream manifestInputStream;
      String packetContentFilename;

      /* Load the new manifest */
      try {

         manifestInputStream = new DataInputStream(
               new FileInputStream(filename));
         numberPackets = manifestInputStream.readInt();

         manifestList.clear();
         for (int i = 0; i < numberPackets; i++) {

            ManifestEntry manifestEntry = new ManifestEntry(
                  manifestInputStream.readLong(),
                  manifestInputStream.readInt(), manifestInputStream.readInt(),
                  manifestInputStream.readShort());

            // If this is the initial manifest entry, initialize the
            // initialTimestamp to make time relative to first packet.
            if (i == 0)
               initialTimestamp = manifestEntry.packetTimeStamp;

            manifestEntry.packetTimeStamp -= initialTimestamp;

            // Add new manifest entry
            manifestList.add(manifestEntry);
         }

         manifestInputStream.close();
      } catch (IOException E) {
         System.out.println("Exception occured when loading " + filename);
         numberPackets = 0;
         return;
      }

      /* Close existing DataInputStream for packet content if open */
      try {
         packetContentRAF.close();
      } catch (IOException E) {
      } catch (NullPointerException E) {
      }

      packetContentFilename = filename.replace(".man", ".bin");

      try {
         /* Open up the DataInputStream for packet content */
         packetContentRAF = new RandomAccessFile(packetContentFilename, "r");
      } catch (IOException E) {
         System.out.println("Exception occured when loading "
               + packetContentFilename);
         numberPackets = 0;
         return;
      }
   }

   public ManifestEntry getPacket(int packetNumber) {
      return manifestList.get(packetNumber);
   }

   public boolean getNextPacket(PacketEntry packetEntry) {

      PacketPlayerData packetPlayerData = PacketPlayerData.getInstance();
      PacketPlayerUI packetPlayerUI = PacketPlayerUI.getInstance();

      // Update currentPacketNumber
      int currentPacketNumber = packetPlayerData.getCurrentPacketNumber();
      ManifestEntry manifestEntry;

      // If manifest hasn't been loaded or we have played all the packets,
      // stop the player and return false.
      if ((numberPackets == 0) || (currentPacketNumber == numberPackets)) {

         packetPlayerUI.pushStopButton();
         return false;
      }

      manifestEntry = manifestList.get(currentPacketNumber);

      try {
         packetEntry.byteBuffer = new byte[manifestEntry.packetSize];
         packetContentRAF.seek(manifestEntry.packetFilePos);
         packetContentRAF.readFully(packetEntry.byteBuffer, 0,
               manifestEntry.packetSize);
         packetEntry.packetTimeDelta = manifestEntry.packetTimeDelta;
      } catch (IOException E) {
         System.out.println("Exception when accessing packet content.");
         return false;
      }

      return true;
   }

   public int getNumberPackets() {

      return numberPackets;
   }
}