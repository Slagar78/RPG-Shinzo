# lib/ui.rb
require 'json'

# ============================================
# МЕНЮ 4 ПЛИТКИ (Bottom Menu)
# ============================================
class BottomMenu
  attr_reader :selected_index

  # tiles_data – необязательный массив хешей с ключами id, name, icon, icon_anim
  def initialize(tiles_data = nil)
    load_tiles(tiles_data)
    @visible = false
    @selected_index = 0
    @anim_timer = 0
    @tile_size = 48
    @offset = 48
    load_textures
  end

  def load_tiles(tiles_data = nil)
    if tiles_data
      @tiles = tiles_data
    elsif File.exist?("data/menu.json")
      data = JSON.parse(File.read("data/menu.json"))
      @tiles = data["tiles"]
    else
      @tiles = [
        { "id" => 0, "name" => "status", "icon" => "assets/ui/menu/status.png", "icon_anim" => "assets/ui/menu/status_anim.png" },
        { "id" => 1, "name" => "magic",  "icon" => "assets/ui/menu/magic.png",  "icon_anim" => "assets/ui/menu/magic_anim.png" },
        { "id" => 2, "name" => "items",  "icon" => "assets/ui/menu/items.png",  "icon_anim" => "assets/ui/menu/items_anim.png" },
        { "id" => 3, "name" => "event",  "icon" => "assets/ui/menu/event.png",  "icon_anim" => "assets/ui/menu/event_anim.png" }
      ]
    end
  end
  
  def load_textures
    @textures = []
    @tiles.each do |tile|
      normal = Raylib.LoadTexture(tile["icon"])
      anim = Raylib.LoadTexture(tile["icon_anim"])
      Raylib.SetTextureFilter(normal, 0)
      Raylib.SetTextureFilter(anim, 0)
      @textures << { normal: normal, anim: anim }
    end
  end
  
  def open
    @visible = true
    @selected_index = 0
    @anim_timer = 0
  end
  
  def close
    @visible = false
  end
  
  def handle_input
    return unless @visible
    
    if Raylib.IsKeyPressed(Raylib::KEY_UP)
      @selected_index = 0
    elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
      @selected_index = 1
    elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @selected_index = 2
    elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
      @selected_index = 3
    end
  end
  
  def update
    return unless @visible
    @anim_timer += 1
  end
  
  def draw
    return unless @visible
    
    center_x = 576 / 2
    center_y = 480 - 80
    
    positions = [
      { x: center_x,           y: center_y - @offset + 24 },
      { x: center_x - @offset, y: center_y },
      { x: center_x + @offset, y: center_y },
      { x: center_x,           y: center_y + @offset - 24 }
    ]
    
    (0..3).each do |i|
      tex = @textures[i]
      pos = positions[i]
      
      if i == @selected_index
        use_anim = (@anim_timer % 24) < 12 && tex[:anim]
        texture = use_anim ? tex[:anim] : tex[:normal]
      else
        texture = tex[:normal]
      end
      
      dst = Raylib::Rectangle.create(pos[:x] - @tile_size/2, pos[:y] - @tile_size/2, @tile_size, @tile_size)
      src = Raylib::Rectangle.create(0, 0, @tile_size, @tile_size)
      Raylib.DrawTexturePro(texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end
  end
end

# ============================================
# ОВЕРЛЕЙ СТАТУСА (Status Overlay)
# ============================================
class StatusOverlay

  def get_actor_stats(actor_name)
    @party.each do |actor|
      if actor["name"] == actor_name
        return actor
      end
    end
    nil
  end

def find_actor_items(actor_name)
  actor = @party.find { |a| a["name"] == actor_name }
  return [] unless actor

  entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
  return [] unless entry
  entry["items"] || []
end

def change_selected_actor(delta)
  return unless @party.any?
  new_index = @selected_actor_index + delta
  return if new_index < 0 || new_index >= @party.length

  @selected_actor_index = new_index

  # Автоскролл (как в JS)
  if @selected_actor_index < @list_top_index
    @list_top_index = @selected_actor_index
  elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
    @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
  end

  # Обновляем текущего персонажа
  @current_actor = @party[@selected_actor_index]["name"]
  @current_items = find_actor_items(@current_actor)
  actor = @party[@selected_actor_index]
  portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
  @portrait_tex = load_portrait(portrait_name)
  @blink_tex = load_blink_portrait(portrait_name)

  # Обновляем заклинания
  actor = @party[@selected_actor_index]
  if actor
    klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
    spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
    @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
  end
end

  VISIBLE_ROWS = 5
  attr_reader :current_actor
  def initialize(font = nil)
    @font = font
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
	@blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	@portrait_cache = {}
	
    # Загружаем классы
    @class_names = {}
    @classes_data = []
   if File.exist?("data/actors/classes.json")
    data = JSON.parse(File.read("data/actors/classes.json"))
    @classes_data = data["classes"] || []
    @classes_data.each { |c| @class_names[c["id"]] = c["name"] }
   end
	
    # Загружаем заклинания
    if File.exist?("data/spells/spells.json")
     data = JSON.parse(File.read("data/spells/spells.json"))
     @all_spells = data["spells"] || []
    else
      @all_spells = []
    end
	
	# Загружаем начальный инвентарь
    if File.exist?("data/actors/start_inventory.json")
     data = JSON.parse(File.read("data/actors/start_inventory.json"))
     @start_inventory = data["start_inventory"] || []
    else
     @start_inventory = []
    end
    
    # Целевые позиции
    @upper_target_x = 182
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16
    
    # Стартовые позиции
    @upper_start_x = 576 + 220
    @lower_start_y = 480 + 220
    @portrait_start_x = -220
    @frame_start_x = -220
    
    # Текущие позиции
    @upper_x = @upper_start_x
    @upper_y = @upper_target_y
    @lower_x = @lower_target_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @portrait_y = @portrait_target_y
    @frame_x = @frame_start_x
    @frame_y = @frame_target_y
    
    # Размеры
    @upper_w = 346
    @upper_h = 208
    @lower_w = 480
    @lower_h = 240
    @portrait_w = 134
    @portrait_h = 208
    @frame_w = 134
    @frame_h = 208
    
    # Моргание
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	# Плавная пульсация рамки выбора
    @selection_blink_timer = 0
	# Режим просмотра нижней панели: 0 = класс/уровень/опыт, 1 = статы
    @status_view_mode = 0
	@list_top_index = 0
    @selected_actor_index = 0      # индекс текущего персонажа в party
	@input_timer_up = 0
    @input_timer_down = 0
    
    load_textures
    load_actors
  end

  # Вспомогательный метод для рисования текста кастомным шрифтом
  def draw_text_custom(text, x, y, size, color)
    if @font
      Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
    else
      Raylib.DrawText(text, x, y, size, color)
    end
  end
  
  def draw_text_centered_h(text, cx, y, size, color)
    return unless @font
    vec = Raylib.MeasureTextEx(@font, text, size, 1)
    x = cx - vec.x / 2
    Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
  end
  
  def load_textures
    @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
    @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
    @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
	@ruby_tex = Raylib.LoadTexture("assets/ui/ruby_icon.png")
    
    Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
    Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
    Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
	Raylib.SetTextureFilter(@ruby_tex, 0) if @ruby_tex
  end
  
  def load_actors
    if File.exist?("data/actors/actors.json")
      data = JSON.parse(File.read("data/actors/actors.json"))
      @party = data["actors"] || []
    else
      @party = []
    end
  end
 
   # Рисует название предмета с переносом, если оно из двух слов (как в SF2)
  def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      # Разбиваем на два слова по первому пробелу
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip

      # Первая строка — как обычно
      draw_text_custom(first_word, x, y, size, color)

      # Вторая строка — смещена вниз (как в JS: itemSplitOffset = 15) и вправо (secondLineIndent = 14)
      draw_text_custom(second_word, x + 14, y + 15, size, color)
    else
      draw_text_custom(text, x, y, size, color)
    end
  end
 
def load_portrait(name)
  return nil unless name
  return @portrait_cache[name] if @portrait_cache.key?(name)

  path = "assets/ui/portraits/#{name}.png"
  return nil unless File.exist?(path)
  img = Raylib.LoadImage(path)
  tex = Raylib.LoadTextureFromImage(img)
  Raylib.UnloadImage(img)
  Raylib.SetTextureFilter(tex, 0)
  @portrait_cache[name] = tex
  tex
end
  
def load_blink_portrait(name)
  return nil unless name
  cache_key = "#{name}_blink"
  return @portrait_cache[cache_key] if @portrait_cache.key?(cache_key)

  path = "assets/ui/portraits/#{name}_blink.png"
  return nil unless File.exist?(path)
  img = Raylib.LoadImage(path)
  tex = Raylib.LoadTextureFromImage(img)
  Raylib.UnloadImage(img)
  Raylib.SetTextureFilter(tex, 0)
  @portrait_cache[cache_key] = tex
  tex
end
  
  def open(player = nil)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false
    
    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
    
  if @party.any?
    # Проверяем, не выходит ли сохранённый индекс за пределы
    if @selected_actor_index >= @party.length
      @selected_actor_index = @party.length - 1
    end
    # Скролл подгоняем, чтобы выбранный персонаж был виден
    if @selected_actor_index < @list_top_index
      @list_top_index = @selected_actor_index
    elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
      @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
    end
    @current_actor = @party[@selected_actor_index]["name"]

    # Находим данные актора
    actor = @party.find { |a| a["name"] == @current_actor }
    if actor
      # Ищем класс актора по class_id
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
      @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
    else
      @current_spells = []
    end

    # Предметы и портреты
    @current_items = find_actor_items(@current_actor)
    actor = @party.find { |a| a["name"] == @current_actor }
    portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
    @portrait_tex = load_portrait(portrait_name)
    @blink_tex = load_blink_portrait(portrait_name)
  else
    @current_actor = nil
    @current_spells = []
    @current_items = []
  end
 # таймеры
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	@selection_blink_timer = 0
  end
  
  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end
  
  def force_close
    @visible = false
    @anim_phase = 0
  end
  
  def handle_input
  return unless @visible && @anim_phase == 2

  # Закрыть меню
  if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
    close
    return
  end

  # Переключение режима ←/→ (одиночное)
  if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
    @status_view_mode = 1 - @status_view_mode
  end

  # Автоповтор ↑
  if Raylib.IsKeyDown(Raylib::KEY_UP)
    @input_timer_up += 1
    if @input_timer_up == 1 || (@input_timer_up > 20 && (@input_timer_up - 20) % 5 == 0)
      change_selected_actor(-1)
    end
  else
    @input_timer_up = 0
  end

  # Автоповтор ↓
  if Raylib.IsKeyDown(Raylib::KEY_DOWN)
    @input_timer_down += 1
    if @input_timer_down == 1 || (@input_timer_down > 20 && (@input_timer_down - 20) % 5 == 0)
      change_selected_actor(1)
    end
  else
    @input_timer_down = 0
  end
end
  
  def update
    return unless @visible
    
    speed = 38
    
    case @anim_phase
    when 1
      @portrait_x += speed
      @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed
      @frame_x = @frame_target_x if @frame_x > @frame_target_x
      
      @upper_x -= speed
      @upper_x = @upper_target_x if @upper_x < @upper_target_x
      
      @lower_y -= speed
      @lower_y = @lower_target_y if @lower_y < @lower_target_y
      
      if @portrait_x >= @portrait_target_x && 
         @frame_x >= @frame_target_x &&
         @upper_x <= @upper_target_x && 
         @lower_y <= @lower_target_y
        @anim_phase = 2
      end
      
    when 3
      @portrait_x -= speed
      @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed
      @frame_x = @frame_start_x if @frame_x < @frame_start_x
      
      @upper_x += speed
      @upper_x = @upper_start_x if @upper_x > @upper_start_x
      
      @lower_y += speed
      @lower_y = @lower_start_y if @lower_y > @lower_start_y
      
      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
      end
    end
    
    if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
	  # Пульсация рамки выбора
      @selection_blink_timer += 1
    end
  end
# def draw метод
  def draw
    return unless @visible

    origin = Raylib::Vector2.create(0, 0)

    # Нижняя панель
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    src = Raylib::Rectangle.create(0, 0, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, src, dst, origin, 0, Raylib::WHITE)

    # Верхняя панель
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    src = Raylib::Rectangle.create(0, 0, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, src, dst, origin, 0, Raylib::WHITE)

    # Портрет
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      src = Raylib::Rectangle.create(0, 0, 134, 208)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end

    # Рамка
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)

    # ===== ВЕРХНЯЯ ПАНЕЛЬ: ТЕКСТ =====
    actor_data = @party.find { |a| a["name"] == @current_actor }
    if actor_data
      class_id   = actor_data["class_id"]
      class_name = @class_names[class_id] || "???"
      level      = actor_data["level"]
      header     = "#{actor_data["name"].slice(0, 10)}  #{class_name.slice(0, 10)}  LV #{level}"
      draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
    else
      draw_text_custom("NO DATA", @upper_x + 25, @upper_y + 12, 20, WHITE)
    end

    draw_text_custom("Магия", @upper_x + 25, @upper_y + 38, 20, WHITE)
    draw_text_custom("Предметы", @upper_x + 195, @upper_y + 38, 20, WHITE)

    # Магия из класса персонажа (только изученные заклинания)
    if @current_spells && @current_spells.any?
      @current_spells.each_with_index do |spell, i|
        y = @upper_y + 72 + i * 34
        draw_text_custom("#{spell["spell"]} Lv#{spell["spell_level"]}", @upper_x + 25, y, 20, WHITE)
      end
    end

    # Предметы из start_inventory.json
    if @current_items && @current_items.any?
      @current_items.each_with_index do |item_entry, i|
        next if item_entry["item"] == "NOTHING"

        y = @upper_y + 64 + i * 33
        draw_item_name(item_entry["item"], @upper_x + 195, y, 18, WHITE)

        # Метка экипировки
        if item_entry["equipped"]
          draw_text_custom("E", @upper_x + 180, y, 18, YELLOW)
        end
      end
    end

    # ===== НИЖНЯЯ ПАНЕЛЬ: ЗАГОЛОВКИ =====
    header_y = @lower_y + 28

    if @status_view_mode == 0
      # Режим 0: Имя, Класс, Уровень, Опыт
      draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
      draw_text_custom("Класс",  @lower_x + 187, header_y, 20, WHITE)

      level_header_center_x = @lower_x + 290 + Raylib.MeasureTextEx(@font, "Уровень", 20, 1).x / 2
      exp_header_center_x   = @lower_x + 395 + Raylib.MeasureTextEx(@font, "Опыт", 20, 1).x / 2

      draw_text_custom("Уровень", @lower_x + 290, header_y, 20, WHITE)
      draw_text_custom("Опыт",    @lower_x + 395, header_y, 20, WHITE)
    else
      # Режим 1: Статистика (HP, MP, AT, DF, AGI, MV)
	  draw_text_custom("Имя", @lower_x + 44, header_y, 20, WHITE)
      stat_headers = ["HP", "MP", "AT", "DF", "AGI", "MV"]
      # Центры колонок (примерно соответствуют прежним Уровень/Опыт + дополнительные)
      stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
      stat_headers.each_with_index do |head, idx|
        cx = stat_centers[idx]
        w = Raylib.MeasureTextEx(@font, head, 20, 1).x
        draw_text_custom(head, cx - w / 2, header_y, 20, WHITE)
      end
    end

      # ===== НИЖНЯЯ ПАНЕЛЬ: СПИСОК ПАРТИИ (с прокруткой) =====
    VISIBLE_ROWS.times do |i|
      list_index = @list_top_index + i
      break if list_index >= @party.length
      member = @party[list_index]
      y = @lower_y + 71 + i * 34

      # Подсветка выбранного персонажа
            # Подсветка только для выбранного персонажа
      if member["name"] == @current_actor
        pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
        alpha = (pulse * 255).to_i
        highlight = Raylib.Fade(Raylib::BLUE, alpha / 255.0)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      end

      # Рубин для всех членов отряда
      if @ruby_tex
        ruby_src = Raylib::Rectangle.create(0, 0, @ruby_tex.width, @ruby_tex.height)
        ruby_dst = Raylib::Rectangle.create(@lower_x + 15, y - 3, 24, 24)
        Raylib.DrawTexturePro(@ruby_tex, ruby_src, ruby_dst,
        Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # Стрелка вверх (на первой строке, если есть скрытые сверху)
      if i == 0 && @list_top_index > 0
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        ax = @lower_x + 27          # середина места рубина (15 + 24/2)
        ay = y + 12                 # центр строки
        Raylib.DrawTriangle(
          Raylib::Vector2.create(ax, ay - 6),
          Raylib::Vector2.create(ax - 6, ay + 4),
          Raylib::Vector2.create(ax + 6, ay + 4),
          color
        )
      end
      # Стрелка вниз (на последней видимой строке, если есть скрытые снизу)
      if i == VISIBLE_ROWS - 1
        ax = @lower_x + 27
        ay = y + 12
        visible = (@list_top_index + VISIBLE_ROWS < @party.length)
        alpha = visible ? ((Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255).to_i : 0
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        Raylib.DrawTriangle(
          Raylib::Vector2.create(ax - 6, ay - 4),  # левый верх
          Raylib::Vector2.create(ax, ay + 6),       # вершина вниз
          Raylib::Vector2.create(ax + 6, ay - 4),   # правый верх
          color
        )
      end

      # Имя (обрезаем до 10 символов)
      name_display = member["name"].slice(0, 10)
      draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

      if @status_view_mode == 0
        # Класс (обрезаем до 10 символов)
        class_name = @class_names[member["class_id"]] || "???"
        class_display = class_name.slice(0, 10)
        draw_text_custom(class_display, @lower_x + 187, y, 18, WHITE)

        # Уровень и опыт (центрированы)
        draw_text_centered_h(member["level"].to_s, level_header_center_x, y, 18, WHITE)
        draw_text_centered_h(member["exp"].to_s,    exp_header_center_x,   y, 18, WHITE)
      else
        # Статы персонажа
        klass = @classes_data.find { |c| c["id"] == member["class_id"] }
        if klass
          hp_start  = klass.dig("hp_growth", "start") || 0
          mp_start  = klass.dig("mp_growth", "start") || 0
          atk_start = klass.dig("attack_growth", "start") || 0
          def_start = klass.dig("defense_growth", "start") || 0
          agi_start = klass.dig("agility_growth", "start") || 0
          mov       = klass["move"] || 0
        else
          hp_start = mp_start = atk_start = def_start = agi_start = mov = 0
        end

        stat_values = [hp_start, mp_start, atk_start, def_start, agi_start, mov]
        stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]

        stat_values.each_with_index do |val, idx|
          draw_text_centered_h(val.to_s, stat_centers[idx], y, 18, WHITE)
        end
      end
    end
 end
end 
# ============================================
# ОКНО ПРОФАЙЛА (Character Profile)
# ============================================
class Profile
  def initialize(font = nil)
    @font = font
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
	@blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	@portrait_cache = {}
	@icon_cache = {}
	@mapsprite_cache = {}
    @sprite_frame = 0
    @sprite_timer = 0
    @sprite_speed = 14   # кадров между сменой кадров

    # Позиции панелей
    @right_panel_target_x = 182      # правая панель на том же месте, где была верхняя в статусе
    @right_panel_target_y = 16
    @sub_panel_target_x = 48         # маленькая панель под портретом
    @sub_panel_target_y = 224        # (портрет заканчивается на y=16+208=224, как раз под ним)

    @right_panel_start_x = 576 + 220
    @sub_panel_start_y = 480 + 220

    @right_panel_x = @right_panel_start_x
    @right_panel_y = @right_panel_target_y
    @sub_panel_x = @sub_panel_target_x
    @sub_panel_y = @sub_panel_start_y

    # Размеры панелей
    @right_panel_w = 346
    @right_panel_h = 448
    @sub_panel_w = 134
    @sub_panel_h = 240         # маленькая панель

    # Портрет и рамка (как в статусе, позиции те же)
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16

    @portrait_start_x = -220
    @frame_start_x = -220

    @portrait_x = @portrait_start_x
    @portrait_y = @portrait_target_y
    @frame_x = @frame_start_x
    @frame_y = @frame_target_y

    @portrait_tex = nil
    @blink_tex = nil

    load_textures
  end

  def load_textures
    @right_panel_tex = Raylib.LoadTexture("assets/ui/right_panel.png")
    @sub_panel_tex   = Raylib.LoadTexture("assets/ui/sub_panel.png")
    @frame_tex       = Raylib.LoadTexture("assets/ui/portrait_frame.png")

    Raylib.SetTextureFilter(@right_panel_tex, 0) if @right_panel_tex
    Raylib.SetTextureFilter(@sub_panel_tex, 0)   if @sub_panel_tex
    Raylib.SetTextureFilter(@frame_tex, 0)       if @frame_tex
  end

  def open(actor_name, party, class_names, classes_data, portrait_cache, start_inventory)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false
	@current_actor = actor_name

    # Сохраняем ссылки на данные
    @party = party
    @class_names = class_names
    @classes_data = classes_data
    @portrait_cache = portrait_cache
    @start_inventory = start_inventory
	
	# Загружаем общее золото из data/global.json
    @gold = 0
  if File.exist?("data/global.json")
    data = JSON.parse(File.read("data/global.json"))
    @gold = data["gold"] || 0
  end

    # Загружаем портрет
    actor = @party.find { |a| a["name"] == actor_name }
    if actor
      portrait_name = actor["portrait"] || actor["name"]
      @portrait_tex = load_portrait(portrait_name)
      @blink_tex = load_blink_portrait(portrait_name)
    end
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	@sprite_timer = 0
    @sprite_frame = 0
		
	# Загружаем mapsprite для анимации в нижней панели
    mapsprite_name = actor ? actor["mapsprite"] : nil
    @mapsprite_tex = mapsprite_name ? load_mapsprite(mapsprite_name) : nil

    # Начальные позиции для анимации
    @right_panel_x = @right_panel_start_x
    @sub_panel_y = @sub_panel_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
  end

  def load_portrait(name)
    return nil unless name
    return @portrait_cache[name] if @portrait_cache.key?(name)

    path = "assets/ui/portraits/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[name] = tex
    tex
  end

  def load_blink_portrait(name)
    return nil unless name
    cache_key = "#{name}_blink"
    return @portrait_cache[cache_key] if @portrait_cache.key?(cache_key)

    path = "assets/ui/portraits/#{name}_blink.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[cache_key] = tex
    tex
  end
# def update Profile
    def update
    return unless @visible
    speed = 38

    case @anim_phase
    when 1  # сборка
      @portrait_x += speed
      @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed
      @frame_x = @frame_target_x if @frame_x > @frame_target_x

      @right_panel_x -= speed
      @right_panel_x = @right_panel_target_x if @right_panel_x < @right_panel_target_x

      @sub_panel_y -= speed
      @sub_panel_y = @sub_panel_target_y if @sub_panel_y < @sub_panel_target_y

      if @portrait_x >= @portrait_target_x &&
         @frame_x >= @frame_target_x &&
         @right_panel_x <= @right_panel_target_x &&
         @sub_panel_y <= @sub_panel_target_y
        @anim_phase = 2
      end

    when 3  # разборка
      @portrait_x -= speed
      @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed
      @frame_x = @frame_start_x if @frame_x < @frame_start_x

      @right_panel_x += speed
      @right_panel_x = @right_panel_start_x if @right_panel_x > @right_panel_start_x

      @sub_panel_y += speed
      @sub_panel_y = @sub_panel_start_y if @sub_panel_y > @sub_panel_start_y

      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
        @ready_to_close = true
      end
    end

    # Моргание (только когда полностью открыто) Profile
        if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
      # Анимация спрайта (шаг вниз)
      @sprite_timer += 1
      if @sprite_timer >= @sprite_speed
        @sprite_timer = 0
        @sprite_frame = (@sprite_frame + 1) % 2
      end
    end
  end

  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end

  def force_close
    @visible = false
    @anim_phase = 0
  end

  def draw
    return unless @visible

    origin = Raylib::Vector2.create(0, 0)

    # Правая панель Profile
    dst = Raylib::Rectangle.create(@right_panel_x, @right_panel_y, @right_panel_w, @right_panel_h)
    src = Raylib::Rectangle.create(0, 0, @right_panel_w, @right_panel_h)
    Raylib.DrawTexturePro(@right_panel_tex, src, dst, origin, 0, Raylib::WHITE)

    # Маленькая панель под портретом Profile
    dst = Raylib::Rectangle.create(@sub_panel_x, @sub_panel_y, @sub_panel_w, @sub_panel_h)
    src = Raylib::Rectangle.create(0, 0, @sub_panel_w, @sub_panel_h)
    Raylib.DrawTexturePro(@sub_panel_tex, src, dst, origin, 0, Raylib::WHITE)

    # Портрет Profile
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      src = Raylib::Rectangle.create(0, 0, 134, 208)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end

    # Рамка портрета Profile
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, 134, 208)
    src = Raylib::Rectangle.create(0, 0, 134, 208)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)

    # ===== ТЕКСТ НА ПРАВОЙ ПАНЕЛИ =====
      actor = @party.find { |a| a["name"] == @current_actor } if @current_actor
    if actor
      class_id = actor["class_id"]
      klass = @classes_data.find { |c| c["id"] == class_id }
	  
      class_full_name = klass ? (klass["full_name"] || klass["name"]) : "???"
      class_full_name = class_full_name.slice(0, 16)
      actor_name = actor["name"].slice(0, 10)

      # Класс – платиновым (золотистым)
      draw_text_custom(class_full_name, @right_panel_x + 25, @right_panel_y + 12, 20, GOLD)
      class_width = Raylib.MeasureTextEx(@font, class_full_name, 20, 1).x
      space_width = 12   # фиксированный отступ ≈ два пробела
      name_x = @right_panel_x + 25 + class_width + space_width
      # Имя – белым
      draw_text_custom(actor_name, name_x, @right_panel_y + 12, 20, WHITE)

      # Статы персонажа (пока начальные, позже – по кривой роста)
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      if klass
        hp_start  = klass.dig("hp_growth", "start") || 0
        mp_start  = klass.dig("mp_growth", "start") || 0
        atk_start = klass.dig("attack_growth", "start") || 0
        def_start = klass.dig("defense_growth", "start") || 0
        agi_start = klass.dig("agility_growth", "start") || 0
        mov       = klass["move"] || 0
      else
        hp_start = mp_start = atk_start = def_start = agi_start = mov = 0
      end

      lv = actor["level"]
      exp = actor["exp"] || 0

      # Статы (шрифт 22, интервал 32)
      left_x = @right_panel_x + 55
      right_x = @right_panel_x + 210
      y_base = @right_panel_y + 55
      line_h = 28 # интервал

      # Левый столбец
      draw_text_custom("LV    #{lv}", left_x, y_base, 20, WHITE)
      draw_text_custom("HP    #{hp_start}", left_x, y_base + line_h, 20, WHITE)
      draw_text_custom("MP    #{mp_start}", left_x, y_base + line_h * 2, 20, WHITE)
      draw_text_custom("EXP   #{exp}", left_x, y_base + line_h * 3, 20, WHITE)

      # Правый столбец
      draw_text_custom("ATT   #{atk_start}", right_x - 50, y_base, 20, WHITE)
      draw_text_custom("DEF   #{def_start}", right_x - 50, y_base + line_h, 20, WHITE)
      draw_text_custom("AGI   #{agi_start}", right_x - 50, y_base + line_h * 2, 20, WHITE)
      draw_text_custom("MOV   #{mov}", right_x - 50, y_base + line_h * 3, 20, WHITE)
	  	  
	        # Получаем заклинания
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
      spells = spell_list.select { |s| s["level"] <= actor["level"] }

            # ── Магия (слева) и Предметы (справа) ──
      section_y = @right_panel_y + 200
      draw_text_custom("Magic", left_x - 30, section_y, 20, WHITE)
      draw_text_custom("Items", right_x - 50, section_y, 20, WHITE)

      # Магия (левый столбец)
      if spells.any?
        spells.first(4).each_with_index do |spell, i|
          y = section_y + 30 + i * 52
          # Иконка 32x48
          spell_icon = load_icon(find_spell_icon(spell["spell"], spell["spell_level"]))
          if spell_icon
            src = Raylib::Rectangle.create(0, 0, 32, 48)
            dst = Raylib::Rectangle.create(left_x - 30, y, 32, 48)
            Raylib.DrawTexturePro(spell_icon, src, dst,
              Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
          end
		  
          draw_text_custom(spell["spell"], left_x + 10, y + 6, 18, WHITE)
          draw_text_custom("Lv #{spell["spell_level"]}", left_x + 10, y + 26, 18, WHITE)
        end
      else
        draw_text_custom("Nothing", left_x, section_y + 30, 18, ORANGE)
      end

            # Предметы (правый столбец)
      inv_entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
      items = inv_entry ? inv_entry["items"] : []

      if items.empty?
        draw_text_custom("Nothing", right_x - 50, section_y + 30, 18, ORANGE)
      else
        items.first(4).each_with_index do |item_entry, i|
          next if item_entry["item"] == "NOTHING"
          y = section_y + 30 + i * 52

          # Иконка 32x48
          item_data = find_item_by_name(item_entry["item"])
          item_icon = item_data ? load_icon(item_data["icon"]) : nil
          if item_icon
            src = Raylib::Rectangle.create(0, 0, 32, 48)
            dst = Raylib::Rectangle.create(right_x - 50, y, 32, 48)
            Raylib.DrawTexturePro(item_icon, src, dst,
              Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
          end

          name = item_entry["item"]
          if item_entry["equipped"]
            draw_text_custom("E", right_x - 15, y + 12, 18, YELLOW)   # жёлтая метка
          end
          draw_item_name(name, right_x + 5, y + 12, 18, WHITE)       # название без префикса
        end
      end
    end
    # Анимированный спрайт персонажа
    if @mapsprite_tex
      src = Raylib::Rectangle.create(@sprite_frame * 48, 2 * 48, 48, 48)   # строка 2 = вниз
      dst = Raylib::Rectangle.create(@sub_panel_x + (134 - 48) / 2, @sub_panel_y + 48, 48, 48)
      Raylib.DrawTexturePro(@mapsprite_tex, src, dst,
        Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)		
	# Счётчики Kills и Defeats (под спрайтом)
    if actor
      kills = actor["kills"] || 0
      defeats = actor["defeats"] || 0
      text_x = @sub_panel_x + 15
      text_y = @sub_panel_y + 106

      draw_text_custom("KILLS", text_x, text_y, 18, WHITE)
      # число — максимум 4 знака, зелёное
      draw_text_custom([kills, 9999].min.to_s.rjust(4), text_x + 70, text_y, 18, GREEN)

      draw_text_custom("DEFEAT", text_x, text_y + 25, 18, WHITE)
      # число — максимум 4 знака, красное
      draw_text_custom([defeats, 9999].min.to_s.rjust(4), text_x + 70, text_y + 25, 18, RED)
	  
      # Золото – заголовок и центрированное под ним значение (шрифт золота крупнее)
      gold_y = text_y + 76
      draw_text_custom("GOLD", text_x + 28, gold_y, 20, WHITE)

      gold_digits = [@gold, 9999999999].min.to_s.chars.join(' ')
      gold_header_width = Raylib.MeasureTextEx(@font, "GOLD", 18, 1).x
      gold_center_x = (text_x + 28) + gold_header_width / 2
      gold_val_width = Raylib.MeasureTextEx(@font, gold_digits, 20, 1).x   # размер шрифта увеличен до 20
      val_x = gold_center_x - gold_val_width / 2
      draw_text_custom(gold_digits, val_x, gold_y + 20, 20, YELLOW)        # и здесь 20
    end
  end
end
# def draw_item_name Profile
def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      # Разбиваем на два слова по первому пробелу
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip

      # Первая строка — как обычно
      draw_text_custom(first_word, x, y, size, color)

      # Вторая строка — смещена вниз (как в JS: itemSplitOffset = 15) и вправо (secondLineIndent = 14)
      draw_text_custom(second_word, x + 14, y + 15, size, color)
    else
      draw_text_custom(text, x, y, size, color)
    end
  end
  # Загрузка mapsprite на панели снизу портрета
  def load_mapsprite(name)
    return nil unless name
    return @mapsprite_cache[name] if @mapsprite_cache.key?(name)

    path = "assets/mapsprites/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @mapsprite_cache[name] = tex
    tex
  end

  # Загрузка иконки с кешем
  def load_icon(path)
    return nil unless path && !path.empty?
    return @icon_cache[path] if @icon_cache.key?(path)

    tex = nil
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, 0)
    end
    @icon_cache[path] = tex
    tex
  end

  # Найти предмет по имени в items.json
  def find_item_by_name(name)
    unless @items_data
      if File.exist?("data/items/items.json")
        data = JSON.parse(File.read("data/items/items.json"))
        @items_data = data["items"] || []
      else
        @items_data = []
      end
    end
    @items_data.find { |item| item["name"] == name }
  end

  # Найти иконку заклинания по имени и уровню в spells.json
  def find_spell_icon(name, level)
    unless @spells_data
      if File.exist?("data/spells/spells.json")
        data = JSON.parse(File.read("data/spells/spells.json"))
        @spells_data = data["spells"] || []
      else
        @spells_data = []
      end
    end
    spell = @spells_data.find { |s| s["name"].casecmp?(name) && s["level"] == level }
    spell ? spell["icon"] : nil
  end

  # Вспомогательный метод для текста
  def draw_text_custom(text, x, y, size, color)
    if @font
      Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
    else
      Raylib.DrawText(text, x, y, size, color)
    end
  end
end

# ============================================
# ОВЕРЛЕЙ МАГИИ (Magic Overlay)
# ============================================
class MagicOverlay

  def get_actor_stats(actor_name)
    @party.each do |actor|
      if actor["name"] == actor_name
        return actor
      end
    end
    nil
  end

def find_actor_items(actor_name)
  actor = @party.find { |a| a["name"] == actor_name }
  return [] unless actor

  entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
  return [] unless entry
  entry["items"] || []
end

def change_selected_actor(delta)
  return unless @party.any?
  new_index = @selected_actor_index + delta
  return if new_index < 0 || new_index >= @party.length

  @selected_actor_index = new_index

  # Автоскролл (как в JS)
  if @selected_actor_index < @list_top_index
    @list_top_index = @selected_actor_index
  elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
    @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
  end

  # Обновляем текущего персонажа
  @current_actor = @party[@selected_actor_index]["name"]
  @current_items = find_actor_items(@current_actor)
  actor = @party[@selected_actor_index]
  portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
  @portrait_tex = load_portrait(portrait_name)
  @blink_tex = load_blink_portrait(portrait_name)

  # Обновляем заклинания
  actor = @party[@selected_actor_index]
  if actor
    klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
    spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
    @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
  end
end

  VISIBLE_ROWS = 5
  attr_reader :current_actor
  def initialize(font = nil)
    @font = font
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
	@blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	@portrait_cache = {}
	
    # Загружаем классы
    @class_names = {}
    @classes_data = []
   if File.exist?("data/actors/classes.json")
    data = JSON.parse(File.read("data/actors/classes.json"))
    @classes_data = data["classes"] || []
    @classes_data.each { |c| @class_names[c["id"]] = c["name"] }
   end
	
    # Загружаем заклинания
    if File.exist?("data/spells/spells.json")
     data = JSON.parse(File.read("data/spells/spells.json"))
     @all_spells = data["spells"] || []
    else
      @all_spells = []
    end
	
	# Загружаем начальный инвентарь
    if File.exist?("data/actors/start_inventory.json")
     data = JSON.parse(File.read("data/actors/start_inventory.json"))
     @start_inventory = data["start_inventory"] || []
    else
     @start_inventory = []
    end
    
    # Целевые позиции
    @upper_target_x = 182
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16
    
    # Стартовые позиции
    @upper_start_x = 576 + 220
    @lower_start_y = 480 + 220
    @portrait_start_x = -220
    @frame_start_x = -220
    
    # Текущие позиции
    @upper_x = @upper_start_x
    @upper_y = @upper_target_y
    @lower_x = @lower_target_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @portrait_y = @portrait_target_y
    @frame_x = @frame_start_x
    @frame_y = @frame_target_y
    
    # Размеры
    @upper_w = 346
    @upper_h = 208
    @lower_w = 480
    @lower_h = 240
    @portrait_w = 134
    @portrait_h = 208
    @frame_w = 134
    @frame_h = 208
    
    # Моргание
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	# Плавная пульсация рамки выбора
    @selection_blink_timer = 0
	# Режим просмотра нижней панели: 0 = класс/уровень/опыт, 1 = статы
    @status_view_mode = 0
	@list_top_index = 0
    @selected_actor_index = 0      # индекс текущего персонажа в party
	@input_timer_up = 0
    @input_timer_down = 0
	@empty_magic_tex = nil
    @empty_magic_tex_loaded = false
	@icon_cache = {}
    @empty_magic_tex = nil
    
    load_textures
    load_actors
  end

  # Вспомогательный метод для рисования текста кастомным шрифтом
  def draw_text_custom(text, x, y, size, color)
    if @font
      Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
    else
      Raylib.DrawText(text, x, y, size, color)
    end
  end
  
  def draw_text_centered_h(text, cx, y, size, color)
    return unless @font
    vec = Raylib.MeasureTextEx(@font, text, size, 1)
    x = cx - vec.x / 2
    Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
  end
  
  def load_textures
    @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
    @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
    @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
	@ruby_tex = Raylib.LoadTexture("assets/ui/ruby_icon.png")
    
    Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
    Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
    Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
	Raylib.SetTextureFilter(@ruby_tex, 0) if @ruby_tex
	
	if File.exist?("assets/items/item_empty.png")
      @empty_magic_tex = Raylib.LoadTexture("assets/items/item_empty.png")
      Raylib.SetTextureFilter(@empty_magic_tex, 0)
    end
  end
  
  def load_actors
    if File.exist?("data/actors/actors.json")
      data = JSON.parse(File.read("data/actors/actors.json"))
      @party = data["actors"] || []
    else
      @party = []
    end
  end
 
   # Рисует название предмета с переносом, если оно из двух слов (как в SF2)
  def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      # Разбиваем на два слова по первому пробелу
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip

      # Первая строка — как обычно
      draw_text_custom(first_word, x, y, size, color)

      # Вторая строка — смещена вниз (как в JS: itemSplitOffset = 15) и вправо (secondLineIndent = 14)
      draw_text_custom(second_word, x + 14, y + 15, size, color)
    else
      draw_text_custom(text, x, y, size, color)
    end
  end
 
def load_portrait(name)
  return nil unless name
  return @portrait_cache[name] if @portrait_cache.key?(name)

  path = "assets/ui/portraits/#{name}.png"
  return nil unless File.exist?(path)
  img = Raylib.LoadImage(path)
  tex = Raylib.LoadTextureFromImage(img)
  Raylib.UnloadImage(img)
  Raylib.SetTextureFilter(tex, 0)
  @portrait_cache[name] = tex
  tex
end
  
def load_blink_portrait(name)
  return nil unless name
  cache_key = "#{name}_blink"
  return @portrait_cache[cache_key] if @portrait_cache.key?(cache_key)

  path = "assets/ui/portraits/#{name}_blink.png"
  return nil unless File.exist?(path)
  img = Raylib.LoadImage(path)
  tex = Raylib.LoadTextureFromImage(img)
  Raylib.UnloadImage(img)
  Raylib.SetTextureFilter(tex, 0)
  @portrait_cache[cache_key] = tex
  tex
end
  
  def open(player = nil)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false
    
    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
    
  if @party.any?
    # Проверяем, не выходит ли сохранённый индекс за пределы
    if @selected_actor_index >= @party.length
      @selected_actor_index = @party.length - 1
    end
    # Скролл подгоняем, чтобы выбранный персонаж был виден
    if @selected_actor_index < @list_top_index
      @list_top_index = @selected_actor_index
    elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
      @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
    end
    @current_actor = @party[@selected_actor_index]["name"]

    # Находим данные актора
    actor = @party.find { |a| a["name"] == @current_actor }
    if actor
      # Ищем класс актора по class_id
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
      @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
    else
      @current_spells = []
    end

    # Предметы и портреты
    @current_items = find_actor_items(@current_actor)
    actor = @party.find { |a| a["name"] == @current_actor }
    portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
    @portrait_tex = load_portrait(portrait_name)
    @blink_tex = load_blink_portrait(portrait_name)
  else
    @current_actor = nil
    @current_spells = []
    @current_items = []
  end
 # таймеры
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
	@selection_blink_timer = 0
  end
  
  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end
  
  def force_close
    @visible = false
    @anim_phase = 0
  end
  
  def handle_input
  return unless @visible && @anim_phase == 2

  # Закрыть меню
  if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
    close
    return
  end

  # Переключение режима ←/→ (одиночное)
  if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
    @status_view_mode = 1 - @status_view_mode
  end

  # Автоповтор ↑
  if Raylib.IsKeyDown(Raylib::KEY_UP)
    @input_timer_up += 1
    if @input_timer_up == 1 || (@input_timer_up > 20 && (@input_timer_up - 20) % 5 == 0)
      change_selected_actor(-1)
    end
  else
    @input_timer_up = 0
  end

  # Автоповтор ↓
  if Raylib.IsKeyDown(Raylib::KEY_DOWN)
    @input_timer_down += 1
    if @input_timer_down == 1 || (@input_timer_down > 20 && (@input_timer_down - 20) % 5 == 0)
      change_selected_actor(1)
    end
  else
    @input_timer_down = 0
  end
end
  
  def update
    return unless @visible
    
    speed = 38
    
    case @anim_phase
    when 1
      @portrait_x += speed
      @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed
      @frame_x = @frame_target_x if @frame_x > @frame_target_x
      
      @upper_x -= speed
      @upper_x = @upper_target_x if @upper_x < @upper_target_x
      
      @lower_y -= speed
      @lower_y = @lower_target_y if @lower_y < @lower_target_y
      
      if @portrait_x >= @portrait_target_x && 
         @frame_x >= @frame_target_x &&
         @upper_x <= @upper_target_x && 
         @lower_y <= @lower_target_y
        @anim_phase = 2
      end
      
    when 3
      @portrait_x -= speed
      @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed
      @frame_x = @frame_start_x if @frame_x < @frame_start_x
      
      @upper_x += speed
      @upper_x = @upper_start_x if @upper_x > @upper_start_x
      
      @lower_y += speed
      @lower_y = @lower_start_y if @lower_y > @lower_start_y
      
      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
      end
    end
    
    if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
	  # Пульсация рамки выбора
      @selection_blink_timer += 1
    end
  end
  
  def draw
    return unless @visible

    origin = Raylib::Vector2.create(0, 0)

    # Нижняя панель
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    src = Raylib::Rectangle.create(0, 0, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, src, dst, origin, 0, Raylib::WHITE)

    # Верхняя панель
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    src = Raylib::Rectangle.create(0, 0, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, src, dst, origin, 0, Raylib::WHITE)

    # Портрет
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      src = Raylib::Rectangle.create(0, 0, 134, 208)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end

    # Рамка
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)

    # ===== ВЕРХНЯЯ ПАНЕЛЬ: МАГИЯ (иконки крестом, текст как предметы в статусе) =====
    actor_data = @party.find { |a| a["name"] == @current_actor }
    if actor_data
      class_id   = actor_data["class_id"]
      class_name = @class_names[class_id] || "???"
      level      = actor_data["level"]
      header     = "#{actor_data["name"]}  #{class_name}  LV #{level}"
      draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
    else
      draw_text_custom("NO DATA", @upper_x + 25, @upper_y + 12, 20, WHITE)
    end

    draw_text_custom("-- MAGIC --", @upper_x + 47, @upper_y + 35, 20, WHITE)

    # === КРЕСТ ИКОНОК (не трогаем) ===
    base_x = @upper_x + 40
    base_y = @upper_y + 60
    offset_x = 44
    offset_y = 42

    icon_positions = [
      { x: base_x + offset_x, y: base_y + 8 },               # верхняя
      { x: base_x,            y: base_y + offset_y },         # левая
      { x: base_x + offset_x * 2, y: base_y + offset_y },    # правая
      { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 } # нижняя
    ]

    # === ТЕКСТ КАК ПРЕДМЕТЫ В СТАТУСЕ ===
    text_x = @upper_x + 195           # как "Предметы" в StatusOverlay (правая колонка)
    text_y = @upper_y + 48            # начало списка
    text_line_h = 36                  # интервал, как у предметов

    spells = @current_spells.first(4) if @current_spells

    # Загрузка пустой текстуры
    unless @empty_magic_tex_loaded
      if File.exist?("assets/spell/magic_empty.png")
        @empty_magic_tex = Raylib.LoadTexture("assets/spell/magic_empty.png")
        Raylib.SetTextureFilter(@empty_magic_tex, 0) if @empty_magic_tex
      end
      @empty_magic_tex_loaded = true
    end

    # Рисуем иконки
    (0..3).each do |i|
      ipos = icon_positions[i]
      spell = spells[i] if spells && i < spells.length

      if spell
        icon = load_icon(find_spell_icon(spell["spell"], spell["spell_level"]))
        if icon
          src = Raylib::Rectangle.create(0, 0, 32, 48)
          dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
          Raylib.DrawTexturePro(icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end
      else
        if @empty_magic_tex
          dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
          Raylib.DrawTexturePro(@empty_magic_tex, Raylib::Rectangle.create(0,0,32,48), dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
        else
          Raylib.DrawRectangle(ipos[:x], ipos[:y], 32, 48, Raylib::GRAY)
        end
      end
    end

    # Рисуем текст столбиком
    (0..3).each do |i|
      spell = spells[i] if spells && i < spells.length
      next unless spell

      y = text_y + i * text_line_h
      draw_text_custom(spell["spell"], text_x, y, 20, PURPLE)
      draw_text_custom("level #{spell["spell_level"]}", text_x + 14, y + 18, 20, LIME)
    end
	
    # ===== НИЖНЯЯ ПАНЕЛЬ: ЗАГОЛОВКИ =====
    header_y = @lower_y + 28

    if @status_view_mode == 0
      draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
      draw_text_custom("Класс",  @lower_x + 187, header_y, 20, WHITE)

      level_header_center_x = @lower_x + 290 + Raylib.MeasureTextEx(@font, "Уровень", 20, 1).x / 2
      exp_header_center_x   = @lower_x + 395 + Raylib.MeasureTextEx(@font, "Опыт", 20, 1).x / 2

      draw_text_custom("Уровень", @lower_x + 290, header_y, 20, WHITE)
      draw_text_custom("Опыт",    @lower_x + 395, header_y, 20, WHITE)
    else
      draw_text_custom("Имя", @lower_x + 44, header_y, 20, WHITE)
      stat_headers = ["HP", "MP", "AT", "DF", "AGI", "MV"]
      stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
      stat_headers.each_with_index do |head, idx|
        cx = stat_centers[idx]
        w = Raylib.MeasureTextEx(@font, head, 20, 1).x
        draw_text_custom(head, cx - w / 2, header_y, 20, WHITE)
      end
    end

    VISIBLE_ROWS.times do |i|
      list_index = @list_top_index + i
      break if list_index >= @party.length
      member = @party[list_index]
      y = @lower_y + 71 + i * 34

      if member["name"] == @current_actor
        pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
        alpha = (pulse * 255).to_i
        highlight = Raylib.Fade(Raylib::BLUE, alpha / 255.0)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      end

      if @ruby_tex
        ruby_src = Raylib::Rectangle.create(0, 0, @ruby_tex.width, @ruby_tex.height)
        ruby_dst = Raylib::Rectangle.create(@lower_x + 15, y - 3, 24, 24)
        Raylib.DrawTexturePro(@ruby_tex, ruby_src, ruby_dst,
        Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      if i == 0 && @list_top_index > 0
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        ax = @lower_x + 27
        ay = y + 12
        Raylib.DrawTriangle(
          Raylib::Vector2.create(ax, ay - 6),
          Raylib::Vector2.create(ax - 6, ay + 4),
          Raylib::Vector2.create(ax + 6, ay + 4),
          color
        )
      end
      if i == VISIBLE_ROWS - 1
        ax = @lower_x + 27
        ay = y + 12
        visible = (@list_top_index + VISIBLE_ROWS < @party.length)
        alpha = visible ? ((Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255).to_i : 0
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        Raylib.DrawTriangle(
          Raylib::Vector2.create(ax - 6, ay - 4),
          Raylib::Vector2.create(ax, ay + 6),
          Raylib::Vector2.create(ax + 6, ay - 4),
          color
        )
      end

      name_display = member["name"].slice(0, 10)
      draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

      if @status_view_mode == 0
        class_name = @class_names[member["class_id"]] || "???"
        class_display = class_name.slice(0, 10)
        draw_text_custom(class_display, @lower_x + 187, y, 18, WHITE)
        # уровень в окне Магия 2 кнопка
        draw_text_centered_h(member["level"].to_s, level_header_center_x, y, 18, LIME)
        draw_text_centered_h(member["exp"].to_s,    exp_header_center_x,   y, 18, LIME)
      else
        klass = @classes_data.find { |c| c["id"] == member["class_id"] }
        if klass
          hp_start  = klass.dig("hp_growth", "start") || 0
          mp_start  = klass.dig("mp_growth", "start") || 0
          atk_start = klass.dig("attack_growth", "start") || 0
          def_start = klass.dig("defense_growth", "start") || 0
          agi_start = klass.dig("agility_growth", "start") || 0
          mov       = klass["move"] || 0
        else
          hp_start = mp_start = atk_start = def_start = agi_start = mov = 0
        end

        stat_values = [hp_start, mp_start, atk_start, def_start, agi_start, mov]
        stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]

        stat_values.each_with_index do |val, idx|
          draw_text_centered_h(val.to_s, stat_centers[idx], y, 18, LIME)
        end
      end
    end
 end
 
   def load_icon(path)
    return nil unless path && !path.empty?
    return @icon_cache[path] if @icon_cache.key?(path)

    tex = nil
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, 0)
    end
    @icon_cache[path] = tex
    tex
  end

  def find_spell_icon(name, level)
    unless @spells_data
      if File.exist?("data/spells/spells.json")
        data = JSON.parse(File.read("data/spells/spells.json"))
        @spells_data = data["spells"] || []
      else
        @spells_data = []
      end
    end
    spell = @spells_data.find { |s| s["name"].casecmp?(name) && s["level"] == level }
    spell ? spell["icon"] : nil
  end
end 