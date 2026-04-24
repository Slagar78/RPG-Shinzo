require 'gosu'

class TestWindow < Gosu::Window
  def initialize
    super(640, 480)
    self.caption = "Gosu Test"
  end

  def draw
    # Заливаем фон красным
    Gosu.draw_rect(0, 0, width, height, Gosu::Color::RED, 0)
    # Рисуем текст
    font = Gosu::Font.new(30)
    font.draw_text("Gosu works!", 10, 10, 1, 1, 1, Gosu::Color::WHITE)
  end
end

TestWindow.new.show