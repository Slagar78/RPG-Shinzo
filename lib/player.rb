# lib/player.rb
require 'raylib'

# Константы
TILE_SIZE = 48
DEFAULT_GRID_W = 12
DEFAULT_GRID_H = 10

DIR_DOWN = 2
DIR_LEFT = 4
DIR_RIGHT = 6
DIR_UP = 8

MOVE_SPEED = 0.09
ANIM_SPEED = 12

class Player
  attr_accessor :x, :y, :direction, :pattern
  attr_accessor :moving, :move_dir, :move_offset
  attr_accessor :anim_frame
  attr_accessor :can_move
  attr_accessor :map

  def initialize(map = nil)
    @map = map
    if @map
      @x = @map.width / 2
      @y = @map.height / 2
    else
      @x = 6
      @y = 5
    end
    @direction = DIR_DOWN
    @pattern = 0
    @moving = false
    @move_dir = DIR_DOWN
    @move_offset = 0.0
    @anim_frame = 0
    @can_move = true
    load_textures
  end

  def load_textures
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

  def handle_input
    if @can_move && !@moving
      if IsKeyDown(KEY_RIGHT)
        start_move(DIR_RIGHT)
      elsif IsKeyDown(KEY_LEFT)
        start_move(DIR_LEFT)
      elsif IsKeyDown(KEY_DOWN)
        start_move(DIR_DOWN)
      elsif IsKeyDown(KEY_UP)
        start_move(DIR_UP)
      end
    end
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

    if @map
      # границы карты
      return if new_x < 0 || new_x >= @map.width
      return if new_y < 0 || new_y >= @map.height
      return unless @map.passable?(new_x, new_y)
    else
      return if new_x < 0 || new_x >= DEFAULT_GRID_W
      return if new_y < 0 || new_y >= DEFAULT_GRID_H
    end

    @move_dir = dir
    @direction = dir
    @moving = true
    @move_offset = 0.0
  end

  def update_animation
    @anim_frame += 1
    if @anim_frame >= ANIM_SPEED
      @anim_frame = 0
      @pattern = (@pattern + 1) % 2
    end
  end

  def update_movement
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

  def update
    update_animation
    update_movement
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