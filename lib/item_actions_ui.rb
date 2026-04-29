# item_actions_ui.rb
# Общая база для окон работы с предметами (Use / Give / Drop / Equip)
require 'json'

# ============================================================
# Вспомогательный модуль (не влияет на другие окна)
# ============================================================
module ItemUIHelpers
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

  # Рисует название предмета с переносом, если оно из двух слов
  def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip
      draw_text_custom(first_word, x, y, size, color)
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

  def find_actor_items(actor_name)
    actor = @party.find { |a| a["name"] == actor_name }
    return [] unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return [] unless entry
    entry["items"] || []
  end

  def find_item_by_name(name)
    unless @items_data
      if File.exist?("data/actors/items.json")
        data = JSON.parse(File.read("data/actors/items.json"))
        @items_data = data["items"] || []
      else
        @items_data = []
      end
    end
    @items_data.find { |item| item["name"] == name }
  end
end

# ============================================================
# Базовый класс для всех подменю предметов
# ============================================================
class ItemSubMenuBase
  include ItemUIHelpers

  attr_reader :visible, :current_actor, :anim_phase

  def initialize(mode, font, party, classes_data, class_names, start_inventory)
    @mode = mode
    @font = font
    @party = party
    @classes_data = classes_data
    @class_names = class_names
    @start_inventory = start_inventory

    # Кеши
    @portrait_cache = {}
    @icon_cache = {}
    @items_data = nil

    # Видимость и анимация
    @visible = false
    @anim_phase = 0
    @ready_to_close = false

    # Позиции панелей (как в StatusOverlay)
    @upper_target_x = 182
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16

    @upper_start_x = 576 + 220
    @lower_start_y = 480 + 220
    @portrait_start_x = -220
    @frame_start_x = -220

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

    # Таймеры моргания, пульсации
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @selection_blink_timer = 0
    @status_view_mode = 0     # 0 = класс/уровень/опыт, 1 = статы

    # Индексы для выбора персонажа и предмета
    @selected_actor_index = 0
    @list_top_index = 0
    @selected_item_index = 0
    @item_scroll = 0

    # Автоповтор для персонажа
    @input_timer_up = 0
    @input_timer_down = 0

    # Фокус: :party (нижняя панель) или :items (верхняя панель)
    @focus = :party

    # Текстуры панелей
    load_common_textures
  end

  # ----------------------------------------------------------
  # Загрузка общих текстур
  # ----------------------------------------------------------
  def load_common_textures
  @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
  @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
  @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
  @ruby_tex  = Raylib.LoadTexture("assets/ui/ruby_icon.png")

  Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
  Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
  Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
  Raylib.SetTextureFilter(@ruby_tex, 0)  if @ruby_tex

  # Дефолтная иконка предмета (если файла нет, останется nil – тогда рисовать не будем)
  if File.exist?("assets/items/item_empty.png")
    @empty_item_tex = Raylib.LoadTexture("assets/items/item_empty.png")
    Raylib.SetTextureFilter(@empty_item_tex, 0) if @empty_item_tex
  else
    @empty_item_tex = nil
  end
end

  # ----------------------------------------------------------
  # Анимация открытия
  # ----------------------------------------------------------
  def open(actor_name = nil)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false

    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x

    if @party.any?
      if @selected_actor_index >= @party.length
        @selected_actor_index = @party.length - 1
      end
      if @selected_actor_index < @list_top_index
        @list_top_index = @selected_actor_index
      elsif @selected_actor_index >= @list_top_index + 5
        @list_top_index = @selected_actor_index - 4
      end
      update_current_actor
    else
      @current_actor = nil
      @current_items = []
    end

    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @selection_blink_timer = 0
    @selected_item_index = 0
    @item_scroll = 0
    @focus = :party   # начинаем с выбора персонажа
  end

  # ----------------------------------------------------------
  # Закрытие с анимацией
  # ----------------------------------------------------------
  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end

  def force_close
    @visible = false
    @anim_phase = 0
  end

  # ----------------------------------------------------------
  # Обновление состояния текущего актора
  # ----------------------------------------------------------
  def update_current_actor
  @current_actor = @party[@selected_actor_index]["name"]
  @current_items = filter_items(find_actor_items(@current_actor))

  actor = @party[@selected_actor_index]
  portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
  @portrait_tex = load_portrait(portrait_name)
  @blink_tex = load_blink_portrait(portrait_name)

  @selected_item_index = 0
  @item_scroll = 0
