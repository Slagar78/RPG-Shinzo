require 'raylib'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

InitWindow(800, 600, "Test")

until WindowShouldClose()
  BeginDrawing()
  ClearBackground(RAYWHITE)
  DrawText("Hello", 100, 100, 20, BLACK)
  EndDrawing()
end

CloseWindow()