# lib/status_overlay.rb
require 'json'

class StatusOverlay
  def initialize
    @visible = false
    @anim_phase = 0  # 0=закрыт,1=открытие,2=открыт,3=закрытие
    @anim_timer = 0
    @ready_to_close = false
    
    # Целевые позиции
    @upper_target_x = 182
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16
    
    # Стартовые позиции (за экраном)
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
    
    load_textures
    load_actors
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
    
    # Сброс моргания
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
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
    
    # ===== АНИМАЦИЯ СБОРКИ/РАЗБОРКИ =====
    speed = 38
    
    case @anim_phase
    when 1  # открытие
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
      
    when 3  # закрытие (разборка)
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
    
    # ===== МОРГАНИЕ ПОРТРЕТА =====
    if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
    end
  end
  
  def draw
    return unless @visible
    
    origin = Raylib::Vector2.create(0, 0)
    
    # 1. Нижняя панель
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    src = Raylib::Rectangle.create(0, 0, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, src, dst, origin, 0, Raylib::WHITE)
    
    # 2. Верхняя панель
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    src = Raylib::Rectangle.create(0, 0, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, src, dst, origin, 0, Raylib::WHITE)
    
    # 3. Портрет (с морганием)
    if @portrait_tex
      # Выбираем текстуру: мигающую или обычную
      if @blink_duration > 0 && @blink_tex
        portrait = @blink_tex
      else
        portrait = @portrait_tex
      end
      
      dst = Raylib::Rectangle.create(@portrait_x + 2, @portrait_y + 2, @portrait_w - 4, @portrait_h - 4)
      src = Raylib::Rectangle.create(0, 0, @portrait_w, @portrait_h)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end
    
    # 4. Рамка (поверх портрета)
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)
  end
end