end

  # ----------------------------------------------------------
  # Изменение выбранного персонажа (со скроллингом)
  # ----------------------------------------------------------
  def change_selected_actor(delta)
    return unless @party.any?
    new_index = @selected_actor_index + delta
    return if new_index < 0 || new_index >= @party.length

    @selected_actor_index = new_index
    if @selected_actor_index < @list_top_index
      @list_top_index = @selected_actor_index
    elsif @selected_actor_index >= @list_top_index + 5
      @list_top_index = @selected_actor_index - 4
    end
    update_current_actor
  end

  # ----------------------------------------------------------
  # Обновление (движение панелей, моргание)
  # ----------------------------------------------------------
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
      @selection_blink_timer += 1
    end
  end

  # ----------------------------------------------------------
  # Обработка ввода
  # ----------------------------------------------------------
  def handle_input
    return unless @visible && @anim_phase == 2

    case @focus
    when :party
      # ----- Фокус на нижней панели (персонажи) -----
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        close
        return
      end

      if Raylib.IsKeyDown(Raylib::KEY_UP)
        @input_timer_up += 1
        if @input_timer_up == 1 || (@input_timer_up > 20 && (@input_timer_up - 20) % 5 == 0)
          change_selected_actor(-1)
        end
      else
        @input_timer_up = 0
      end

      if Raylib.IsKeyDown(Raylib::KEY_DOWN)
        @input_timer_down += 1
        if @input_timer_down == 1 || (@input_timer_down > 20 && (@input_timer_down - 20) % 5 == 0)
          change_selected_actor(1)
        end
      else
        @input_timer_down = 0
      end

      if Raylib.IsKeyPressed(Raylib::KEY_R)
        @status_view_mode = 1 - @status_view_mode
      end

      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        @focus = :items
        @selected_item_index = 0
      end

    when :items
      # ----- Фокус на верхней панели (предметы) -----
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        @focus = :party
        return
      end

      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @selected_item_index = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @selected_item_index = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @selected_item_index = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @selected_item_index = 3
      end

      if Raylib.IsKeyPressed(Raylib::KEY_A)
        if @current_items && @selected_item_index < @current_items.length
          item_entry = @current_items[@selected_item_index]
          actor = @party[@selected_actor_index]
          if actor && item_entry && item_entry["item"] != "NOTHING"
            confirm_action(item_entry, actor)
          end
        end
      end
    end
  end

  # ----------------------------------------------------------
  # Отрисовка всего окна
  # ----------------------------------------------------------
  def draw
    return unless @visible
    origin = Raylib::Vector2.create(0, 0)

    # Верхняя панель
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    src = Raylib::Rectangle.create(0, 0, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, src, dst, origin, 0, Raylib::WHITE)

    # Нижняя панель
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    src = Raylib::Rectangle.create(0, 0, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, src, dst, origin, 0, Raylib::WHITE)

    # Портрет и рамка
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x + 2, @portrait_y + 2, 130, 204)
      src = Raylib::Rectangle.create(0, 0, 134, 208)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)

    # Контент панелей
    draw_upper_content
    draw_lower_content
  end

  # ----------------------------------------------------------
  # Верхняя панель: предметы текущего персонажа
  # ----------------------------------------------------------
 def draw_upper_content
  # Заголовок
  actor_data = @party[@selected_actor_index]
  if actor_data
    class_id   = actor_data["class_id"]
    class_name = @class_names[class_id] || "???"
    level      = actor_data["level"]
    header     = "#{actor_data["name"]}  #{class_name}  LV #{level}"
    draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
  else
    draw_text_custom("NO DATA", @upper_x + 25, @upper_y + 12, 20, WHITE)
  end

  draw_text_custom("-- ITEMS --", @upper_x + 47, @upper_y + 35, 20, WHITE)

  # Крест иконок (как в MagicOverlay)
  base_x = @upper_x + 40
  base_y = @upper_y + 60
  offset_x = 44
  offset_y = 42
  icon_positions = [
    { x: base_x + offset_x, y: base_y + 8 },               # верхняя (индекс 0)
    { x: base_x,            y: base_y + offset_y },         # левая  (индекс 1)
    { x: base_x + offset_x * 2, y: base_y + offset_y },    # правая (индекс 2)
    { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 } # нижняя (индекс 3)
  ]

  # Если все слоты пустые – показываем одну надпись NOTHING
  if @current_items.nil? || @current_items.none? { |entry| entry && entry["item"] != "NOTHING" }
    draw_text_custom("NOTHING", @upper_x + 195, @upper_y + 48, 18, ORANGE)
  end

  # Текст справа (такой же столбец, как у магии)
  text_x = @upper_x + 195
  text_y = @upper_y + 48
  text_line_h = 36

  4.times do |i|
    ipos = icon_positions[i]
    item_entry = @current_items ? @current_items[i] : nil

    # Определяем, какую иконку рисовать
    if item_entry && item_entry["item"] != "NOTHING"
      # Есть предмет: пробуем загрузить его иконку
      item_data = find_item_by_name(item_entry["item"])
      icon_path = item_data ? item_data["icon"] : nil
      icon_tex = load_icon(icon_path)  # вернёт nil, если файл не найден
    else
      icon_tex = nil
    end

    # Если иконка не найдена – используем дефолтную (empty_item_tex)
    tex_to_draw = icon_tex || @empty_item_tex

    if tex_to_draw
      src = Raylib::Rectangle.create(0, 0, 32, 48)
      dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
      Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    else
      # Если совсем ничего нет – оставляем серый квадрат (на всякий случай)
      Raylib.DrawRectangle(ipos[:x], ipos[:y], 32, 48, Raylib::GRAY)
      Raylib.DrawRectangleLines(ipos[:x], ipos[:y], 32, 48, Raylib::DARKGRAY)
    end

    # Название предмета (справа) – только для заполненных слотов
    if item_entry && item_entry["item"] != "NOTHING"
      y_text = text_y + i * text_line_h
      text_color = (i == @selected_item_index && @focus == :items) ? Raylib::LIME : Raylib::WHITE
      draw_item_name(item_entry["item"], text_x, y_text, 18, text_color)
      if item_entry["equipped"]
        draw_text_custom("E", text_x - 15, y_text + 4, 18, YELLOW)
      end
    end

    # Подсветка текущего выбранного слота (только в режиме выбора предметов)
    if i == @selected_item_index && @focus == :items
      alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
      color = Raylib.Fade(Raylib::GREEN, alpha / 255.0)
      Raylib.DrawRectangleLines(ipos[:x] - 1, ipos[:y] - 1, 34, 50, color)
    end
  end
