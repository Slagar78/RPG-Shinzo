# map_editor.rb
require 'gosu'
require 'json'

class MapEditor < Gosu::Window
  TILE_SIZE = 48
  TILESET_COLS = 12
  PALETTE_WIDTH = TILESET_COLS * TILE_SIZE + 20
  MAP_AREA_X = PALETTE_WIDTH + 10
  DEFAULT_WIDTH = 20
  DEFAULT_HEIGHT = 15

  # Кнопки (верхняя панель)
  BTN_A_X      = 10
  BTN_A_Y      = 10
  BTN_A_W      = 60
  BTN_A_H      = 30
  BTN_C_X      = 80
  BTN_C_Y      = 10
  BTN_C_W      = 60
  BTN_C_H      = 30
  BTN_SAVE_W   = 70
  BTN_SAVE_H   = 30

  # Кнопки действий с картами (правее, вертикально)
  ACTION_BTN_X = 150
  ACTION_BTN_Y = 10
  ACTION_BTN_W = 80
  ACTION_BTN_H = 30
  ACTION_BTN_GAP = 10
  ACTIONS = [
    { label: "New",   method: :create_map_dialog },
    { label: "Del",   method: :delete_current },
    { label: "Rename",method: :rename_map_dialog },
    { label: "Resize",method: :resize_map_dialog }
  ]

  # Режимы трансформации
  TRANS_X_OFFSET = 200
  TRANS_W     = 40
  TRANS_H     = 30
  TRANS_LABELS = ["Rot", "MirH", "MirV"]
  TRANS_MODES  = [:rotate, :mirror_h, :mirror_v]

  # Типы тайлов (иконки)
  TYPE_ICON_SIZE = 32
  TYPE_ICONS_Y   = 60
  TYPE_ICONS_START_X = 10
  TYPE_ICONS_GAP = 40

  # Список карт
  MAP_LIST_START_X = 10
  MAP_LIST_START_Y = 60
  MAP_LIST_ITEM_H = 25
  MAP_LIST_W = 120

  # Палитра тайлов
  PALETTE_START_X = 10
  PALETTE_START_Y = 60 + (DEFAULT_HEIGHT * MAP_LIST_ITEM_H) + 50
  PALETTE_ROW_H = TILE_SIZE

  def initialize
    super(MAP_AREA_X + DEFAULT_WIDTH * TILE_SIZE + 50, 800)
    self.caption = "Map Editor"

    @font = Gosu::Font.new(16)

    @maps = []
    @current_index = 0
    @camera = { x: 0.0, y: 0.0, zoom: 1.0 }
    @palette_scroll = 0
    @selected_tile = 0
    @mode = :draw
    @current_type = 0
    @right_click_mode = :rotate
    @drag_x = nil
    @drag_y = nil

    @tileset_path = "assets/tilesets/tileset.png"
    @tiles = []
    @tile_types = []
    @type_icons = []

    load_maps
    load_tileset
    load_tile_types
    load_type_icons
  end

  # ========== ЗАГРУЗКА / СОХРАНЕНИЕ КАРТ ==========
  def load_maps
    Dir.mkdir("data/maps") unless Dir.exist?("data/maps")
    @maps = Dir["data/maps/*.json"].map { |p| JSON.parse(File.read(p)) rescue nil }.compact
    create_map("map00", DEFAULT_WIDTH, DEFAULT_HEIGHT) if @maps.empty?
  end

  def save_current
    File.write("data/maps/#{current_map['name']}.json", JSON.pretty_generate(current_map))
    puts "Saved #{current_map['name']}"
  end

  def create_map(name, w, h)
    base = name
    counter = 0
    while @maps.any? { |m| m['name'] == name }
      counter += 1
      name = "#{base}#{counter}"
    end
    map = {
      "name" => name,
      "width" => w,
      "height" => h,
      "tileset" => @tileset_path,
      "tiles" => Array.new(w) { Array.new(h, 0) },
      "rot" => Array.new(w) { Array.new(h, 0) },
      "mirror_x" => Array.new(w) { Array.new(h, false) },
      "mirror_y" => Array.new(w) { Array.new(h, false) }
    }
    @maps << map
    @current_index = @maps.size - 1
    save_current
    load_tileset
    load_tile_types
  end

  def delete_current
    return if @maps.size <= 1
    File.delete("data/maps/#{current_map['name']}.json")
    @maps.delete_at(@current_index)
    @current_index = 0 if @current_index >= @maps.size
    load_tileset
    load_tile_types
  end

  def rename_current_map(new_name)
    old_path = "data/maps/#{current_map['name']}.json"
    new_path = "data/maps/#{new_name}.json"
    if File.exist?(old_path)
      File.rename(old_path, new_path)
    end
    current_map['name'] = new_name
    save_current
    load_maps
    find_current_index_by_name(new_name)
  end

  def resize_current_map(new_w, new_h)
    map = current_map
    old_w = map['width']
    old_h = map['height']
    new_tiles = Array.new(new_w) { Array.new(new_h, 0) }
    new_rot = Array.new(new_w) { Array.new(new_h, 0) }
    new_mirror_x = Array.new(new_w) { Array.new(new_h, false) }
    new_mirror_y = Array.new(new_w) { Array.new(new_h, false) }

    (0...[old_w, new_w].min).each do |x|
      (0...[old_h, new_h].min).each do |y|
        new_tiles[x][y] = map['tiles'][x][y]
        new_rot[x][y] = map['rot'][x][y]
        new_mirror_x[x][y] = map['mirror_x'][x][y]
        new_mirror_y[x][y] = map['mirror_y'][x][y]
      end
    end
    map['tiles'] = new_tiles
    map['rot'] = new_rot
    map['mirror_x'] = new_mirror_x
    map['mirror_y'] = new_mirror_y
    map['width'] = new_w
    map['height'] = new_h
    save_current
  end

  def find_current_index_by_name(name)
    @current_index = @maps.index { |m| m['name'] == name } || 0
  end

  def current_map
    @maps[@current_index]
  end

  # ========== ТАЙЛСЕТ ==========
  def load_tileset
    if File.exist?(@tileset_path)
      @tiles = Gosu::Image.load_tiles(@tileset_path, TILE_SIZE, TILE_SIZE)
    else
      @tiles = []
    end
  end

  # ========== ТИПЫ ТАЙЛОВ ==========
  def load_tile_types
    @tile_types = Array.new(@tiles.size, 0)
    if File.exist?("data/tile_types.json")
      types = JSON.parse(File.read("data/tile_types.json"))
      @tile_types = types
    end
    @tile_types += Array.new(@tiles.size - @tile_types.size, 0) if @tile_types.size < @tiles.size
  end

  def save_tile_types
    File.write("data/tile_types.json", JSON.pretty_generate(@tile_types))
  end

  def load_type_icons
    @type_icons = []
    ["passable.png", "block.png", "slow.png", "under.png"].each do |icon|
      path = "assets/icons/#{icon}"
      if File.exist?(path)
        @type_icons << Gosu::Image.new(path)
      else
        @type_icons << Gosu::Image.new(32, 32, color: Gosu::Color::RED)
      end
    end
  end

  # ========== ДИАЛОГИ (через консоль) ==========
  def create_map_dialog
    puts "Enter map name:"
    name = gets.chomp
    puts "Enter width:"
    w = gets.to_i
    puts "Enter height:"
    h = gets.to_i
    create_map(name, w, h)
  end

  def rename_map_dialog
    puts "Enter new name for map '#{current_map['name']}':"
    new_name = gets.chomp
    rename_current_map(new_name)
  end

  def resize_map_dialog
    puts "Enter new width:"
    w = gets.to_i
    puts "Enter new height:"
    h = gets.to_i
    resize_current_map(w, h)
  end

  # ========== ОТРИСОВКА ==========
  def draw
    Gosu.draw_rect(0, 0, width, height, Gosu::Color::GRAY, -1)
    draw_map_list
    draw_palette
    draw_map
    draw_ui
    draw_toolbar
  end

  def draw_map_list
    x = MAP_LIST_START_X
    y = MAP_LIST_START_Y
    # Рисуем рамку списка
    Gosu.draw_rect(x-2, y-2, MAP_LIST_W+4, @maps.size * MAP_LIST_ITEM_H + 30, Gosu::Color::BLACK, 0)
    Gosu.draw_rect(x, y, MAP_LIST_W, @maps.size * MAP_LIST_ITEM_H + 26, Gosu::Color::GRAY, 0)  # было DARK_GRAY -> GRAY
    @font.draw_text("MAPS", x+5, y, 2)
    @maps.each_with_index do |map, i|
      y += MAP_LIST_ITEM_H
      color = (i == @current_index) ? Gosu::Color::GREEN : Gosu::Color::WHITE
      @font.draw_text(map['name'], x+5, y+2, 2, 1, 1, color)
    end
  end

  def draw_palette
    return if @tiles.empty?
    x = PALETTE_START_X
    y = PALETTE_START_Y
    visible_rows = (height - y) / TILE_SIZE
    start_row = @palette_scroll
    total_rows = (@tiles.size + TILESET_COLS - 1) / TILESET_COLS
    end_row = [start_row + visible_rows, total_rows].min

    (start_row...end_row).each do |row|
      (0...TILESET_COLS).each do |col|
        idx = row * TILESET_COLS + col
        next if idx >= @tiles.size
        px = x + col * TILE_SIZE
        py = y + (row - start_row) * TILE_SIZE
        @tiles[idx].draw(px, py, 0)

        # Рисуем сетку в палитре
        Gosu.draw_rect(px, py, TILE_SIZE, 1, Gosu::Color::BLACK, 1)
        Gosu.draw_rect(px, py, 1, TILE_SIZE, Gosu::Color::BLACK, 1)

        if idx == @selected_tile
          alpha = (Gosu.milliseconds / 100) % 2 == 0 ? 150 : 0
          Gosu.draw_rect(px, py, TILE_SIZE, TILE_SIZE, Gosu::Color.new(alpha, 255, 255, 255), 1)
        end

        if @mode == :assign
          type = @tile_types[idx]
          @type_icons[type].draw(px + 2, py + 2, 1) if type < @type_icons.size
        end
      end
    end
  end

  def draw_map
    map = current_map
    return unless map
    (0...map['width']).each do |x|
      (0...map['height']).each do |y|
        px = MAP_AREA_X + x * TILE_SIZE * @camera[:zoom] - @camera[:x]
        py = y * TILE_SIZE * @camera[:zoom] - @camera[:y]
        next if px + TILE_SIZE*@camera[:zoom] < 0 || px > width ||
                py + TILE_SIZE*@camera[:zoom] < 0 || py > height

        tile_id = map['tiles'][x][y]
        rot = map['rot'][x][y] % 4
        mirror_x = map['mirror_x'][x][y] ? -1.0 : 1.0
        mirror_y = map['mirror_y'][x][y] ? -1.0 : 1.0
        angle = rot * 90

        if @tiles[tile_id]
          @tiles[tile_id].draw_rot(px + (TILE_SIZE*@camera[:zoom])/2,
                                   py + (TILE_SIZE*@camera[:zoom])/2,
                                   0, angle, 0.5, 0.5, mirror_x, mirror_y)
        else
          Gosu.draw_rect(px, py, TILE_SIZE*@camera[:zoom], TILE_SIZE*@camera[:zoom],
                         Gosu::Color::GREEN, 0)
        end
        # Сетка на карте не рисуется
      end
    end
  end

  def draw_ui
    @font.draw_text("Mode: #{@mode == :draw ? 'DRAW (A)' : 'ASSIGN (C)'}", MAP_AREA_X + 10, 10, 2)
    @font.draw_text("Zoom: #{(@camera[:zoom]*100).to_i}%", width - 120, 10, 2)
    @font.draw_text("Left: draw/assign | Right: transform | Wheel: zoom | Ctrl+Right drag: pan",
                    10, height - 20, 2)
  end

  def draw_toolbar
    btn_y = 10
    # A / C
    Gosu.draw_rect(BTN_A_X, btn_y, BTN_A_W, BTN_A_H,
                   @mode == :draw ? Gosu::Color::GREEN : Gosu::Color::GRAY, 1)
    @font.draw_text("A", BTN_A_X + 20, btn_y + 8, 2)
    Gosu.draw_rect(BTN_C_X, btn_y, BTN_C_W, BTN_C_H,
                   @mode == :assign ? Gosu::Color::GREEN : Gosu::Color::GRAY, 1)
    @font.draw_text("C", BTN_C_X + 20, btn_y + 8, 2)

    # Save
    btn_save_x = width - BTN_SAVE_W - 10
    Gosu.draw_rect(btn_save_x, btn_y, BTN_SAVE_W, BTN_SAVE_H, Gosu::Color::GRAY, 1)
    @font.draw_text("Save", btn_save_x + 15, btn_y + 8, 2)

    # Кнопки действий с картами
    ACTIONS.each_with_index do |action, idx|
      x = ACTION_BTN_X
      y = ACTION_BTN_Y + idx * (ACTION_BTN_H + ACTION_BTN_GAP)
      Gosu.draw_rect(x, y, ACTION_BTN_W, ACTION_BTN_H, Gosu::Color::GRAY, 1)
      @font.draw_text(action[:label], x + 10, y + 8, 2)
    end

    # Трансформации
    trans_start_x = width - TRANS_X_OFFSET
    TRANS_MODES.each_with_index do |mode, i|
      x = trans_start_x + i * (TRANS_W + 5)
      color = (@right_click_mode == mode) ? Gosu::Color::GREEN : Gosu::Color::GRAY
      Gosu.draw_rect(x, btn_y, TRANS_W, TRANS_H, color, 1)
      @font.draw_text(TRANS_LABELS[i], x + 5, btn_y + 8, 2)
    end

    # Типы тайлов (в режиме assign)
    if @mode == :assign
      y = TYPE_ICONS_Y
      @type_icons.each_with_index do |icon, i|
        x = TYPE_ICONS_START_X + i * TYPE_ICONS_GAP
        if @current_type == i
          Gosu.draw_rect(x - 2, y - 2, TYPE_ICON_SIZE + 4, TYPE_ICON_SIZE + 4,
                         Gosu::Color::GREEN, 1)
        end
        icon.draw(x, y, 1)
      end
    end
  end

  # ========== ОБРАБОТКА ВВОДА ==========
  def update
    handle_mouse
    handle_keyboard
  end

  def handle_mouse
    if Gosu.button_down?(Gosu::MsLeft)
      handle_left_click
    end
    if Gosu.button_down?(Gosu::MsRight) && Gosu.button_down?(Gosu::KB_LEFT_CONTROL)
      dx = mouse_x - @drag_x if @drag_x
      dy = mouse_y - @drag_y if @drag_y
      @camera[:x] -= dx if dx
      @camera[:y] -= dy if dy
      @drag_x = mouse_x
      @drag_y = mouse_y
    else
      @drag_x = nil
      @drag_y = nil
    end
  end

  def handle_keyboard
    if Gosu.button_down?(Gosu::KB_UP)
      @palette_scroll -= 1 if @palette_scroll > 0
      sleep 0.05
    end
    if Gosu.button_down?(Gosu::KB_DOWN)
      max = (@tiles.size + TILESET_COLS - 1) / TILESET_COLS - (height - PALETTE_START_Y) / TILE_SIZE
      @palette_scroll += 1 if @palette_scroll < max
      sleep 0.05
    end
    if Gosu.button_down?(Gosu::KB_A)
      @mode = :draw
    end
    if Gosu.button_down?(Gosu::KB_C)
      @mode = :assign
    end
    if Gosu.button_down?(Gosu::KB_LEFT_CONTROL) && Gosu.button_down?(Gosu::KB_S)
      save_current
      save_tile_types
    end
  end

  def button_down(id)
    case id
    when Gosu::MsWheelUp
      zoom_at(mouse_x, mouse_y, 1.1)
    when Gosu::MsWheelDown
      zoom_at(mouse_x, mouse_y, 1/1.1)
    when Gosu::MsRight
      unless Gosu.button_down?(Gosu::KB_LEFT_CONTROL)
        handle_right_click
      end
    end
  end

  def zoom_at(screen_x, screen_y, factor)
    old_zoom = @camera[:zoom]
    new_zoom = old_zoom * factor
    new_zoom = [[new_zoom, 0.3].max, 4.0].min
    return if new_zoom == old_zoom

    world_x = (screen_x - MAP_AREA_X + @camera[:x]) / (TILE_SIZE * old_zoom)
    world_y = (screen_y + @camera[:y]) / (TILE_SIZE * old_zoom)

    @camera[:zoom] = new_zoom
    @camera[:x] = screen_x - MAP_AREA_X - world_x * TILE_SIZE * new_zoom
    @camera[:y] = screen_y - world_y * TILE_SIZE * new_zoom

    clamp_camera
  end

  def clamp_camera
    map = current_map
    return unless map
    max_x = map['width'] * TILE_SIZE * @camera[:zoom] - width + MAP_AREA_X
    max_y = map['height'] * TILE_SIZE * @camera[:zoom] - height
    @camera[:x] = [[@camera[:x], 0].max, max_x].min if max_x > 0
    @camera[:y] = [[@camera[:y], 0].max, max_y].min if max_y > 0
  end

  def handle_left_click
    mx, my = mouse_x, mouse_y

    # Клик по списку карт
    if mx >= MAP_LIST_START_X && mx <= MAP_LIST_START_X + MAP_LIST_W
      y = MAP_LIST_START_Y + 20
      @maps.each_with_index do |_, i|
        y += MAP_LIST_ITEM_H
        if my.between?(y - MAP_LIST_ITEM_H, y)
          @current_index = i
          load_tileset
          load_tile_types
          return
        end
      end
    end

    # Клик по кнопкам действий
    ACTIONS.each_with_index do |action, idx|
      x = ACTION_BTN_X
      y = ACTION_BTN_Y + idx * (ACTION_BTN_H + ACTION_BTN_GAP)
      if mx.between?(x, x+ACTION_BTN_W) && my.between?(y, y+ACTION_BTN_H)
        send(action[:method])
        return
      end
    end

    # A / C / Save
    if my.between?(10, 10+BTN_A_H)
      if mx.between?(BTN_A_X, BTN_A_X+BTN_A_W)
        @mode = :draw
        return
      elsif mx.between?(BTN_C_X, BTN_C_X+BTN_C_W)
        @mode = :assign
        return
      else
        btn_save_x = width - BTN_SAVE_W - 10
        if mx.between?(btn_save_x, btn_save_x+BTN_SAVE_W)
          save_current
          save_tile_types
          return
        end
      end
    end

    # Трансформации
    if my.between?(10, 10+TRANS_H)
      trans_start_x = width - TRANS_X_OFFSET
      TRANS_MODES.each_with_index do |mode, i|
        x = trans_start_x + i * (TRANS_W + 5)
        if mx.between?(x, x+TRANS_W)
          @right_click_mode = mode
          return
        end
      end
    end

    # Выбор типа (assign)
    if @mode == :assign && my.between?(TYPE_ICONS_Y, TYPE_ICONS_Y+TYPE_ICON_SIZE)
      @type_icons.each_with_index do |_, i|
        x = TYPE_ICONS_START_X + i * TYPE_ICONS_GAP
        if mx.between?(x, x+TYPE_ICON_SIZE)
          @current_type = i
          return
        end
      end
    end

    # Выбор тайла из палитры
    if mx >= PALETTE_START_X && mx < PALETTE_START_X + TILESET_COLS * TILE_SIZE && my >= PALETTE_START_Y
      col = ((mx - PALETTE_START_X) / TILE_SIZE).to_i
      row = ((my - PALETTE_START_Y) / TILE_SIZE).to_i + @palette_scroll
      idx = row * TILESET_COLS + col
      if idx >= 0 && idx < @tiles.size
        if @mode == :assign
          @tile_types[idx] = @current_type
          save_tile_types
        else
          @selected_tile = idx
        end
      end
      return
    end

    # Рисование на карте
    if @mode == :draw
      map = current_map
      return unless map
      gx = ((mx - MAP_AREA_X + @camera[:x]) / (TILE_SIZE * @camera[:zoom])).to_i
      gy = ((my + @camera[:y]) / (TILE_SIZE * @camera[:zoom])).to_i
      if gx >= 0 && gx < map['width'] && gy >= 0 && gy < map['height']
        map['tiles'][gx][gy] = @selected_tile
        map['rot'][gx][gy] = 0
        map['mirror_x'][gx][gy] = false
        map['mirror_y'][gx][gy] = false
      end
    end
  end

  def handle_right_click
    map = current_map
    return unless map
    mx, my = mouse_x, mouse_y
    gx = ((mx - MAP_AREA_X + @camera[:x]) / (TILE_SIZE * @camera[:zoom])).to_i
    gy = ((my + @camera[:y]) / (TILE_SIZE * @camera[:zoom])).to_i
    if gx >= 0 && gx < map['width'] && gy >= 0 && gy < map['height']
      case @right_click_mode
      when :rotate
        map['rot'][gx][gy] = (map['rot'][gx][gy] + 1) % 4
      when :mirror_h
        map['mirror_x'][gx][gy] = !map['mirror_x'][gx][gy]
      when :mirror_v
        map['mirror_y'][gx][gy] = !map['mirror_y'][gx][gy]
      end
    end
  end
end

MapEditor.new.show