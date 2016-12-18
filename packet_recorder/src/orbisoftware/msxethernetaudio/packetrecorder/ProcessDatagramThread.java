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

import java.net.*;
import java.beans.*;

import java.util.List;
import java.util.LinkedList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import java.beans.PropertyChangeListener;

public class ProcessDatagramThread extends Thread implements
      PropertyChangeListener {

   private BlockingQueue<DatagramPacket> queue = new LinkedBlockingQueue<DatagramPacket>();

   public void propertyChange(PropertyChangeEvent evt) {

      byte[] buffer = new byte[((DatagramPacket) evt.getNewValue()).getLength()];

      if (evt.getPropertyName().toString().equals("datagramReceived")) {

         synchronized (queue) {

            // Make a copy of the datagram packet, as the referenced object
            // is owned by the other thread.
            System.arraycopy(((DatagramPacket) evt.getNewValue()).getData(), 0,
                  buffer, 0, buffer.length);

            DatagramPacket receivedPacket = new DatagramPacket(buffer,
                  buffer.length);

            queue.add(receivedPacket);
            queue.notify();
         }
      }
   }

   public void run() {

      ReceiveDatagramThread receiveDatagramThread = new ReceiveDatagramThread();
      int packetCounter = 0;

      receiveDatagramThread.getPropertyChangeSupport()
            .addPropertyChangeListener(this);
      receiveDatagramThread.start();

      while (true) {

         List<DatagramPacket> datagramList = new LinkedList<DatagramPacket>();

         try {

            // Block until notified that more datagrams packets have been
            // received or a timeout occurs.
            synchronized (queue) {
               queue.wait(20);
               queue.drainTo(datagramList);
            }
         } catch (InterruptedException e) {
         }

         for (DatagramPacket packet : datagramList) {

            packetCounter++;
            Default.processPacket(packet, packetCounter);
         }
      }
   }
}
