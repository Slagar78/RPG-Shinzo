# game.rb
require 'raylib'
require_relative 'lib/database'
require_relative 'lib/player'
require_relative 'lib/ui'
require_relative 'lib/AudioManager'
require_relative 'lib/item_actions_ui'
require 'json'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

def clamp(value, min, max)
  return min if value < min
  return max if value > max
  value
end

# ========== ЗАГРУЗЧИК КАРТЫ ==========
class GameMap
  attr_reader :width, :height, :tile_size, :tileset_texture, :music_file, :music_volume

  def initialize
    maps = Dir["data/maps/*.json"]
    @src_rect_cache = {}
    if maps.empty?
      puts "No maps found in data/maps/"
      @width = 20
      @height = 15
      @tiles = Array.new(@width) { Array.new(@height, 0) }
      @tile_types = []
      @tileset_texture = nil
      return
    end

    data = JSON.parse(File.read(maps.first))
    @width = data['width']
    @height = data['height']
    @tiles = data['tiles']
    @rot = data['rot'] || Array.new(@width) { Array.new(@height, 0) }
    @mirror_x = data['mirror_x'] || Array.new(@width) { Array.new(@height, false) }
    @mirror_y = data['mirror_y'] || Array.new(@width) { Array.new(@height, false) }

    if data['music']
      @music_file = data['music']['file'] || ""
      @music_volume = data['music']['volume'] || 0.8
    else
      @music_file = ""
      @music_volume = 0.8
    end

    # === ЗАГРУЗКА ТИПОВ ТАЙЛОВ (как в редакторе) ===
    # Чистим путь к тайлсету: заменяем все \ на / (избавляемся от двойных экранирований)
    raw_path = data['tileset'] || "assets/tilesets/tileset.png"
    tileset_path = raw_path.gsub('\\', '/')         # теперь всегда с прямыми слешами

    # Формируем безопасное имя файла типов (заменяем / \ : на _)
    safe = tileset_path.gsub(/[\\\/:]/, '_')
    type_file = "data/tile_types/#{safe}.json"

    if File.exist?(type_file)
      @tile_types = JSON.parse(File.read(type_file))
    else
      @tile_types = []
    end

    # Загружаем текстуру тайлсета
    @tileset_texture = LoadTexture(tileset_path)
	SetTextureFilter(@tileset_texture, TEXTURE_FILTER_POINT)
    @tile_size = 48
    @dst_rect = Rectangle.create(0, 0, @tile_size, @tile_size)
    @zero_vec = Vector2.create(0, 0)
	@center_vec = Vector2.create(@tile_size / 2.0, @tile_size / 2.0)

    # Размеры тайлсета
    tex_w = @tileset_texture.width
    tex_h = @tileset_texture.height
    @full_cols = tex_w / @tile_size
    @full_rows = tex_h / @tile_size
    total_tiles = @full_cols * @full_rows

    # Расширяем массив типов, если он меньше нужного (на случай неполного файла)
    if @tile_types.length < total_tiles
      @tile_types += Array.new(total_tiles - @tile_types.length, 0)
    end
  end

  # 
  def tile_src_rect(tile_id)
  return nil if tile_id.nil? || tile_id < 0
  return @src_rect_cache[tile_id] if @src_rect_cache.key?(tile_id)

  # Количество полос шириной 8 тайлов
  strips = @full_cols / 8   # для 32 -> 4
  rows_per_strip = @full_rows   # 32

  strip = tile_id / (8 * rows_per_strip)   # в какой полосе (0..3)
  local = tile_id % (8 * rows_per_strip)   # позиция внутри полосы

  col = strip * 8 + (local % 8)   # столбец в тайлсете
  row = local / 8                 # строка в тайлсете

  rect = Rectangle.create(col * @tile_size, row * @tile_size,
                          @tile_size, @tile_size)
  @src_rect_cache[tile_id] = rect
  rect
end

  def draw_under_tiles
    return unless @tileset_texture
    (0...@width).each do |x|
      (0...@height).each do |y|
        tile_id = @tiles[x][y]
        next if tile_id.nil? || tile_id < 0
        type = @tile_types[tile_id] || 0
        next unless type == 3
        src = tile_src_rect(tile_id)
        next unless src
        dst = @dst_rect
        dst.x = x * @tile_size
        dst.y = y * @tile_size
        DrawTexturePro(@tileset_texture, src, dst, @zero_vec, 0, WHITE)
      end
    end
  end

