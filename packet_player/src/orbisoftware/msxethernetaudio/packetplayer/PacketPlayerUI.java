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

import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JTextField;
import javax.swing.JFileChooser;
import javax.swing.JSlider;
import javax.swing.UIManager;

public class PacketPlayerUI implements ActionListener, ChangeListener {

   private static PacketPlayerUI instance = null;

   private JTextField ipAddress = null;
   private JTextField port = null;
   private JButton fileSelectButton = null;
   private JButton startButton = null;
   private JButton stopButton = null;
   private String playbackDir = "playback_db";
   private String fileNameDefault = "default_db.man";
   private JTextField fileName = null;
   private JFileChooser fileChooser = null;
   private FileExtensionFilter filter = null;
   private JLabel currentPacketLabel = null;
   private JLabel elapsedTimeLabel = null;
   private JSlider packetSlider = null;
   private boolean ignoreChangeEvent = false;

   protected PacketPlayerUI() {

   };

   public static PacketPlayerUI getInstance() {
      if (instance == null) {
         instance = new PacketPlayerUI();
      }
      return instance;
   }

   private void addComponentsToPane(Container pane) {

      PacketPlayerData packetPlayerData = PacketPlayerData.getInstance();

      JLabel label;
      pane.setLayout(new GridBagLayout());
      GridBagConstraints c = new GridBagConstraints();
      c.weightx = 1.0;
      c.fill = GridBagConstraints.HORIZONTAL;

      label = new JLabel("IP Address:");
      c.gridx = 0;
      c.gridy = 0;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(label, c);

      ipAddress = new JTextField();
      ipAddress.setText(packetPlayerData.getIPAddress());
      c.gridx = 1;
      c.gridy = 0;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(ipAddress, c);

      label = new JLabel("Port:");
      c.gridx = 0;
      c.gridy = 1;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(label, c);

      port = new JTextField();
      port.setText(Integer.toString(packetPlayerData.getPort()));
      c.gridx = 1;
      c.gridy = 1;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(port, c);

      label = new JLabel("File Name:");
      c.gridx = 0;
      c.gridy = 2;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(label, c);

      fileSelectButton = new JButton("File Select");
      c.gridx = 1;
      c.gridy = 2;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(fileSelectButton, c);
      fileSelectButton.setEnabled(true);
      fileSelectButton.addActionListener(this);

      fileName = new JTextField();
      fileName.setText(fileNameDefault);

      fileName.setText(System.getProperty("user.dir")
            + System.getProperty("file.separator") + playbackDir
            + System.getProperty("file.separator") + fileNameDefault);

      c.gridx = 0;
      c.gridy = 3;
      c.gridwidth = 2;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(fileName, c);

      startButton = new JButton("Start");
      c.gridx = 0;
      c.gridy = 4;
      c.gridwidth = 1;
      c.insets = new Insets(20, 10, 10, 10);
      pane.add(startButton, c);
      startButton.setEnabled(true);
      startButton.addActionListener(this);

      stopButton = new JButton("Stop");
      c.gridx = 1;
      c.gridy = 4;
      c.insets = new Insets(20, 10, 10, 10);
      pane.add(stopButton, c);
      stopButton.setEnabled(false);
      stopButton.addActionListener(this);

      label = new JLabel("Current Packet:");
      c.gridx = 0;
      c.gridy = 5;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(label, c);

      currentPacketLabel = new JLabel("0");
      c.gridx = 1;
      c.gridy = 5;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(currentPacketLabel, c);

      label = new JLabel("Elapsed Time:");
      c.gridx = 0;
      c.gridy = 6;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(label, c);

      elapsedTimeLabel = new JLabel("0");
      c.gridx = 1;
      c.gridy = 6;
      c.insets = new Insets(20, 10, 0, 10);
      pane.add(elapsedTimeLabel, c);

      packetSlider = new JSlider();
      packetSlider.setPaintTicks(true);
      packetSlider.setPaintLabels(true);
      packetSlider.setValue(0);
      c.gridx = 0;
      c.gridy = 7;
      c.gridwidth = 2;
      c.insets = new Insets(20, 10, 10, 10);
      pane.add(packetSlider, c);
      packetSlider.addChangeListener(this);
   }

