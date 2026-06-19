// Sample game source with intentional anti-patterns for PerfBuddy to flag.
#include <iostream>
#include <vector>
#include <string>
#include "entity.h"

// std::endl in a loop -> flush every iteration (pb_code flags this)
static void log_spam() {
  for (int i = 0; i < 1000; ++i) {
    std::cout << "frame " << i << std::endl;  // should be '\n'
  }
}

// raw new/delete -> manual memory management (pb_code flags this)
Entity* spawn(int hp) {
  Entity* e = new Entity();  // TODO: use std::make_unique
  e->hp = hp;
  return e;
}

void despawn(Entity* e) {
  delete e;
}

int main() {
  std::vector<Entity*> ents;
  for (int i = 0; i < 10; ++i) ents.push_back(spawn(100));
  // FIXME: leaks every entity except via despawn below
  for (auto* e : ents) {
    if (e) {
      if (e->hp > 0) {
        if (e->hp < 200) {
          if (e->hp != 50) {
            if (e->hp % 2 == 0) {
              despawn(e);  // deep nesting -> pb_code flags this
            }
          }
        }
      }
    }
  }
  log_spam();
  return 0;
}