end
  # ----------------------------------------------------------
  # Нижняя панель: список персонажей (как в StatusOverlay)
  # ----------------------------------------------------------
  def draw_lower_content
    header_y = @lower_y + 28

    if @status_view_mode == 0
      draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
      draw_text_custom("Класс",  @lower_x + 187, header_y, 20, WHITE)
      level_header_center_x = @lower_x + 290 + Raylib.MeasureTextEx(@font, "Уровень", 20, 1).x / 2
      exp_header_center_x   = @lower_x + 395 + Raylib.MeasureTextEx(@font, "Опыт", 20, 1).x / 2
      draw_text_custom("Уровень", @lower_x + 290, header_y, 20, WHITE)
      draw_text_custom("Опыт",    @lower_x + 395, header_y, 20, WHITE)

      5.times do |i|
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

        name_display = member["name"].slice(0, 10)
        draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

        class_name = @class_names[member["class_id"]] || "???"
        class_display = class_name.slice(0, 10)
        draw_text_custom(class_display, @lower_x + 187, y, 18, WHITE)

        draw_text_centered_h(member["level"].to_s, level_header_center_x, y, 18, WHITE)
        draw_text_centered_h(member["exp"].to_s,    exp_header_center_x,   y, 18, WHITE)
      end
    else
      draw_text_custom("Имя", @lower_x + 44, header_y, 20, WHITE)
      stat_headers = ["HP", "MP", "AT", "DF", "AGI", "MV"]
      stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
      stat_headers.each_with_index do |head, idx|
        cx = stat_centers[idx]
        w = Raylib.MeasureTextEx(@font, head, 20, 1).x
        draw_text_custom(head, cx - w / 2, header_y, 20, WHITE)
      end

      5.times do |i|
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

        name_display = member["name"].slice(0, 10)
        draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

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
        stat_values.each_with_index do |val, idx|
          draw_text_centered_h(val.to_s, stat_centers[idx], y, 18, WHITE)
        end
      end
    end

    # Стрелки прокрутки
    if @list_top_index > 0
      alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
      color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
      ax = @lower_x + 27
      ay = @lower_y + 71 + 12
      Raylib.DrawTriangle(
        Raylib::Vector2.create(ax, ay - 6),
        Raylib::Vector2.create(ax - 6, ay + 4),
        Raylib::Vector2.create(ax + 6, ay + 4),
        color
      )
    end
    if @list_top_index + 5 < @party.length
      alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
      color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
      ax = @lower_x + 27
      ay = @lower_y + 71 + 4*34 + 12
      Raylib.DrawTriangle(
        Raylib::Vector2.create(ax - 6, ay - 4),
        Raylib::Vector2.create(ax, ay + 6),
        Raylib::Vector2.create(ax + 6, ay - 4),
        color
      )
    end
  end

  # ----------------------------------------------------------
  # Методы для переопределения в наследниках
  # ----------------------------------------------------------
  def filter_items(items)
    items
  end

  def confirm_action(item_entry, actor)
  end
