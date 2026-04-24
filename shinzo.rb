# shinzo.rb
require 'raylib'
require_relative 'lib/database'
require_relative 'lib/player'
require_relative 'lib/ui'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

class Shinzo
  def initialize
    InitWindow(576, 480, "RPG Shinzo")
    SetTargetFPS(60)
    
    @db = Database.new
    @player = Player.new
    @menu = BottomMenu.new
    @status_overlay = StatusOverlay.new
    @game_state = :playing
  end
  
  def run
    until WindowShouldClose()
      handle_input
      update
      draw
    end
    CloseWindow()
  end
  
  def handle_input
    case @game_state
    when :playing
      if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        @game_state = :menu
        @menu.open
      else
        @player.handle_input
      end
    when :menu
      if IsKeyPressed(KEY_S)
        @game_state = :playing
        @menu.close
      elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        @game_state = :status
        @status_overlay.open(@player)
      else
        @menu.handle_input
      end
    when :status
      if IsKeyPressed(KEY_S)
        @game_state = :playing
        @status_overlay.force_close
      elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        @status_overlay.handle_input
      else
        @status_overlay.handle_input
      end
    end
  end
  
  def update
    # АНИМАЦИЯ ИГРОКА ВСЕГДА
    @player.update_animation
    
    # Движение только в игре
    if @game_state == :playing
      @player.update_movement
    end
    
    @menu.update if @game_state == :menu
    @status_overlay.update if @game_state == :status
  end
  
  def draw
    BeginDrawing()
    ClearBackground(RAYWHITE)
    
    draw_grid
    @player.draw
    
    case @game_state
    when :menu
      @menu.draw
    when :status
      @status_overlay.draw
    end
    
    DrawText("FPS: #{GetFPS()}", 576 - 100, 10, 20, DARKGRAY)
    EndDrawing()
  end
  
  def draw_grid
    (0..12).each do |x|
      DrawLine(x * 48, 0, x * 48, 480, LIGHTGRAY)
    end
    (0..10).each do |y|
      DrawLine(0, y * 48, 576, y * 48, LIGHTGRAY)
    end
  end
end