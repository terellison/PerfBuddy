#pragma once
// Sample header with a deliberate anti-pattern: `using namespace std` in a
// header leaks into every translation unit that includes it (pb_code flags it).
#include <string>
#include <vector>
#include <map>
#include <iostream>

using namespace std;  // <- pb_code: using_namespace_in_header

struct Entity {
  int hp = 100;
  string name;
  vector<int> inventory;
};