   public void createAndShowGUI() {
      // Create and set up the window.
      JFrame frame = new JFrame("Packet Player");
      frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
      filter = new FileExtensionFilter();

      // Make the file chooser readonly to disallow file renaming
      UIManager.put("FileChooser.readOnly", true);

      // Instantiate file chooser
      fileChooser = new JFileChooser(System.getProperty("user.dir")
            + System.getProperty("file.separator") + playbackDir);

      filter.addExtension("man");
      filter.setDescription("Packet Player Manifest Files");

      fileChooser.setFileFilter(filter);

      // Set up the content pane.
      addComponentsToPane(frame.getContentPane());

      // Load the playback database
      loadPlaybackDB();

      // Display the window.
      frame.setSize(300, 380);
      frame.setVisible(true);
   }

   public void actionPerformed(ActionEvent e) {

      PacketPlayerData packetPlayerData = PacketPlayerData.getInstance();

      if (e.getSource() == startButton) {

         try {
            packetPlayerData.setIPAddress(ipAddress.getText());
            packetPlayerData.setPort(Integer.parseInt(port.getText()));

            pushStartButton();
         } catch (Exception exception) {
            System.out.println("\nCould not open file");
         }
      } else if (e.getSource() == stopButton) {

         pushStopButton();

      } else if (e.getSource() == fileSelectButton) {

         int returnVal = fileChooser.showOpenDialog(null);

         if (returnVal == JFileChooser.APPROVE_OPTION) {

            fileName.setText(fileChooser.getSelectedFile().getPath());
            loadPlaybackDB();
         }
      }
   }

   private void loadPlaybackDB() {

      int numberPackets;

      Manifest manifest = Manifest.getInstance();

      manifest.loadFile(fileName.getText());

      numberPackets = manifest.getNumberPackets();

      if (numberPackets > 0) {

         // Increase numberPackets until a noneven boundary is encountered.
         // This keeps the right slider label from being omitted.
         while (numberPackets % 4 == 0)
            numberPackets++;

         packetSlider.setMinimum(1);
         packetSlider.setMaximum(numberPackets);
         packetSlider.setLabelTable(null);
         packetSlider.setMajorTickSpacing(numberPackets / 4);
      }
   }

   public void stateChanged(ChangeEvent ce) {

      Manifest manifest = Manifest.getInstance();
      ManifestEntry manifestEntry;
      PacketPlayerData packetPlayerData = PacketPlayerData.getInstance();

      if (ce.getSource() == packetSlider) {

         int currentPacketNumber = packetSlider.getValue() - 1;

         // The slider can exceed the number of packets, so must check
         if ((currentPacketNumber < manifest.getNumberPackets())
               && (!ignoreChangeEvent)) {

            pushStopButton();

            manifestEntry = manifest.getPacket(currentPacketNumber);

            currentPacketLabel.setText(Integer.toString(currentPacketNumber + 1));
            elapsedTimeLabel.setText(Long
                  .toString(manifestEntry.packetTimeStamp));

            packetPlayerData.setCurrentPacketNumber(currentPacketNumber);
         }
      } else if (ce.getSource() == SendDatagramThread.class) {

         int currentPacketNumber = packetPlayerData.getCurrentPacketNumber();

         manifestEntry = manifest.getPacket(currentPacketNumber);

         currentPacketLabel.setText(Integer.toString(currentPacketNumber + 1));
         elapsedTimeLabel.setText(Long.toString(manifestEntry.packetTimeStamp));

         // The setValue call will generate another change event, so set
         // the ignoreChangeEvent flag to true, since the SendDatagramThread
         // is the controller in this case (not the slider control).
         ignoreChangeEvent = true;
         packetSlider.setValue(currentPacketNumber);
      }

      ignoreChangeEvent = false;
   }

   private void pushStartButton() {

      PacketPlayerData packetPlayerData = PacketPlayerData.getInstance();

      packetPlayerData.setPlayerActive(true);

      startButton.setEnabled(false);
      stopButton.setEnabled(true);
   }

   public void pushStopButton() {

      PacketPlayerData packetPlayerData = PacketPlayerData.getInstance();

      packetPlayerData.setPlayerActive(false);

      startButton.setEnabled(true);
      stopButton.setEnabled(false);

      // Interrupt SendDatagramThread so that it will stop sleeping
      SendDatagramThread.getInstance().interrupt();
   }
}
