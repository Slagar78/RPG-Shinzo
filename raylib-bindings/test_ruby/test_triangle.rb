require 'raylib'

# Загружаем DLL, как в игре
shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

InitWindow(400, 300, "Triangle Test")
SetTargetFPS(60)

until WindowShouldClose()
  BeginDrawing()
  ClearBackground(DARKGRAY)

  # Верхний треугольник (красный, остриём вверх) - проверено, работает
  DrawTriangle(
    Vector2.create(200, 80),
    Vector2.create(160, 160),
    Vector2.create(240, 160),
    RED
  )

  # Нижний треугольник (остриём вниз) — против часовой стрелки
  DrawTriangle(
    Vector2.create(160, 140),  # левый верх
    Vector2.create(200, 220),  # нижняя вершина
    Vector2.create(240, 140),  # правый верх
    RED
  )

  DrawText("UP", 180, 170, 20, WHITE)
  DrawText("DOWN (rect)", 150, 230, 20, WHITE)

  EndDrawing()
end

CloseWindow()