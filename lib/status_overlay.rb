# lib/status_overlay.rb
require 'json'

class StatusOverlay
  def initialize
    @visible = false
    
    # Верхняя панель (из плагина: x=182)
    @upper_x = 182
    @upper_y = 16
    @upper_w = 346
    @upper_h = 208
    
    # Нижняя панель (из плагина: x=48, y=224)
    @lower_x = 48
    @lower_y = 224
    @lower_w = 480
    @lower_h = 240
    
    # Портрет (из плагина: x=48, y=16)
    @portrait_x = 48
    @portrait_y = 16
    @portrait_w = 134
    @portrait_h = 208
    
    # Рамка (там же где портрет)
    @frame_x = 48
    @frame_y = 16
    @frame_w = 134
    @frame_h = 208
    
    load_textures
    load_actors
  end
  
  def load_textures
    @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
    @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
    @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
  end
  
  def load_actors
    if File.exist?("data/actors.json")
      data = JSON.parse(File.read("data/actors.json"))
      @party = data["actors"] || []
    end
  end
  
  def load_portrait(name)
    path = "assets/ui/portraits/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    tex
  end
  
  def open(player = nil)
    @visible = true
    if @party.any?
      @portrait_tex = load_portrait(@party[0]["name"])
    end
  end
  
  def close
    @visible = false
  end
  
  def handle_input
    return unless @visible
    if Raylib.IsKeyPressed(Raylib::KEY_S) ||
       Raylib.IsKeyPressed(Raylib::KEY_A) ||
       Raylib.IsKeyPressed(Raylib::KEY_D)
      close
    end
  end
  
  def update
  end
  
  def draw
    return unless @visible
    
    origin = Raylib::Vector2.create(0, 0)
    
    # 1. Верхняя панель (x=180, как в плагине)
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    src = Raylib::Rectangle.create(0, 0, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, src, dst, origin, 0, Raylib::WHITE)
    
    # 2. Нижняя панель (x=48, y=224, как в плагине)
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    src = Raylib::Rectangle.create(0, 0, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, src, dst, origin, 0, Raylib::WHITE)
    
    # 3. Портрет
    if @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, @portrait_w, @portrait_h)
      src = Raylib::Rectangle.create(0, 0, @portrait_w, @portrait_h)
      Raylib.DrawTexturePro(@portrait_tex, src, dst, origin, 0, Raylib::WHITE)
    end
    
    # 4. Рамка
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)
  end
end