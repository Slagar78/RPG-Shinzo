# lib/ui.rb
require 'json'

# ============================================
# МЕНЮ 4 ПЛИТКИ (Bottom Menu)
# ============================================
class BottomMenu
  def initialize
    load_tiles
    @visible = false
    @selected_index = 0
    @anim_timer = 0
    @tile_size = 48
    @offset = 48
    load_textures
  end
  
  def load_tiles
    if File.exist?("data/menu.json")
      data = JSON.parse(File.read("data/menu.json"))
      @tiles = data["tiles"]
    else
      @tiles = [
        { "id" => 0, "name" => "status", "icon" => "assets/ui/menu/status.png", "icon_anim" => "assets/ui/menu/status_anim.png" },
        { "id" => 1, "name" => "magic", "icon" => "assets/ui/menu/magic.png", "icon_anim" => "assets/ui/menu/magic_anim.png" },
        { "id" => 2, "name" => "items", "icon" => "assets/ui/menu/items.png", "icon_anim" => "assets/ui/menu/items_anim.png" },
        { "id" => 3, "name" => "event", "icon" => "assets/ui/menu/event.png", "icon_anim" => "assets/ui/menu/event_anim.png" }
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
        use_anim = (@anim_timer % 24) < 12
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

  def initialize(font = nil)
    @font = font
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
	
    # Загружаем заклинания
    if File.exist?("data/spells.json")
      data = JSON.parse(File.read("data/spells.json"))
      @all_spells = data["spells"] || []
    else
      @all_spells = []
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
  
  def load_textures
    @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
    @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
    @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
    
    Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
    Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
    Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
  end
  
  def load_actors
    if File.exist?("data/actors.json")
      data = JSON.parse(File.read("data/actors.json"))
      @party = data["actors"] || []
    else
      @party = []
    end
  end
  
  def load_portrait(name)
    return nil unless name
    path = "assets/ui/portraits/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    tex
  end
  
  def load_blink_portrait(name)
    return nil unless name
    path = "assets/ui/portraits/#{name}_blink.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
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
      @current_actor = @party[0]["name"]
      @portrait_tex = load_portrait(@current_actor)
      @blink_tex = load_blink_portrait(@current_actor)
    end
    
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
    
    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      close
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
      dst = Raylib::Rectangle.create(@portrait_x + 2, @portrait_y + 2, @portrait_w - 4, @portrait_h - 4)
      src = Raylib::Rectangle.create(0, 0, @portrait_w, @portrait_h)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end
    
    # Рамка
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)
    
    # ===== ТЕКСТ (теперь через кастомный шрифт) =====
    draw_text_custom("MUSHRA  MAGE  LV 18", @upper_x + 25, @upper_y + 12, 20, WHITE)
    draw_text_custom("Магия", @upper_x + 25, @upper_y + 42, 24, WHITE)
    draw_text_custom("Предметы", @upper_x + 195, @upper_y + 38, 24, WHITE)
    
    draw_text_custom("BLAZE Lv2", @upper_x + 25, @upper_y + 72, 20, WHITE)
    draw_text_custom("MUDDLE Lv1", @upper_x + 25, @upper_y + 106, 20, WHITE)
    draw_text_custom("DISPEL Lv1", @upper_x + 25, @upper_y + 140, 20, WHITE)
    draw_text_custom("DESOUL Lv1", @upper_x + 25, @upper_y + 174, 20, WHITE)
    
    draw_text_custom("Medical Herb", @upper_x + 195, @upper_y + 64, 18, WHITE)
    draw_text_custom("Healing Seed", @upper_x + 195, @upper_y + 97, 18, WHITE)
    
    # Нижняя панель — список партии
    @party.each_with_index do |member, i|
      y = @lower_y + 71 + i * 34
      
      if member["name"] == @current_actor
        pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
        alpha = (pulse * 255).to_i
        highlight = Raylib.Fade(Raylib::BLUE, alpha / 255.0)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      end
      
      draw_text_custom(member["name"], @lower_x + 44, y, 18, WHITE)
      draw_text_custom(member["class"], @lower_x + 187, y, 18, WHITE)
      draw_text_custom(member["level"].to_s, @lower_x + 290, y, 18, WHITE)
      draw_text_custom(member["exp"].to_s, @lower_x + 395, y, 18, WHITE)
    end
  end
end