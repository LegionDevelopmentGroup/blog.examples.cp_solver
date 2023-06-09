require "faker"
require "json"

num_items = 50000
num_prod_type_range = 5..10
num_man_type_range = 5..10

value_range = 1..100
weight_range = 1..50
volume_range = 1..25

num_prod_types = rand(num_prod_type_range)
num_man_types = rand(num_man_type_range)

prod_type_list = [] of String
man_type_list = [] of String

num_prod_types.times do |x|
  prod_type_list << Faker::Commerce.department
end

num_man_types.times do |x|
  man_type_list << Faker::Company.name
end

class Item
  include JSON::Serializable
  property value : Int32, weight : Int32, volume : Int32, product_type : String, manufacturer : String

  def initialize(@value, @weight, @volume, @product_type, @manufacturer)
  end
end

items = Array(Item).new

num_items.times do |x|
  items << Item.new(
    rand(value_range),
    rand(weight_range),
    rand(volume_range),
    prod_type_list[rand(num_prod_types)],
    man_type_list[rand(num_man_types)])
end

puts items.to_json
