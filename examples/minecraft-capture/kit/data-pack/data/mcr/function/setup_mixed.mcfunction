# ONLY run in a dedicated disposable capture world; clears the small capture region.
gamemode spectator @s
effect clear @s
gamerule doDaylightCycle false
gamerule doWeatherCycle false
gamerule doMobSpawning false
time set noon
weather clear
fill -8 76 -4 9 85 18 minecraft:air
setblock 1 80 0 minecraft:oak_slab[type=bottom,waterlogged=false]
setblock 1 80 1 minecraft:stone_slab[type=top,waterlogged=false]
setblock 1 81 0 minecraft:stone_slab[type=top,waterlogged=false]
setblock 0 80 1 minecraft:oak_slab[type=bottom,waterlogged=false]
setblock 0 81 0 minecraft:oak_slab[type=bottom,waterlogged=false]
setblock 0 81 1 minecraft:stone_slab[type=top,waterlogged=false]
function mcr:camera_on
