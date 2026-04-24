require 'gosu'
require 'json'

class MapEditor < Gosu::Window
  MAP_W = 12
  MAP_H = 12
  TILE_SIZE = 48
  TILESET_COLS = 12
  PALETTE_WIDTH = TILESET_COLS * TILE_SIZE + 20

  MODE_ROTATE = 0
  MODE_MIRROR_H = 1
  MODE_MIRROR_V = 2

  def initialize
    super(PALETTE_WIDTH + MAP_W * TILE_SIZE + 20, MAP_H * TILE_SIZE + 150)
    self.caption = "Map Editor - Right click: rotate/mirror (buttons on top right)"

    @font = Gosu::Font.new(16)
    @small_font = Gosu::Font.new(14)

    # Данные карты
    @map = Array.new(MAP_W) { Array.new(MAP_H, 0) }
    @map_rot = Array.new(MAP_W) { Array.new(MAP_H, 0) }
    @map_mirror_x = Array.new(MAP_W) { Array.new(MAP_H, false) }
    @map_mirror_y = Array.new(MAP_W) { Array.new(MAP_H, false) }
    load_map

    # Тайлсет
    if File.exist?("assets/tilesets/tileset.png")
      @tiles = Gosu::Image.load_tiles("assets/tilesets/tileset.png", TILE_SIZE, TILE_SIZE)
      puts "Loaded #{@tiles.size} tiles"
    else
      @tiles = []
      puts "Tileset not found, using colored squares"
    end

    @selected_tile = 0
    @palette_scroll = 0
    @visible_tiles_rows = (height - 160) / TILE_SIZE   # оставляем место для кнопок

    @right_click_mode = MODE_ROTATE

    # Координаты кнопок (верхний правый угол)
    @btn_rotate_rect = nil
    @btn_h_rect = nil
    @btn_v_rect = nil
  end

  def load_map
    return unless File.exist?("data/map.json")
    data = JSON.parse(File.read("data/map.json"))
    @map = data["tiles"] if data["tiles"]
    @map_rot = data["tiles_rot"] if data["tiles_rot"]
    @map_mirror_x = data["tiles_mirror_x"] if data["tiles_mirror_x"]
    @map_mirror_y = data["tiles_mirror_y"] if data["tiles_mirror_y"]
    @map_rot ||= Array.new(MAP_W) { Array.new(MAP_H, 0) }
    @map_mirror_x ||= Array.new(MAP_W) { Array.new(MAP_H, false) }
    @map_mirror_y ||= Array.new(MAP_W) { Array.new(MAP_H, false) }
  end

  def save_map
    data = {
      tiles: @map,
      tiles_rot: @map_rot,
      tiles_mirror_x: @map_mirror_x,
      tiles_mirror_y: @map_mirror_y
    }
    File.write("data/map.json", JSON.pretty_generate(data))
    puts "Map saved"
  end

  def draw
    Gosu.draw_rect(0, 0, width, height, Gosu::Color::GRAY, -1)
    draw_tileset_palette
    draw_map
    draw_ui
    draw_mode_buttons
  end

  def draw_tileset_palette
    return if @tiles.empty?
    total_rows = (@tiles.size + TILESET_COLS - 1) / TILESET_COLS
    start_row = @palette_scroll
    end_row = [start_row + @visible_tiles_rows, total_rows].min

    (start_row...end_row).each do |row|
      (0...TILESET_COLS).each do |col|
        idx = row * TILESET_COLS + col
        next if idx >= @tiles.size
        x = 10 + col * TILE_SIZE
        y = 60 + (row - start_row) * TILE_SIZE   # отодвинул ниже
        @tiles[idx].draw(x, y, 0)

        if idx == @selected_tile
          alpha = (Gosu.milliseconds / 100) % 2 == 0 ? 150 : 0
          Gosu.draw_rect(x, y, TILE_SIZE, TILE_SIZE, Gosu::Color.new(alpha, 255, 255, 255), 1)
        end
      end
    end
  end

  def draw_map
    map_x = PALETTE_WIDTH
    (0...MAP_W).each do |x|
      (0...MAP_H).each do |y|
        px = map_x + x * TILE_SIZE
        py = y * TILE_SIZE
        tile_id = @map[x][y]
        rot = @map_rot[x][y] % 4
        mirror_x = @map_mirror_x[x][y] ? -1.0 : 1.0
        mirror_y = @map_mirror_y[x][y] ? -1.0 : 1.0
        angle = rot * 90

        if @tiles[tile_id]
          @tiles[tile_id].draw_rot(px + TILE_SIZE/2, py + TILE_SIZE/2, 0, angle, 0.5, 0.5, mirror_x, mirror_y)
        else
          Gosu.draw_rect(px, py, TILE_SIZE, TILE_SIZE, Gosu::Color::GREEN, 0)
        end
        Gosu.draw_rect(px, py, TILE_SIZE, 1, Gosu::Color::BLACK, 1)
        Gosu.draw_rect(px, py, 1, TILE_SIZE, Gosu::Color::BLACK, 1)
      end
    end
  end

  def draw_mode_buttons
    # Три кнопки в правой части верхней панели (под заголовком)
    btn_width = 100
    btn_height = 30
    spacing = 10
    start_x = width - (btn_width * 3 + spacing * 2) - 10

    # Кнопка Rotate
    @btn_rotate_rect = [start_x, 10, btn_width, btn_height]
    color_rotate = (@right_click_mode == MODE_ROTATE) ? Gosu::Color::GREEN : Gosu::Color::GRAY
    Gosu.draw_rect(start_x, 10, btn_width, btn_height, color_rotate, 1)
    @font.draw_text("Rotate", start_x + 25, 18, 2)

    # Кнопка Mirror H
    @btn_h_rect = [start_x + btn_width + spacing, 10, btn_width, btn_height]
    color_h = (@right_click_mode == MODE_MIRROR_H) ? Gosu::Color::GREEN : Gosu::Color::GRAY
    Gosu.draw_rect(start_x + btn_width + spacing, 10, btn_width, btn_height, color_h, 1)
    @font.draw_text("Mirror H", start_x + btn_width + spacing + 15, 18, 2)

    # Кнопка Mirror V
    @btn_v_rect = [start_x + 2 * (btn_width + spacing), 10, btn_width, btn_height]
    color_v = (@right_click_mode == MODE_MIRROR_V) ? Gosu::Color::GREEN : Gosu::Color::GRAY
    Gosu.draw_rect(start_x + 2 * (btn_width + spacing), 10, btn_width, btn_height, color_v, 1)
    @font.draw_text("Mirror V", start_x + 2 * (btn_width + spacing) + 15, 18, 2)
  end

  def draw_ui
    # Кнопка Save
    save_x = PALETTE_WIDTH + MAP_W * TILE_SIZE - 80
    save_y = MAP_H * TILE_SIZE + 10
    Gosu.draw_rect(save_x, save_y, 70, 30, Gosu::Color::GRAY, 1)
    @font.draw_text("Save", save_x + 15, save_y + 8, 2)

    @font.draw_text("Left click (hold): draw tile", 10, 10, 2)
    @font.draw_text("Right click: apply transformation (select above)", 10, 35, 2)
    @font.draw_text("Scroll wheel: scroll tileset", 10, height - 25, 2)
  end

  def update
    if Gosu.button_down?(Gosu::MsLeft)
      handle_left_click
    end
  end

  def button_down(id)
    case id
    when Gosu::MsRight
      handle_right_click
    when Gosu::MsLeft
      # Проверка клика по кнопкам режимов (приоритет над рисованием)
      check_mode_buttons
    when Gosu::MsWheelUp
      @palette_scroll -= 1 if @palette_scroll > 0
    when Gosu::MsWheelDown
      total_rows = (@tiles.size + TILESET_COLS - 1) / TILESET_COLS
      max_scroll = [0, total_rows - @visible_tiles_rows].max
      @palette_scroll += 1 if @palette_scroll < max_scroll
    else
      # клавиши R, H, V для смены режима
      if id == Gosu::KB_R
        @right_click_mode = MODE_ROTATE
      elsif id == Gosu::KB_H
        @right_click_mode = MODE_MIRROR_H
      elsif id == Gosu::KB_V
        @right_click_mode = MODE_MIRROR_V
      end
    end
  end

  def check_mode_buttons
    mx, my = mouse_x, mouse_y
    # Кнопка Rotate
    if @btn_rotate_rect && mx.between?(@btn_rotate_rect[0], @btn_rotate_rect[0] + @btn_rotate_rect[2]) &&
       my.between?(@btn_rotate_rect[1], @btn_rotate_rect[1] + @btn_rotate_rect[3])
      @right_click_mode = MODE_ROTATE
      return
    end
    # Кнопка Mirror H
    if @btn_h_rect && mx.between?(@btn_h_rect[0], @btn_h_rect[0] + @btn_h_rect[2]) &&
       my.between?(@btn_h_rect[1], @btn_h_rect[1] + @btn_h_rect[3])
      @right_click_mode = MODE_MIRROR_H
      return
    end
    # Кнопка Mirror V
    if @btn_v_rect && mx.between?(@btn_v_rect[0], @btn_v_rect[0] + @btn_v_rect[2]) &&
       my.between?(@btn_v_rect[1], @btn_v_rect[1] + @btn_v_rect[3])
      @right_click_mode = MODE_MIRROR_V
      return
    end
  end

  def handle_left_click
    mx, my = mouse_x, mouse_y

    # Выбор тайла из палитры (палитра теперь сдвинута вниз, y начинается с 60)
    if mx >= 10 && mx < 10 + TILESET_COLS * TILE_SIZE && my >= 60
      col = ((mx - 10) / TILE_SIZE).to_i
      row = ((my - 60) / TILE_SIZE).to_i + @palette_scroll
      idx = row * TILESET_COLS + col
      if idx >= 0 && idx < @tiles.size
        @selected_tile = idx
      end
      return
    end

    # Рисование на карте
    map_x_start = PALETTE_WIDTH
    if mx >= map_x_start && mx < map_x_start + MAP_W * TILE_SIZE && my >= 0 && my < MAP_H * TILE_SIZE
      gx = ((mx - map_x_start) / TILE_SIZE).to_i
      gy = (my / TILE_SIZE).to_i
      @map[gx][gy] = @selected_tile
      @map_rot[gx][gy] = 0
      @map_mirror_x[gx][gy] = false
      @map_mirror_y[gx][gy] = false
    end

    # Кнопка Save
    save_x = PALETTE_WIDTH + MAP_W * TILE_SIZE - 80
    save_y = MAP_H * TILE_SIZE + 10
    if mx.between?(save_x, save_x + 70) && my.between?(save_y, save_y + 30)
      save_map
    end
  end

  def handle_right_click
    mx, my = mouse_x, mouse_y
    map_x_start = PALETTE_WIDTH
    if mx >= map_x_start && mx < map_x_start + MAP_W * TILE_SIZE && my >= 0 && my < MAP_H * TILE_SIZE
      gx = ((mx - map_x_start) / TILE_SIZE).to_i
      gy = (my / TILE_SIZE).to_i

      case @right_click_mode
      when MODE_ROTATE
        @map_rot[gx][gy] = (@map_rot[gx][gy] + 1) % 4
      when MODE_MIRROR_H
        @map_mirror_x[gx][gy] = !@map_mirror_x[gx][gy]
      when MODE_MIRROR_V
        @map_mirror_y[gx][gy] = !@map_mirror_y[gx][gy]
      end
    end
  end
end

MapEditor.new.show