# Marbluino Now

## Multiplayer variant of Marbluino game, using ESP-Now

[![Marbluino Now](http://img.youtube.com/vi/-h-PS4ID7qo/0.jpg)](http://www.youtube.com/watch?v=-h-PS4ID7qo "MarbluinoNow: ESP8266 multiplayer Marbluino game")

This game expands on [Marbluino](https://github.com/jablan/marbluino) game for Arduino and ESP8266, by introducing wireless multiplayer feature. It requires ESP8266 and relies on [ESP-Now](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html) protocol for communicating between nodes. That means that no access point is needed, the devices communicate directly among themselves.

With a single device, it behaves the same as normal Marbluino game. As soon as another device appears, it turns to multiplayer mode: all players see each other's marbles, but control only their own. They try to get to the flags (triangles) faster than others, collecting points and avoiding obstacles (squares). Other players are represented by hollow circles.
