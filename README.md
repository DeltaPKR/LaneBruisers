# 2D RTS Base Defence Game

---

## Description

Lane Bruisers is a 2D RTS 1vs1 base defence game written in c++. At the start of the match, each player is given specific amount of coins which they can spend on spawning units. Players have only 5 spawnable units with different cost and stats. The first player to destroy another player's base wins the match.

## Logic

 - server.cpp - epoll based TCP server, game loop and authoritative simulation.
 - client.cpp - SFML window, input and rendering.

 - All the gameplay is computed on the server, client only sends SPAWN commands and renders the state it receives.

## Unit Stats

| Key | Cost | HP | DMG | Spawn CD |
|-----|------|----|-----|----------|
|  1  |  5   | 20 |  2  |   1.5s   |
|  2  |  7   | 30 |  4  |   2.0s   |
|  3  |  10  | 40 |  6  |   2.5s   |
|  4  |  12  | 50 |  8  |   3.0s   |
|  5  |  15  | 60 |  10 |   3.5s   |


##### Dependencies

 - Linux, a C++17 compiler (or higher), SFML 2.x and arial.ttf in the working directory (for the text)


