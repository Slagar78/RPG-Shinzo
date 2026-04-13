package editor.portrait;

import javax.imageio.ImageIO;
import javax.swing.*;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.File;

public class PortraitEditor {

    public static void main(String[] args) {

        JFrame frame = new JFrame("Portrait Editor");

        PortraitPanel panel = new PortraitPanel();

        JButton loadBtn = new JButton("Load PNG");

        loadBtn.addActionListener(e -> {
            try {
                JFileChooser chooser = new JFileChooser();

                if (chooser.showOpenDialog(null) == JFileChooser.APPROVE_OPTION) {

                    File file = chooser.getSelectedFile();
                    BufferedImage img = ImageIO.read(file);

                    Portrait p = PortraitLoader.load(img);
                    panel.setPortrait(p);
                }

            } catch (Exception ex) {
                ex.printStackTrace();
            }
        });

        JPanel top = new JPanel();
        top.add(loadBtn);

        frame.setLayout(new BorderLayout());
        frame.add(top, BorderLayout.NORTH);
        frame.add(panel, BorderLayout.CENTER);

        frame.setSize(500, 300);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setLocationRelativeTo(null);
        frame.setVisible(true);
    }
}