package editor.mapsprite;

import javax.swing.*;
import java.awt.*;

public class MapSpritePanel extends JPanel {

    public MapSpritePanel() {
        setBackground(Color.BLACK);
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);

        g.setColor(Color.WHITE);
        g.setFont(new Font("Arial", Font.BOLD, 20));
        g.drawString("Map Sprite Editor (placeholder)", 50, 50);

        g.setColor(Color.GREEN);
        g.drawRect(50, 100, 64, 64);
        g.drawString("sprite slot", 50, 180);
    }
}