package editor.map;

import javax.swing.*;
import java.awt.*;

public class MapPanel extends JPanel {

    static final int TILE = 32;
    static final int W = 10;
    static final int H = 9;

    int[][] map = new int[H][W];

    public MapPanel() {
        setBackground(Color.DARK_GRAY);
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);

        Graphics2D g2 = (Graphics2D) g;

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {

                int t = map[y][x];

                if (t == 0) g2.setColor(new Color(60, 180, 75));
                if (t == 1) g2.setColor(new Color(139, 69, 19));
                if (t == 2) g2.setColor(new Color(30, 144, 255));

                g2.fillRect(x * TILE, y * TILE, TILE, TILE);

                g2.setColor(Color.BLACK);
                g2.drawRect(x * TILE, y * TILE, TILE, TILE);
            }
        }
    }
}