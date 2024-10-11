#include <iostream>
#include <string_view>

std::string_view preclude_syi = 
R"(
extern void putint(int number);
extern void putfloat(float number);
extern int getint();
extern float getfloat();
)";