end

# ============================================================
# Конкретные меню (Use / Give / Drop / Equip)
# ============================================================

class UseMenu < ItemSubMenuBase
  def initialize(font, party, classes_data, class_names, start_inventory)
    super(:use, font, party, classes_data, class_names, start_inventory)
  end

  def filter_items(items)
  items.select do |entry|
    item_data = find_item_by_name(entry["item"])
    item_data && item_data["category"] == "consumable"
  end
end

  def confirm_action(item_entry, actor)
    puts "Использован предмет #{item_entry["item"]} на #{actor["name"]}"
  end
end

class GiveMenu < ItemSubMenuBase
  def initialize(font, party, classes_data, class_names, start_inventory)
    super(:give, font, party, classes_data, class_names, start_inventory)
  end

  def confirm_action(item_entry, actor)
    puts "Передаём предмет #{item_entry["item"]} от #{actor["name"]} -> выбор цели"
  end
end

class DropMenu < ItemSubMenuBase
  def initialize(font, party, classes_data, class_names, start_inventory)
    super(:drop, font, party, classes_data, class_names, start_inventory)
  end

  def confirm_action(item_entry, actor)
    puts "Выброшен предмет #{item_entry["item"]} у #{actor["name"]}"
  end
end

class EquipMenu < ItemSubMenuBase
  def initialize(font, party, classes_data, class_names, start_inventory)
    super(:equip, font, party, classes_data, class_names, start_inventory)
  end

  def filter_items(items)
    items.select do |entry|
      item_data = find_item_by_name(entry["item"])
      item_data && (item_data["type"] == "weapon" || item_data["type"] == "armor")
    end
  end

  def confirm_action(item_entry, actor)
    puts "Экипируем/снимаем #{item_entry["item"]} на #{actor["name"]}"
  end
end