# === ОПТИМИЗИРОВАННЫЕ МЕТОДЫ С ОТСЕЧЕНИЕМ ===
def draw_visible(camera)
  return unless @tileset_texture
  view_left   = camera.target.x - camera.offset.x
  view_right  = camera.target.x + camera.offset.x
  view_top    = camera.target.y - camera.offset.y
  view_bottom = camera.target.y + camera.offset.y

  start_x = [ (view_left / @tile_size).floor - 1, 0 ].max
  end_x   = [ (view_right / @tile_size).ceil + 1, @width ].min
  start_y = [ (view_top / @tile_size).floor - 1, 0 ].max
  end_y   = [ (view_bottom / @tile_size).ceil + 1, @height ].min

  half_tile = @tile_size / 2.0
  # Используем повторно один Vector2 и один Rectangle
  center_vec = @center_vec   # уже создан в initialize (или создай, если нет)
  src_rect = Rectangle.create(0, 0, @tile_size, @tile_size)
  dst_rect = Rectangle.create(0, 0, @tile_size, @tile_size)

  (start_x...end_x).each do |x|
    (start_y...end_y).each do |y|
      tile_id = @tiles[x][y]
      next if tile_id.nil? || tile_id < 0

      src = tile_src_rect(tile_id)
      next unless src

      rot = @rot[x][y] || 0
      flip_x = @mirror_x[x][y] || false
      flip_y = @mirror_y[x][y] || false

      world_center_x = x * @tile_size + half_tile
      world_center_y = y * @tile_size + half_tile

      dst_rect.x = world_center_x
      dst_rect.y = world_center_y
      dst_rect.width  = flip_x ? -@tile_size : @tile_size
      dst_rect.height = flip_y ? -@tile_size : @tile_size

      angle = rot * 90.0
      DrawTexturePro(@tileset_texture, src, dst_rect, center_vec, angle, WHITE)
    end
  end
end

def draw_under_tiles_visible(camera)
  return unless @tileset_texture

  view_left   = camera.target.x - camera.offset.x
  view_right  = camera.target.x + camera.offset.x
  view_top    = camera.target.y - camera.offset.y
  view_bottom = camera.target.y + camera.offset.y

  start_x = [ (view_left / @tile_size).floor - 1, 0 ].max
  end_x   = [ (view_right / @tile_size).ceil + 1, @width ].min
  start_y = [ (view_top / @tile_size).floor - 1, 0 ].max
  end_y   = [ (view_bottom / @tile_size).ceil + 1, @height ].min

  half_tile = @tile_size / 2.0
  # Повторно используем уже созданные векторы и прямоугольники
  center_vec = @center_vec   # создан в initialize
  dst_rect = @dst_rect       # уже есть в initialize (или создадим один раз, если его нет)

  (start_x...end_x).each do |x|
    (start_y...end_y).each do |y|
      tile_id = @tiles[x][y]
      next if tile_id.nil? || tile_id < 0

      type = @tile_types[tile_id] || 0
      next unless type == 3

      src = tile_src_rect(tile_id)
      next unless src

      rot = @rot[x][y] || 0
      flip_x = @mirror_x[x][y] || false
      flip_y = @mirror_y[x][y] || false

      world_center_x = x * @tile_size + half_tile
      world_center_y = y * @tile_size + half_tile

      dst_rect.x = world_center_x
      dst_rect.y = world_center_y
      dst_rect.width  = flip_x ? -@tile_size : @tile_size
      dst_rect.height = flip_y ? -@tile_size : @tile_size

      angle = rot * 90.0
      DrawTexturePro(@tileset_texture, src, dst_rect, center_vec, angle, WHITE)
    end
  end
end

  # === КОНЕЦ НОВЫХ МЕТОДОВ ===

  def passable?(x, y)
    return false if x < 0 || x >= @width || y < 0 || y >= @height
    tile_id = @tiles[x][y]
    type = @tile_types[tile_id] || 0
    type != 1
  end

  def tile_type_at(x, y)
    return 1 if x < 0 || x >= @width || y < 0 || y >= @height
    tile_id = @tiles[x][y]
    @tile_types[tile_id] || 0
  end
end

def lerp(a, b, t)
  a + (b - a) * t
end

