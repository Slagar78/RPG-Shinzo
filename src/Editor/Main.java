package editor;

import editor.map.MapPanel;
import editor.mapsprite.MapSpritePanel;
import editor.portrait.PortraitPanel;

import javax.swing.*;

public class Main {

    public static void main(String[] args) {

        JFrame frame = new JFrame("SRPG Editor (Caravan-lite)");

        JTabbedPane tabs = new JTabbedPane();

        tabs.addTab("Map", new MapPanel());
        tabs.addTab("Map Sprites", new MapSpritePanel());
        tabs.addTab("Portraits", new PortraitPanel());

        frame.add(tabs);

        frame.setSize(900, 600);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setLocationRelativeTo(null);
        frame.setVisible(true);
    }
}