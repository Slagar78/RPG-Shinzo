# database.rb
require 'json'
require 'ostruct'

class Database
  attr_reader :items
  
  def initialize
    @items = []
    load_data
  end
  
  def load_data
    if File.exist?("data/actors/items.json")
      data = JSON.parse(File.read("data/actors/items.json"))
      @items = data["items"].map { |h| OpenStruct.new(h) }
      puts "Загружено предметов: #{@items.size}"
    else
      puts "Файл data/items.json не найден!"
    end
  end
  
  def item(id)
    @items.find { |i| i.id == id }
  end
  
  def find_by_name(name)
    @items.find { |i| i.name == name }
  end
end