# ========== ОСНОВНАЯ ИГРА ==========
class Game
  def initialize
    # ── Окно и целевой FPS ──────────────────────
    InitWindow(576, 480, "RPG Shinzo")
    SetTargetFPS(60)

    # ── База данных предметов ──────────────────
    @db = Database.new

    # ── Карта (GameMap) ─────────────────────────
    @game_map = GameMap.new

    # ── Аудиосистема ────────────────────────────
    Raylib.InitAudioDevice()
    @audio = AudioManager.new
    @audio.play(@game_map.music_file, @game_map.music_volume)

    # ── Игрок ───────────────────────────────────
    @player = Player.new(@game_map)

    # ── Нижнее меню (4 плитки) ─────────────────
    @menu = BottomMenu.new	
    @items_submenu = BottomMenu.new([
      { "id" => 0, "name" => "use",   "icon" => "assets/ui/menu/Use.png",   "icon_anim" => "assets/ui/menu/Use_anim.png" },
      { "id" => 1, "name" => "give",  "icon" => "assets/ui/menu/Give.png",  "icon_anim" => "assets/ui/menu/Give_anim.png" },
      { "id" => 2, "name" => "equip", "icon" => "assets/ui/menu/Equip.png", "icon_anim" => "assets/ui/menu/Equip_anim.png" },
      { "id" => 3, "name" => "drop",  "icon" => "assets/ui/menu/Drop.png",  "icon_anim" => "assets/ui/menu/Drop_anim.png" }
    ])

    # ── Загрузка шрифта с поддержкой кириллицы ──
    codepoints = []
    # базовая латиница (пробел, цифры, английские буквы и знаки)
    (32..126).each { |cp| codepoints << cp }
    # кириллица (русские буквы, включая Ё/ё)
    (0x0400..0x04FF).each { |cp| codepoints << cp }

    # Создаём низкоуровневый C-совместимый буфер для кодов символов
    cp_ptr = FFI::MemoryPointer.new(:int, codepoints.size)
    cp_ptr.write_array_of_int(codepoints)

    # Загружаем шрифт с явным указанием нужных глифов
    @font = LoadFontEx("assets/ui/fonts/main.ttf", 20, cp_ptr, codepoints.size)
	Raylib.SetTextureFilter(@font.texture, TEXTURE_FILTER_POINT)
	
	# ── Загрузка общих данных для меню предметов ──
    @party = []
   if File.exist?("data/actors/actors.json")
    data = JSON.parse(File.read("data/actors/actors.json"))
    @party = data["actors"] || []
   end

    @classes_data = []
    @class_names = {}
   if File.exist?("data/actors/classes.json")
    data = JSON.parse(File.read("data/actors/classes.json"))
    @classes_data = data["classes"] || []
    @classes_data.each { |c| @class_names[c["id"]] = c["name"] }
   end

    @start_inventory = []
   if File.exist?("data/actors/start_inventory.json")
    data = JSON.parse(File.read("data/actors/start_inventory.json"))
    @start_inventory = data["start_inventory"] || []
   end

    # Предметные подменю
    @use_menu = UseMenu.new(@font, @party, @classes_data, @class_names, @start_inventory)
	@give_menu = GiveMenu.new(@font, @party, @classes_data, @class_names, @start_inventory)
    # Позже добавятся  EquipMenu, DropMenu

    # ── Статусный оверлей (с кастомным шрифтом) ─
    @status_overlay = StatusOverlay.new(@font)
	@magic_overlay = MagicOverlay.new(@font)
	@profile = Profile.new(@font)

    # ── Состояние игры ──────────────────────────
    @game_state = :playing
	@pending_profile_open = false
	@pending_status_open = false
	@pending_menu_open = false
	@active_item_action = nil   # будет хранить текущее окно (Use/Give/...)
	@pending_items_close = false
	@pending_menu_request = false   # ждём завершения шага, чтобы открыть меню

    # ── 2D-камера для следования за игроком ─────
    @camera = Camera2D.new
    @camera.zoom = 1.0
    @camera.offset = Vector2.create(288, 240)           # центр экрана
    @camera.target = Vector2.create(
      @player.x * @game_map.tile_size + 24,
      @player.y * @game_map.tile_size + 24
    )
	@snapped_camera = Camera2D.new
	@camera_x = @camera.target.x
    @camera_y = @camera.target.y
	@accumulator = 0.0
    @fixed_dt = 1.0 / 60.0
  end

  def run
  previous_time = GetTime()
  until WindowShouldClose()
    current_time = GetTime()
    frame_time = current_time - previous_time
    previous_time = current_time
    # Предотвращаем спираль смерти, если кадр занял очень много времени
    frame_time = 0.2 if frame_time > 0.2
    @accumulator += frame_time

    while @accumulator >= @fixed_dt
      handle_input     # ввод можно обрабатывать каждый тик, но достаточно и раз за кадр? Оставим пока здесь для простоты
      update            # логика двигается с постоянным dt
      @accumulator -= @fixed_dt
    end

    draw               # рисуем с плавающей частотой
  end
  @audio.stop
  Raylib.CloseAudioDevice()
  Raylib.UnloadFont(@font) if @font
  CloseWindow()
end
# HANDLE INPUT
def handle_input
  case @game_state
  when :playing
    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      if @player.moving
        @pending_menu_request = true      # запоминаем
      else
        @game_state = :menu
        @menu.open
      end
    else
      @player.handle_input
    end
