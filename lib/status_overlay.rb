# lib/status_overlay.rb
require 'json'

class StatusOverlay
  def initialize
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    
    @upper_target_x = 180
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    
    @upper_width = 380
    @upper_height = 200
    @lower_width = 560
    @lower_height = 520
    @portrait_width = 134
    @portrait_height = 208
    
    @upper_x = 576 + 220
    @upper_y = @upper_target_y
    @lower_x = @lower_target_x
    @lower_y = 480 + 220
    @portrait_x = -220
    @portrait_y = 16
    
    @current_actor = nil
    @portrait_tex = nil
    @blink_tex = nil
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    
    @view_mode = 0
    @party = []
    @selected_index = 0
    
    load_textures
    load_actors
  end
  
  def load_textures
    # Все файлы лежат прямо в assets/ui/
    @upper_panel = LoadTexture("assets/ui/upper_panel.png")
    @lower_panel = LoadTexture("assets/ui/lower_panel.png")
    @portrait_frame = LoadTexture("assets/ui/portrait_frame.png")
    
    SetTextureFilter(@upper_panel, TEXTURE_FILTER_POINT)
    SetTextureFilter(@lower_panel, TEXTURE_FILTER_POINT)
    SetTextureFilter(@portrait_frame, TEXTURE_FILTER_POINT)
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
    path = "assets/ui/portraits/#{name}.png"
    if File.exist?(path)
      img = LoadImage(path)
      tex = LoadTextureFromImage(img)
      UnloadImage(img)
      SetTextureFilter(tex, TEXTURE_FILTER_POINT)
      tex
    else
      nil
    end
  end
  
  def load_blink_portrait(name)
    path = "assets/ui/portraits/#{name}_blink.png"
    if File.exist?(path)
      img = LoadImage(path)
      tex = LoadTextureFromImage(img)
      UnloadImage(img)
      SetTextureFilter(tex, TEXTURE_FILTER_POINT)
      tex
    else
      nil
    end
  end
  
  def open(player)
    @visible = true
    @anim_phase = 1
    @anim_timer = 0
    @view_mode = 0
    @selected_index = 0
    
    if @party.any?
      @current_actor = @party[0]["name"]
      @portrait_tex = load_portrait(@current_actor)
      @blink_tex = load_blink_portrait(@current_actor)
    end
  end
  
  def close
    @visible = false
  end
  
  def handle_input
    return unless @visible && @anim_phase == 2
    
    if IsKeyPressed(KEY_S) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      @anim_phase = 3
      @anim_timer = 0
      return
    end
    
    if IsKeyPressed(KEY_UP)
      @selected_index = [@selected_index - 1, 0].max
      update_current_actor
    end
    if IsKeyPressed(KEY_DOWN)
      @selected_index = [@selected_index + 1, @party.size - 1].min
      update_current_actor
    end
    
    if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)
      @view_mode = 1 - @view_mode
    end
  end
  
  def update_current_actor
    if @party.any? && @selected_index < @party.size
      @current_actor = @party[@selected_index]["name"]
      @portrait_tex = load_portrait(@current_actor)
      @blink_tex = load_blink_portrait(@current_actor)
    end
  end
  
  def update
    return unless @visible
    
    speed = 38
    
    case @anim_phase
    when 1
      @portrait_x = [@portrait_x + speed, 48].min
      @upper_x = [@upper_x - speed, @upper_target_x].max
      @lower_y = [@lower_y - speed, @lower_target_y].max
      
      if @portrait_x >= 48 && @upper_x <= @upper_target_x && @lower_y <= @lower_target_y
        @anim_phase = 2
      end
      
    when 3
      @portrait_x = [@portrait_x - speed, -220].max
      @upper_x = [@upper_x + speed, 576 + 220].min
      @lower_y = [@lower_y + speed, 480 + 220].min
      
      if @portrait_x <= -220 && @upper_x >= 576 + 220 && @lower_y >= 480 + 220
        @anim_phase = 0
        @visible = false
      end
    end
    
    # Моргание
    @blink_timer += 1
    if @blink_duration > 0
      @blink_duration -= 1
    elsif @blink_timer >= @blink_interval
      @blink_duration = 8
      @blink_timer = 0
      @blink_interval = 100 + rand(50)
    end
  end
  
  def draw
    return unless @visible
    
    # Верхняя панель
    dst = Rectangle.create
    dst.x = @upper_x
    dst.y = @upper_y
    dst.width = @upper_width
    dst.height = @upper_height
    DrawTexturePro(@upper_panel, Rectangle.create(0, 0, @upper_width, @upper_height), dst, Vector2.create(0, 0), 0, WHITE)
    
    # Нижняя панель
    dst.x = @lower_x
    dst.y = @lower_y
    dst.width = @lower_width
    dst.height = @lower_height
    DrawTexturePro(@lower_panel, Rectangle.create(0, 0, @lower_width, @lower_height), dst, Vector2.create(0, 0), 0, WHITE)
    
    # Рамка портрета
    dst.x = @portrait_x
    dst.y = @portrait_y
    dst.width = @portrait_width
    dst.height = @portrait_height
    DrawTexturePro(@portrait_frame, Rectangle.create(0, 0, @portrait_width, @portrait_height), dst, Vector2.create(0, 0), 0, WHITE)
    
    # Портрет
    if @portrait_tex
      dst.x = @portrait_x + 2
      dst.y = @portrait_y + 2
      dst.width = @portrait_width - 4
      dst.height = @portrait_height - 4
      
      texture = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      DrawTexturePro(texture, Rectangle.create(0, 0, @portrait_width, @portrait_height), dst, Vector2.create(0, 0), 0, WHITE)
    end
  end
end