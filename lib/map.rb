# lib/map.rb
require 'json'

class GameMap
  attr_reader :width, :height

  def initialize(map_path = nil, tile_types_path = "data/tile_types.json")
    # Загружаем первую карту из папки data/maps, если не указана
    if map_path.nil?
      maps = Dir["data/maps/*.json"]
      map_path = maps.first if maps.any?
    end
    if map_path.nil?
      # Если нет карт, создаём пустую карту по умолчанию
      @width = 20
      @height = 15
      @tiles = Array.new(@width) { Array.new(@height, 0) }
      @rot = Array.new(@width) { Array.new(@height, 0) }
      @mirror_x = Array.new(@width) { Array.new(@height, false) }
      @mirror_y = Array.new(@width) { Array.new(@height, false) }
      return
    end

    data = JSON.parse(File.read(map_path))
    @width = data['width']
    @height = data['height']
    @tiles = data['tiles']
    @rot = data['rot']
    @mirror_x = data['mirror_x']
    @mirror_y = data['mirror_y']

    # Загружаем типы тайлов
    if File.exist?(tile_types_path)
      @tile_types = JSON.parse(File.read(tile_types_path))
    else
      @tile_types = []
    end

    # Загружаем тайлсет (нужно передать путь к тайлсету, но у карты есть поле tileset)
    # Для простоты используем фиксированный путь assets/tilesets/tileset.png
    @tileset_path = data['tileset'] || "assets/tilesets/tileset.png"
  end

  def load_textures
    # Это нужно вызывать после инициализации окна Raylib
    if File.exist?(@tileset_path)
      @tileset_texture = Raylib.LoadTexture(@tileset_path)
      # Предполагаем, что тайлы 48x48
      @tile_width = 48
      @tile_height = 48
      @cols = @tileset_texture.width / @tile_width
    else
      @tileset_texture = nil
    end
  end

  def draw
    return unless @tileset_texture
    (0...@width).each do |x|
      (0...@height).each do |y|
        tile_id = @tiles[x][y]
        src_x = (tile_id % @cols) * @tile_width
        src_y = (tile_id / @cols) * @tile_height
        src = Rectangle.create(src_x, src_y, @tile_width, @tile_height)

        dst_x = x * @tile_width
        dst_y = y * @tile_height
        dst = Rectangle.create(dst_x, dst_y, @tile_width, @tile_height)

        rot = @rot[x][y] % 4
        mirror_x = @mirror_x[x][y] ? -1.0 : 1.0
        mirror_y = @mirror_y[x][y] ? -1.0 : 1.0
        angle = rot * 90

        Raylib.DrawTexturePro(@tileset_texture, src, dst, Vector2.create(0, 0), angle, Raylib::WHITE)
        # Обратите внимание: DrawTexturePro не поддерживает масштабирование для зеркал напрямую, но можно использовать dst.width отрицательное.
        # Однако проще использовать DrawTexturePro с отрицательной шириной для зеркала.
        # В текущем коде мы не учитываем зеркала (mirror_x, mirror_y) – их нужно применить отдельно.
        # Для простоты сначала без зеркал.
      end
    end
  end

  def tile_type_at(x, y)
    return 1 if x < 0 || x >= @width || y < 0 || y >= @height
    tile_id = @tiles[x][y]
    @tile_types[tile_id] || 0
  end

  def passable?(x, y)
    type = tile_type_at(x, y)
    type != 1  # 1 = block
  end
end