# меню из 4плиток
  when :menu
    if IsKeyPressed(KEY_S)
      @game_state = :playing
      @menu.close
    elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        case @menu.selected_index
      when 0
        @game_state = :status
        @status_overlay.open(@player)
      when 1
        @game_state = :magic
        @magic_overlay.open(@player)
      when 2
        @game_state = :items
        @items_submenu.open
      else
        @game_state = :playing
        @menu.close
      end
    else
      @menu.handle_input
    end
	
  when :status
    if IsKeyPressed(KEY_S)
      @status_overlay.close          # запускаем анимацию разборки
      @pending_menu_open = true      # ждём завершения, затем вернёмся в меню
    elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      @status_overlay.close
      @pending_profile_open = true
    else
      @status_overlay.handle_input
    end
  when :magic
    if IsKeyPressed(KEY_S)
      @magic_overlay.close          # запускаем анимацию разборки
      @pending_menu_open = true     # ждём завершения, затем вернёмся в меню
    else
      @magic_overlay.handle_input
    end
	
    when :items
    if IsKeyPressed(KEY_S)
      @items_submenu.close
      @game_state = :menu
    elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      unless @pending_items_close
        case @items_submenu.selected_index

        when 0   # Use
          @use_menu.open
          @active_item_action = @use_menu
          @game_state = :item_action
        when 1   # Give
          @give_menu.open
          @active_item_action = @give_menu
          @game_state = :item_action
        # when 2 # Equip
        # when 3 # Drop
        end
      end
    else
      @items_submenu.handle_input unless @pending_items_close
    end
	
  when :profile
    if IsKeyPressed(KEY_S) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      @profile.close
      @pending_status_open = true
      # @game_state остаётся :profile, чтобы анимация завершилась
    end
	
  when :item_action
  @active_item_action.handle_input
  # Если окно начало анимацию закрытия (S на персонажах), запоминаем, чтобы дождаться конца
  @pending_items_close = true if @active_item_action.anim_phase == 3
  end
end

  def update
    @audio.update
    @player.update_animation
    @player.update_movement if @game_state == :playing
	if @pending_menu_request && !@player.moving
      @pending_menu_request = false
      @game_state = :menu
      @menu.open
    end
    @menu.update if @game_state == :menu
	@items_submenu.update if @game_state == :items   
	@status_overlay.update if @game_state == :status
	@magic_overlay.update if @game_state == :magic
	@active_item_action&.update if @game_state == :item_action
	
if @pending_menu_open
  if @game_state == :magic && !@magic_overlay.instance_variable_get(:@visible)
    @game_state = :menu
    @pending_menu_open = false
  elsif @game_state == :status && !@status_overlay.instance_variable_get(:@visible)
    @game_state = :menu
    @pending_menu_open = false
  end
end
    # Когда статус полностью скрылся (после анимации), открываем Profile
    if @pending_profile_open && !@status_overlay.instance_variable_get(:@visible)
        @profile.open(
        @status_overlay.current_actor,
        @status_overlay.instance_variable_get(:@party),
        @status_overlay.instance_variable_get(:@class_names),
        @status_overlay.instance_variable_get(:@classes_data),
        @status_overlay.instance_variable_get(:@portrait_cache),
        @status_overlay.instance_variable_get(:@start_inventory)
      )
      @pending_profile_open = false
      @game_state = :profile
    end
    @profile.update if @game_state == :profile
	# Когда Profile полностью скрылся, открываем статус
    if @pending_status_open && !@profile.instance_variable_get(:@visible)
      @status_overlay.open
      @pending_status_open = false
      @game_state = :status
    end
 if @game_state == :playing
  target_x = @player.visual_x + @game_map.tile_size / 2
  target_y = @player.visual_y + @game_map.tile_size / 2

  half_w = 288
  half_h = 240
  max_x = @game_map.width * @game_map.tile_size - half_w
  max_y = @game_map.height * @game_map.tile_size - half_h

  target_x = clamp(target_x, half_w, max_x) if max_x > half_w
  target_y = clamp(target_y, half_h, max_y) if max_y > half_h

  @camera.target = Vector2.create(target_x, target_y)
end
	
	# Ожидание закрытия окна Use/Give/Drop/Equip
    if @pending_items_close && @active_item_action && !@active_item_action.visible
    @game_state = :items
    @pending_items_close = false
    end
  end

def draw
    BeginDrawing()
    ClearBackground(RAYWHITE)

    # --- ИСПРАВЛЕНИЕ МИКРОФРИЗОВ ---
    BeginMode2D(@camera)
@game_map.draw_visible(@camera)
@player.draw
@game_map.draw_under_tiles_visible(@camera)
EndMode2D()
    # --------------------------------

    case @game_state
    when :menu
      @menu.draw
    when :status
      @status_overlay.draw
    when :magic
      @magic_overlay.draw
    when :profile
      @profile.draw
    when :items
      @items_submenu.draw
    when :item_action
      @active_item_action.draw
    end

    DrawText("FPS: #{GetFPS()}", 576 - 100, 10, 20, DARKGRAY)
    EndDrawing()
   end
end

Game.new.run if __FILE__ == $0