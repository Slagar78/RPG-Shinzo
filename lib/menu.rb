# lib/menu.rb
require 'json'

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
      normal = LoadTexture(tile["icon"])
      anim = LoadTexture(tile["icon_anim"])
      SetTextureFilter(normal, TEXTURE_FILTER_POINT)
      SetTextureFilter(anim, TEXTURE_FILTER_POINT)
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
    
    if IsKeyPressed(KEY_UP)
      @selected_index = 0
    elsif IsKeyPressed(KEY_LEFT)
      @selected_index = 1
    elsif IsKeyPressed(KEY_RIGHT)
      @selected_index = 2
    elsif IsKeyPressed(KEY_DOWN)
      @selected_index = 3
    end
    
    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      # Выбор плитки
      puts "Выбрана плитка: #{@tiles[@selected_index]['name']}"
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
      { x: center_x,           y: center_y - @offset + 24 },  # верх
      { x: center_x - @offset, y: center_y },                  # лево
      { x: center_x + @offset, y: center_y },                  # право
      { x: center_x,           y: center_y + @offset - 24 }    # низ
    ]
    
    (0..3).each do |i|
      tex = @textures[i]
      pos = positions[i]
      
      # Анимация: если выбрана, меняем картинку каждые 12 кадров
      if i == @selected_index
        use_anim = (@anim_timer % 24) < 12
        texture = use_anim ? tex[:anim] : tex[:normal]
      else
        texture = tex[:normal]
      end
      
      dst = Rectangle.create
      dst.x = pos[:x] - @tile_size / 2
      dst.y = pos[:y] - @tile_size / 2
      dst.width = @tile_size
      dst.height = @tile_size
      
      src = Rectangle.create
      src.x = 0
      src.y = 0
      src.width = @tile_size
      src.height = @tile_size
      
      DrawTexturePro(texture, src, dst, Vector2.create(0, 0), 0, WHITE)
    end
  end
end