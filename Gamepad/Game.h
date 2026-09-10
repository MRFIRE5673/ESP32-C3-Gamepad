#pragma once

class Game {
public:
  virtual void init() = 0;
  virtual void update() = 0;
  virtual void draw() = 0;
  virtual ~Game() {}
};

extern Game* currentGame;
