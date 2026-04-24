# lib/map.rb
require 'json'

class GameMap
  attr_reader :width, :height, :tile_size, :tiles
  
  def initialize
    load_map("data/map.json")
  end
  
  def load_map(path)
    if File.exist?(path)
      data = JSON.parse(File.read(path))
      @width = data["width"]
      @height = data["height"]
      @tile_size = data["tile_size"]
      @tiles = data["tiles"]
    else
      # Карта по умолчанию
      @width = 20
      @height = 15
      @tile_size = 48
      @tiles = Array.new(@width) { Array.new(@height, 0) }
    end
  end
  
  def draw
    (0...@width).each do |x|
      (0...@height).each do |y|
        tile_id = @tiles[x][y]
        color = case tile_id
        when 1 then Gosu::Color::GREEN
        when 2 then Gosu::Color::BROWN
        when 3 then Gosu::Color::BLUE
        when 4 then Gosu::Color::RED
        else Gosu::Color::GRAY
        end
        Gosu.draw_rect(x * @tile_size, y * @tile_size, @tile_size, @tile_size, color, 0)
      end
    end
  end
end