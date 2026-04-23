# main.rb
require 'raylib'
require_relative 'lib/database'

# Загрузка библиотеки Raylib
shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

# ===== КОНСТАНТЫ =====
SCREEN_W = 576
SCREEN_H = 480
TILE_SIZE = 48
GRID_W = 12
GRID_H = 10

DIR_DOWN = 2
DIR_LEFT = 4
DIR_RIGHT = 6
DIR_UP = 8

MOVE_SPEED = 0.09
ANIM_SPEED = 12

# ===== ЗАГРУЗКА БАЗЫ ДАННЫХ =====
db = Database.new
puts "База данных загружена: #{db.items.size} предметов"

# ===== КЛАСС ПЕРСОНАЖА =====
class Player
  attr_accessor :x, :y, :direction, :pattern
  attr_accessor :moving, :move_dir, :move_offset
  attr_accessor :anim_frame
  
  def initialize
    @x = 6
    @y = 5
    @direction = DIR_DOWN
    @pattern = 0
    @moving = false
    @move_dir = DIR_DOWN
    @move_offset = 0.0
    @anim_frame = 0
    load_textures
  end
  
  def load_textures
    # Путь к спрайту в папке mapsprites
    img = LoadImage("assets/mapsprites/hero.png")
    img_mirror = ImageCopy(img)
    ImageFlipHorizontal(img_mirror)
    
    @tex_left = LoadTextureFromImage(img)
    @tex_right = LoadTextureFromImage(img_mirror)
    
    SetTextureFilter(@tex_left, TEXTURE_FILTER_POINT)
    SetTextureFilter(@tex_right, TEXTURE_FILTER_POINT)
    
    UnloadImage(img)
    UnloadImage(img_mirror)
  end
  
  def start_move(dir)
    return if @moving
    
    new_x = @x
    new_y = @y
    
    case dir
    when DIR_RIGHT then new_x += 1
    when DIR_LEFT  then new_x -= 1
    when DIR_DOWN  then new_y += 1
    when DIR_UP    then new_y -= 1
    end
    
    return if new_x < 0 || new_x >= GRID_W
    return if new_y < 0 || new_y >= GRID_H
    
    @move_dir = dir
    @direction = dir
    @moving = true
    @move_offset = 0.0
  end
  
  def update
    @anim_frame += 1
    if @anim_frame >= ANIM_SPEED
      @anim_frame = 0
      @pattern = (@pattern + 1) % 2
    end
    
    if @moving
      @move_offset += MOVE_SPEED
      if @move_offset >= 1.0
        case @move_dir
        when DIR_RIGHT then @x += 1
        when DIR_LEFT  then @x -= 1
        when DIR_DOWN  then @y += 1
        when DIR_UP    then @y -= 1
        end
        @moving = false
        @move_offset = 0.0
      end
    end
  end
  
  def draw
    px = @x * TILE_SIZE
    py = @y * TILE_SIZE
    
    if @moving
      case @move_dir
      when DIR_RIGHT then px += @move_offset * TILE_SIZE
      when DIR_LEFT  then px -= @move_offset * TILE_SIZE
      when DIR_DOWN  then py += @move_offset * TILE_SIZE
      when DIR_UP    then py -= @move_offset * TILE_SIZE
      end
    end
    
    texture = (@direction == DIR_RIGHT) ? @tex_right : @tex_left
    
    case @direction
    when DIR_UP    then row = 0
    when DIR_LEFT  then row = 1
    when DIR_RIGHT then row = 1
    when DIR_DOWN  then row = 2
    else row = 2
    end
    
    src = Rectangle.create
    src.x = @pattern * TILE_SIZE
    src.y = row * TILE_SIZE
    src.width = TILE_SIZE
    src.height = TILE_SIZE
    
    dst = Rectangle.create
    dst.x = px
    dst.y = py
    dst.width = TILE_SIZE
    dst.height = TILE_SIZE
    
    DrawTexturePro(texture, src, dst, Vector2.create(0, 0), 0, WHITE)
  end
end

# ===== ГЛАВНЫЙ ЦИКЛ =====
InitWindow(SCREEN_W, SCREEN_H, "SF2 Engine - Ruby + Raylib")
SetTargetFPS(60)

player = Player.new

until WindowShouldClose()
  # Управление
  if !player.moving
    if IsKeyDown(KEY_RIGHT)
      player.start_move(DIR_RIGHT)
    elsif IsKeyDown(KEY_LEFT)
      player.start_move(DIR_LEFT)
    elsif IsKeyDown(KEY_DOWN)
      player.start_move(DIR_DOWN)
    elsif IsKeyDown(KEY_UP)
      player.start_move(DIR_UP)
    end
  end
  
  player.update
  
  # Отрисовка
  BeginDrawing()
  ClearBackground(RAYWHITE)
  
  # Сетка
  (0..GRID_W).each do |x|
    DrawLine(x * TILE_SIZE, 0, x * TILE_SIZE, SCREEN_H, LIGHTGRAY)
  end
  (0..GRID_H).each do |y|
    DrawLine(0, y * TILE_SIZE, SCREEN_W, y * TILE_SIZE, LIGHTGRAY)
  end
  
  player.draw
  
  DrawText("FPS: " + GetFPS().to_s, SCREEN_W - 100, 10, 20, DARKGRAY)
  DrawText("← → ↑ ↓ to move", 10, SCREEN_H - 20, 20, DARKGRAY)
  
  EndDrawing()
end

CloseWindow()